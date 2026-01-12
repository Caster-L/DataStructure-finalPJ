#include "../include/Controller.h"
#include <nlohmann/json.hpp>
#include <random>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

GameController::GameController(std::shared_ptr<GameModel> model,
                               GameMode mode,
                               PlayerType team0,
                               PlayerType team1)
    : model(model), running(false), rng(std::random_device{}()),
      gameMode(mode), team0Type(team0), team1Type(team1), currentTurn(0),
      team0HealThisTurn(0), team1HealThisTurn(0) {
    // 为两个队伍创建独立的AI控制器
    aiControllerTeam0 = std::make_unique<AIController>(rng);
    aiControllerTeam1 = std::make_unique<AIController>(rng);
    
    // 如果使用Python AI，初始化Python代理
    if (team0Type == PlayerType::AI_PYTHON || team1Type == PlayerType::AI_PYTHON) {
        pythonAgent = std::make_unique<PythonAgent>();
        pythonAgent->initialize("python/infer.py");
    }
    
    // 如果是训练模式，初始化日志记录器
    if (gameMode == GameMode::TRAINING) {
        trainingLogger = std::make_unique<TrainingLogger>();
        trainingLogger->setModel(model);
        trainingLogger->startGame(gameMode, team0Type, team1Type);
    }
}

GameController::~GameController() {
    stop();
    
    // 结束训练日志
    if (trainingLogger && gameMode == GameMode::TRAINING) {
        trainingLogger->endGame(static_cast<int>(model->getWinner()));
    }
    
    // 关闭Python代理
    if (pythonAgent) {
        pythonAgent->shutdown();
    }
}

void GameController::start() {
    if (running.load()) return;
    
    running.store(true);
    model->initialize();
    
    // 启动游戏循环线程
    workerThreads.emplace_back([this]() { gameLoop(); });
}

void GameController::stop() {
    running.store(false);
    
    // 等待所有线程结束
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear();
}

void GameController::gameLoop() {
    currentTurn = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    while (running.load() && !model->isGameOver()) {
        auto turnStart = std::chrono::steady_clock::now();
        
        // 处理一个回合
        processTurn();
        
        // 训练模式：跳过渲染和时间等待，直接下一回合
        if (gameMode == GameMode::TRAINING) {
            currentTurn++;
            model->incrementTurn();
            
            // 每1000回合报告速度
            if (currentTurn % 1000 == 0) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
                double turnsPerSec = (elapsed > 0) ? (currentTurn * 1000.0 / elapsed) : 0;
                std::cout << "[Training] Turn " << currentTurn 
                          << " | Speed: " << static_cast<int>(turnsPerSec) << " turns/sec" << std::endl;
            }
            continue;
        }
        
        // 正常模式/观战模式：控制回合间隔（1秒）
        auto turnEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(turnEnd - turnStart);
        auto sleepTime = std::chrono::milliseconds(TURN_DURATION_MS) - elapsed;
        
        if (sleepTime.count() > 0) {
            std::this_thread::sleep_for(sleepTime);
        }
        
        currentTurn++;
        model->incrementTurn();
    }
    
    // 游戏结束，保存日志
    if (trainingLogger) {
        trainingLogger->endGame(static_cast<int>(model->getWinner()));
    }
}

void GameController::processTurn() {
    // 1. 生成能量
    generateEnergy();
    
    // 2. 更新队友共享视野
    model->updateSharedVision();
    
    // 3. 准备记录本回合的状态和动作（用于训练日志）
    std::string team0ActionJson = "{\"action_type\": 0, \"base_id\": -1, \"unit_type\": -1}";
    std::string team1ActionJson = "{\"action_type\": 0, \"base_id\": -1, \"unit_type\": -1}";
    
    // 在决策前获取状态（训练模式下需要）
    // 注意：现在训练 Team 1（红色），所以获取 Team 1 的视角
    std::string stateJson;
    if (trainingLogger && gameMode == GameMode::TRAINING) {
        stateJson = getStateJson(1);  // 获取 Team 1 的决策前状态
    }
    
    // 4. Team 0 决策（蓝色 - AI规则/Python AI，允许每回合多次购买）
    const int MAX_PURCHASES_PER_TURN = 3;  // 每回合最多购买3个兵
    
    if (team0Type == PlayerType::AI_PYTHON && pythonAgent && pythonAgent->isInitialized()) {
        // Python AI决策 - 循环调用直到无法购买或达到上限
        for (int i = 0; i < MAX_PURCHASES_PER_TURN; ++i) {
            std::string team0StateJson = getStateJson(0);
            std::string action = pythonAgent->getAction(team0StateJson);
            
            // 检查是否是wait动作（action_type == 0）
            if (action.find("\"action_type\": 0") != std::string::npos) {
                break;  // 模型选择等待，停止购买
            }
            
            bool success = parseAndExecuteAction(0, action);
            if (success) {
                team0ActionJson = action;  // 记录最后一次成功的购买
            } else {
                break;  // 购买失败（能量不足或位置被占），停止购买
            }
        }
    } else if (team0Type == PlayerType::AI_RULE_BASED) {
        // 规则AI决策 - 循环调用直到无法购买或达到上限
        for (int i = 0; i < MAX_PURCHASES_PER_TURN; ++i) {
            std::string action = aiControllerTeam0->tryPurchaseOnce(model, this, currentTurn, Team::TEAM_A);

            // 检查是否是wait动作
            if (action.find("\"action_type\": 0") != std::string::npos) {
                break;  // 无法购买，停止
            }

            // 调用parseAndExecuteAction扣能量
            bool success = parseAndExecuteAction(0, action);
            if (success) {
                team0ActionJson = action;  // 记录最后一次成功的购买
            } else {
                break;  // 购买失败，停止购买
            }
        }
    }
    // HUMAN类型不自动决策，由View层调用purchaseSoldier
    
    // 4. Team 1 决策（红色 - 主控方：人类/Python AI，允许每回合多次购买）
    if (team1Type == PlayerType::AI_PYTHON && pythonAgent && pythonAgent->isInitialized()) {
        // Python AI决策 - 循环调用直到无法购买或达到上限
        for (int i = 0; i < MAX_PURCHASES_PER_TURN; ++i) {
            std::string currentStateJson = getStateJson(1);
            std::string action = pythonAgent->getAction(currentStateJson);
            
            // 检查是否是wait动作
            if (action.find("\"action_type\": 0") != std::string::npos) {
                break;  // 模型选择等待，停止购买
            }
            
            bool success = parseAndExecuteAction(1, action);
            if (success) {
                team1ActionJson = action;  // 记录最后一次成功的购买
                // 训练模式下，只记录第一次购买动作（保持训练数据格式不变）
                if (i == 0 && trainingLogger && gameMode == GameMode::TRAINING) {
                    stateJson = currentStateJson;  // 使用第一次购买前的状态
                }
            } else {
                break;  // 购买失败，停止购买
            }
        }
    } else if (team1Type == PlayerType::AI_RULE_BASED) {
        // 规则AI决策 - 循环调用直到无法购买或达到上限
        for (int i = 0; i < MAX_PURCHASES_PER_TURN; ++i) {
            std::string action = aiControllerTeam1->tryPurchaseOnce(model, this, currentTurn, Team::TEAM_B);

            // 检查是否是wait动作
            if (action.find("\"action_type\": 0") != std::string::npos) {
                break;  // 无法购买，停止
            }

            // 调用parseAndExecuteAction扣能量
            bool success = parseAndExecuteAction(1, action);
            if (success) {
                team1ActionJson = action;  // 记录最后一次成功的购买
            } else {
                break;  // 购买失败，停止购买
            }
        }
    }
    
    // 5. 处理所有士兵的行为（移动）
    auto soldiers = model->getSoldiers();
    for (auto& soldier : soldiers) {
        if (soldier->isAlive()) {
            processSoldierBehavior(soldier);
        }
    }
    
    // 6. 处理战斗，获取治疗统计数据
    std::vector<GameEvent> combatEvents;
    auto healStats = CombatSystem::processCombat(model, combatEvents, currentTurn);
    team0HealThisTurn = healStats[0];
    team1HealThisTurn = healStats[1];
    
    // 将战斗事件添加到日志中
    if (trainingLogger && gameMode == GameMode::TRAINING) {
        for (const auto& evt : combatEvents) {
            trainingLogger->addEvent(evt);
        }
    }
    
    // 7. 清理死亡士兵
    cleanupDeadSoldiers();
    
    // 8. 检查游戏是否结束
    checkGameOver();
    
    // 9. 记录训练日志（记录 Team 1 红色的状态和动作）
    if (trainingLogger && gameMode == GameMode::TRAINING) {
        // stateJson 是 Team 1 的决策前状态
        trainingLogger->recordTurn(currentTurn, stateJson, team0ActionJson, team1ActionJson);
    }
    
    // 10. 每10回合输出一次状态
    int turn = model->getTurnCount();
    if (turn % 10 == 0) {
        auto currentSoldiers = model->getSoldiers();
        int teamA = 0, teamB = 0;
        for (const auto& s : currentSoldiers) {
            if (s->isAlive()) {
                if (s->getTeam() == Team::TEAM_A) teamA++;
                else teamB++;
            }
        }
        
        // 计算基地总HP
        int baseAHp = 0, baseBHp = 0;
        for (const auto& base : model->getBasesTeamA()) {
            baseAHp += base->getHp();
        }
        for (const auto& base : model->getBasesTeamB()) {
            baseBHp += base->getHp();
        }
        
        std::cout << "Turn " << turn << ": Team A=" << teamA 
                  << " soldiers, Team B=" << teamB << " soldiers. "
                  << "Base A HP=" << baseAHp
                  << ", Base B HP=" << baseBHp 
                  << " | Energy A=" << model->getEnergy(Team::TEAM_A)
                  << ", Energy B=" << model->getEnergy(Team::TEAM_B) << std::endl;
    }
}

void GameController::generateEnergy() {
    model->addEnergy(Team::TEAM_A, ENERGY_PER_TURN);
    model->addEnergy(Team::TEAM_B, ENERGY_PER_TURN);
}



bool GameController::purchaseSoldier(Team team, SoldierType type, const Position& basePos) {
    int cost = CombatSystem::getSoldierCost(type);
    
    // 检查并消费能量
    if (!model->spendEnergy(team, cost)) {
        return false;  // 能量不足
    }
    
    // 寻找生成位置
    Position spawnPos = findSpawnPosition(team, basePos);
    if (!model->getMap()->isWalkable(spawnPos)) {
        // 位置不可用，退还能量
        model->addEnergy(team, cost);
        return false;
    }
    
    // 创建士兵
    auto soldier = std::make_shared<Soldier>(spawnPos, type, team);
    model->addSoldier(soldier);
    return true;
}

Position GameController::findSpawnPosition(Team team, const Position& basePos) {
    // 在基地周围选择位置，优先选择拥挤度低的位置
    std::vector<Position> possiblePositions;
    
    // 扩大搜索范围到3格
    for (int dx = -3; dx <= 3; dx++) {
        for (int dy = -3; dy <= 3; dy++) {
            if (dx == 0 && dy == 0) continue;
            Position spawnPos(basePos.x + dx, basePos.y + dy);
            
            if (model->getMap()->isWalkable(spawnPos)) {
                // 检查是否有其他士兵占据
                bool occupied = aiControllerTeam0->isPositionOccupied(model, spawnPos, nullptr);
                if (!occupied) {
                    possiblePositions.push_back(spawnPos);
                }
            }
        }
    }
    
    if (possiblePositions.empty()) {
        return basePos;  // 如果找不到合适位置，返回基地位置
    }
    
    // 选择拥挤度最低的位置
    Position bestPos = possiblePositions[0];
    int minCrowdedness = aiControllerTeam0->getCrowdednessAtPosition(model, bestPos, team, 2);
    
    for (const auto& pos : possiblePositions) {
        int crowdedness = aiControllerTeam0->getCrowdednessAtPosition(model, pos, team, 2);
        if (crowdedness < minCrowdedness) {
            minCrowdedness = crowdedness;
            bestPos = pos;
        }
    }
    
    return bestPos;
}

void GameController::processSoldierBehavior(std::shared_ptr<Soldier> soldier) {
    if (!soldier->isAlive()) return;
    
    Position currentPos = soldier->getPosition();
    
    // 检测拥堵情况
    int nearbyAllies = aiControllerTeam0->countNearbyAllies(model, soldier, 2);
    bool isCrowded = nearbyAllies >= 6;  // 2格范围内有6+个队友视为拥堵
    bool isVeryCrowded = nearbyAllies >= 8;  // 极度拥挤
    
    // 如果极度拥挤且不在战斗中，跳过移动等待疏散
    if (isVeryCrowded) {
        auto soldiers = model->getSoldiers();
        bool inCombat = false;
        for (const auto& enemy : soldiers) {
            if (!enemy->isAlive() || enemy->getTeam() == soldier->getTeam()) continue;
            int dist = currentPos.chebyshevDistanceTo(enemy->getPosition());
            if (dist <= soldier->getAttackRange() + 2) {  // 接近战斗范围
                inCombat = true;
                break;
            }
        }
        if (!inCombat) {
            return;  // 不在战斗中，等待其他士兵先走
        }
    }
    
    // 弓箭手特殊AI：战术决策
    if (soldier->getType() == SoldierType::ARCHER) {
        // 1. 检查攻击范围内是否有敌人（可以原地攻击）
        bool hasTargetInRange = false;
        auto soldiers = model->getSoldiers();
        for (const auto& target : soldiers) {
            if (target->isAlive() && target->getTeam() != soldier->getTeam() &&
                soldier->canAttack(target->getPosition())) {
                hasTargetInRange = true;
                break;
            }
        }
        
        // 如果攻击范围内有敌人，停留原地射击（不移动）
        if (hasTargetInRange) {
            return;
        }
        
        // 2. 检查是否有近战敌人靠近（距离<=2格），需要后撤
        std::shared_ptr<Soldier> nearestMelee = nullptr;
        int minDistance = 999;
        for (const auto& enemy : soldiers) {
            if (!enemy->isAlive() || enemy->getTeam() == soldier->getTeam()) continue;
            
            // 判断是否是近战单位（攻击范围<=1）
            if (enemy->getAttackRange() <= 1) {
                int dist = abs(enemy->getPosition().x - currentPos.x) + 
                          abs(enemy->getPosition().y - currentPos.y);
                if (dist <= 2 && dist < minDistance) {
                    minDistance = dist;
                    nearestMelee = enemy;
                }
            }
        }
        
        // 如果有近战单位接近，后撤（远离近战敌人）
        if (nearestMelee) {
            Position enemyPos = nearestMelee->getPosition();
            std::vector<Position> retreatCandidates = aiControllerTeam0->getRetreatPositions(currentPos, enemyPos);
            
            for (const auto& newPos : retreatCandidates) {
                if (model->getMap()->isWalkable(newPos) && !aiControllerTeam0->isPositionOccupied(model, newPos, soldier)) {
                    soldier->setPosition(newPos);
                    return;  // 后撤成功，结束移动
                }
            }
        }
    }
    
    // 根据士兵速度移动多次（骑兵=4格，步兵/弓箭手=1格）
    int moveSpeed = soldier->getMoveSpeed();
    
    for (int step = 0; step < moveSpeed; ++step) {
        Position currentPos = soldier->getPosition();
        
        // 如果拥堵，优先尝试分散（向外移动）
        if (isCrowded && step == 0) {
            std::vector<Position> dispersePositions;
            Position basePos = (soldier->getTeam() == Team::TEAM_A) ? 
                               model->getBasesTeamA()[0]->getPosition() : 
                               model->getBasesTeamB()[0]->getPosition();
            
            // 远离基地方向
            int baseDx = currentPos.x - basePos.x;
            int baseDy = currentPos.y - basePos.y;
            
            if (abs(baseDx) > 0 || abs(baseDy) > 0) {
                int normDx = (baseDx > 0) ? 1 : (baseDx < 0 ? -1 : 0);
                int normDy = (baseDy > 0) ? 1 : (baseDy < 0 ? -1 : 0);
                
                dispersePositions.push_back(Position(currentPos.x + normDx, currentPos.y + normDy));
                dispersePositions.push_back(Position(currentPos.x + normDx, currentPos.y));
                dispersePositions.push_back(Position(currentPos.x, currentPos.y + normDy));
            }
            
            for (const auto& newPos : dispersePositions) {
                if (model->getMap()->isWalkable(newPos) && !aiControllerTeam0->isPositionOccupied(model, newPos, soldier)) {
                    soldier->setPosition(newPos);
                    continue;  // 分散成功，继续下一步
                }
            }
        }
        
        // AI决策：获取移动候选位置列表
        std::vector<Position> candidates = aiControllerTeam0->getMoveCandidates(model, soldier);
        
        // 如果返回空列表，表示AI建议不移动（当前位置更好）
        if (candidates.empty()) {
            break;  // 停止移动
        }
        
        bool moved = false;
        
        // 尝试所有候选位置
        for (const auto& newPos : candidates) {
            if (model->getMap()->isWalkable(newPos) && !aiControllerTeam0->isPositionOccupied(model, newPos, soldier)) {
                soldier->setPosition(newPos);
                moved = true;
                break;  // 成功移动，继续下一步
            }
        }
        
        if (moved) continue;
        
        // 如果所有位置都被占用，尝试随机移动到周围任意空位
        std::vector<Position> neighbors = {
            Position(currentPos.x + 1, currentPos.y),
            Position(currentPos.x - 1, currentPos.y),
            Position(currentPos.x, currentPos.y + 1),
            Position(currentPos.x, currentPos.y - 1),
            Position(currentPos.x + 1, currentPos.y + 1),
            Position(currentPos.x + 1, currentPos.y - 1),
            Position(currentPos.x - 1, currentPos.y + 1),
            Position(currentPos.x - 1, currentPos.y - 1)
        };
        
        std::shuffle(neighbors.begin(), neighbors.end(), rng);
        
        for (const auto& pos : neighbors) {
            if (model->getMap()->isWalkable(pos) && !aiControllerTeam0->isPositionOccupied(model, pos, soldier)) {
                soldier->setPosition(pos);
                moved = true;
                break;
            }
        }
        
        // 如果仍然无法移动，尝试更远的位置（2格范围）
        if (!moved) {
            std::vector<Position> farPositions;
            for (int i = -2; i <= 2; i++) {
                for (int j = -2; j <= 2; j++) {
                    if (abs(i) + abs(j) <= 2 && (i != 0 || j != 0)) {
                        farPositions.push_back(Position(currentPos.x + i, currentPos.y + j));
                    }
                }
            }
            std::shuffle(farPositions.begin(), farPositions.end(), rng);
            
            for (const auto& pos : farPositions) {
                if (model->getMap()->isWalkable(pos) && !aiControllerTeam0->isPositionOccupied(model, pos, soldier)) {
                    soldier->setPosition(pos);
                    moved = true;
                    break;
                }
            }
        }
        
        // 如果本步无法移动，提前结束（避免原地循环）
        if (!moved) break;
    }
}

void GameController::cleanupDeadSoldiers() {
    auto soldiers = model->getSoldiers();
    
    // 收集死亡士兵
    std::vector<std::shared_ptr<Soldier>> deadSoldiers;
    for (const auto& soldier : soldiers) {
        if (!soldier->isAlive()) {
            deadSoldiers.push_back(soldier);
        }
    }
    
    // 删除死亡士兵
    for (const auto& dead : deadSoldiers) {
        model->removeSoldier(dead);
    }
}

void GameController::checkGameOver() {
    // 检查是否达到最大回合数
    if (currentTurn >= MAX_TURNS) {
        // 达到最大回合数，比较双方基地总血量
        int teamAHp = 0, teamBHp = 0;
        for (const auto& base : model->getBasesTeamA()) {
            teamAHp += base->getHp();
        }
        for (const auto& base : model->getBasesTeamB()) {
            teamBHp += base->getHp();
        }
        
        // 血量多的一方获胜，相同则Team B获胜
        if (teamAHp > teamBHp) {
            model->setGameOver(Team::TEAM_A);
            if (trainingLogger) trainingLogger->addEvent(GameEvent(EventType::GAME_OVER, 0, currentTurn, "Time Limit Reached - Team A Wins"));
        } else {
            model->setGameOver(Team::TEAM_B);
            if (trainingLogger) trainingLogger->addEvent(GameEvent(EventType::GAME_OVER, 1, currentTurn, "Time Limit Reached - Team B Wins"));
        }
        
        std::cout << "Game ended: MAX_TURNS reached (" << MAX_TURNS << "). "
                  << "Team A HP=" << teamAHp << ", Team B HP=" << teamBHp << std::endl;
        return;
    }
    
    // 检查Team A的所有基地是否都被摧毁
    bool teamAAlive = false;
    for (const auto& base : model->getBasesTeamA()) {
        if (base->isAlive()) {
            teamAAlive = true;
            break;
        }
    }
    
    // 检查Team B的所有基地是否都被摧毁
    bool teamBAlive = false;
    for (const auto& base : model->getBasesTeamB()) {
        if (base->isAlive()) {
            teamBAlive = true;
            break;
        }
    }
    
    if (!teamAAlive) {
        model->setGameOver(Team::TEAM_B);
        if (trainingLogger) trainingLogger->addEvent(GameEvent(EventType::GAME_OVER, 1, currentTurn, "Domination - Team B Wins"));
    } else if (!teamBAlive) {
        model->setGameOver(Team::TEAM_A);
        if (trainingLogger) trainingLogger->addEvent(GameEvent(EventType::GAME_OVER, 0, currentTurn, "Domination - Team A Wins"));
    }
}

// ==================== 新增：AI相关功能实现 ====================

std::string GameController::getStateJson(int myTeam) {
    json state;
    
    // 基础特征（8维）
    state["turn"] = currentTurn;
    state["my_team"] = myTeam;
    // 修复：使用正确的能量值（从 Model::getEnergy 获取，而不是 teams[].energy）
    state["my_energy"] = model->getEnergy(myTeam == 0 ? Team::TEAM_A : Team::TEAM_B);
    state["enemy_energy"] = model->getEnergy(myTeam == 0 ? Team::TEAM_B : Team::TEAM_A);
    
    // 统计基地HP和数量
    int myTotalBaseHP = 0, enemyTotalBaseHP = 0;
    int myBaseCount = 0, enemyBaseCount = 0;
    for (const auto& base : model->bases) {
        if (static_cast<int>(base->getTeam()) == myTeam) {
            myTotalBaseHP += base->getHp();
            myBaseCount++;
        } else {
            enemyTotalBaseHP += base->getHp();
            enemyBaseCount++;
        }
    }
    state["my_total_base_hp"] = myTotalBaseHP;
    state["enemy_total_base_hp"] = enemyTotalBaseHP;
    state["my_base_count"] = myBaseCount;
    state["enemy_base_count"] = enemyBaseCount;
    
    // 治疗量统计（本回合）
    state["my_heal_done"] = (myTeam == 0) ? team0HealThisTurn : team1HealThisTurn;
    state["enemy_heal_done"] = (myTeam == 0) ? team1HealThisTurn : team0HealThisTurn;
    
    // 统计士兵数量
    int mySoldierCount = 0, enemySoldierCount = 0;
    for (const auto& soldier : model->soldiers) {
        if (static_cast<int>(soldier->getTeam()) == myTeam) {
            mySoldierCount++;
        } else {
            enemySoldierCount++;
        }
    }
    state["my_soldier_count"] = mySoldierCount;
    state["enemy_soldier_count"] = enemySoldierCount;
    
    // 我方基地（最多5个，按HP降序）
    state["my_bases"] = json::array();
    std::vector<std::shared_ptr<Base>> myBases;
    for (const auto& base : model->bases) {
        if (static_cast<int>(base->getTeam()) == myTeam) {
            myBases.push_back(base);
        }
    }
    std::sort(myBases.begin(), myBases.end(),
              [](const auto& a, const auto& b) { return a->getHp() > b->getHp(); });
    
    for (size_t i = 0; i < std::min(myBases.size(), size_t(5)); i++) {
        int nearbyAllies = 0, nearbyEnemies = 0;
        countNearbySoldiers(myBases[i]->getPosition(), nearbyAllies, nearbyEnemies, myTeam);
        
        state["my_bases"].push_back({
            {"hp", myBases[i]->getHp()},
            {"max_hp", myBases[i]->getMaxHp()},
            {"x", myBases[i]->getPosition().x},
            {"y", myBases[i]->getPosition().y},
            {"nearby_allies", nearbyAllies},
            {"nearby_enemies", nearbyEnemies}
        });
    }
    
    // 敌方基地（最多5个）
    state["enemy_bases"] = json::array();
    std::vector<std::shared_ptr<Base>> enemyBases;
    for (const auto& base : model->bases) {
        if (static_cast<int>(base->getTeam()) != myTeam) {
            enemyBases.push_back(base);
        }
    }
    std::sort(enemyBases.begin(), enemyBases.end(),
              [](const auto& a, const auto& b) { return a->getHp() > b->getHp(); });
    
    for (size_t i = 0; i < std::min(enemyBases.size(), size_t(5)); i++) {
        int distance = getDistanceToNearestBase(enemyBases[i]->getPosition(), myTeam);
        
        state["enemy_bases"].push_back({
            {"hp", enemyBases[i]->getHp()},
            {"max_hp", enemyBases[i]->getMaxHp()},
            {"x", enemyBases[i]->getPosition().x},
            {"y", enemyBases[i]->getPosition().y},
            {"distance_to_nearest_my_base", distance}
        });
    }
    
    // 士兵类型统计
    int myArcher = 0, myInfantry = 0, myCavalry = 0, myCaster = 0, myDoctor = 0;
    int enemyArcher = 0, enemyInfantry = 0, enemyCavalry = 0, enemyCaster = 0, enemyDoctor = 0;
    float myAvgX = 0, myAvgY = 0, enemyAvgX = 0, enemyAvgY = 0;
    int myFrontCount = 0, enemyFrontCount = 0;
    
    for (const auto& soldier : model->soldiers) {
        bool isMy = (static_cast<int>(soldier->getTeam()) == myTeam);
        
        // 统计兵种
        if (isMy) {
            switch (soldier->getType()) {
                case SoldierType::ARCHER: myArcher++; break;
                case SoldierType::INFANTRY: myInfantry++; break;
                case SoldierType::CAVALRY: myCavalry++; break;
                case SoldierType::CASTER: myCaster++; break;
                case SoldierType::DOCTOR: myDoctor++; break;
            }
            myAvgX += soldier->getPosition().x;
            myAvgY += soldier->getPosition().y;
            if (soldier->getPosition().y > 10) myFrontCount++;
        } else {
            switch (soldier->getType()) {
                case SoldierType::ARCHER: enemyArcher++; break;
                case SoldierType::INFANTRY: enemyInfantry++; break;
                case SoldierType::CAVALRY: enemyCavalry++; break;
                case SoldierType::CASTER: enemyCaster++; break;
                case SoldierType::DOCTOR: enemyDoctor++; break;
            }
            enemyAvgX += soldier->getPosition().x;
            enemyAvgY += soldier->getPosition().y;
            if (soldier->getPosition().y < 10) enemyFrontCount++;
        }
    }
    
    // 计算平均位置
    if (mySoldierCount > 0) {
        myAvgX /= mySoldierCount;
        myAvgY /= mySoldierCount;
    }
    if (enemySoldierCount > 0) {
        enemyAvgX /= enemySoldierCount;
        enemyAvgY /= enemySoldierCount;
    }
    
    state["my_soldier_types"] = {
        {"archer_count", myArcher},
        {"infantry_count", myInfantry},
        {"cavalry_count", myCavalry},
        {"caster_count", myCaster},
        {"doctor_count", myDoctor}
    };
    
    state["enemy_soldier_types"] = {
        {"archer_count", enemyArcher},
        {"infantry_count", enemyInfantry},
        {"cavalry_count", enemyCavalry},
        {"caster_count", enemyCaster},
        {"doctor_count", enemyDoctor}
    };
    
    state["soldier_distribution"] = {
        {"my_avg_x", myAvgX},
        {"my_avg_y", myAvgY},
        {"enemy_avg_x", enemyAvgX},
        {"enemy_avg_y", enemyAvgY},
        {"my_front_soldier_count", myFrontCount},
        {"enemy_front_soldier_count", enemyFrontCount}
    };
    
    // 游戏状态
    state["game_over"] = model->isGameOver();
    state["winner"] = model->isGameOver() ? static_cast<int>(model->getWinner()) : -1;
    
    return state.dump();
}

void GameController::countNearbySoldiers(const Position& basePos, int& allies, int& enemies, int team) {
    allies = 0;
    enemies = 0;
    
    for (const auto& soldier : model->soldiers) {
        int dist = basePos.distanceTo(soldier->getPosition());
        if (dist <= 3) {  // 半径3格
            if (static_cast<int>(soldier->getTeam()) == team) {
                allies++;
            } else {
                enemies++;
            }
        }
    }
}

int GameController::getDistanceToNearestBase(const Position& pos, int team) {
    int minDist = 9999;
    
    for (const auto& base : model->bases) {
        if (static_cast<int>(base->getTeam()) == team) {
            int dist = pos.distanceTo(base->getPosition());
            minDist = std::min(minDist, dist);
        }
    }
    
    return minDist;
}

bool GameController::parseAndExecuteAction(int team, const std::string& actionJson) {
    try {
        json action = json::parse(actionJson);
        
        int actionType = action["action_type"];
        
        // action_type = 0: wait
        if (actionType == 0) {
            return true;
        }
        
        // action_type = 1: spawn
        int baseId = action["base_id"];
        int unitType = action["unit_type"];

        // 获取该队伍的基地列表
        std::vector<std::shared_ptr<Base>> teamBases;
        for (const auto& base : model->bases) {
            if (static_cast<int>(base->getTeam()) == team) {
                teamBases.push_back(base);
            }
        }

        // [修复] 检查baseId对应基地是否被摧毁，如果被摧毁则随机选择存活基地
        if (baseId < 0 || baseId >= static_cast<int>(teamBases.size()) || !teamBases[baseId]->isAlive()) {
            std::vector<size_t> aliveBases;
            for (size_t i = 0; i < teamBases.size(); i++) {
                if (teamBases[i]->isAlive()) {
                    aliveBases.push_back(i);
                }
            }

            if (aliveBases.empty()) {
                // 所有基地都被摧毁了，不能出兵
                return true;
            }

            // 随机选择一个存活基地
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, aliveBases.size() - 1);
            baseId = aliveBases[dis(gen)];
        }
        
        // 转换unitType为SoldierType（支持全部5种兵）
        SoldierType soldierType;
        switch (unitType) {
            case 0: soldierType = SoldierType::ARCHER; break;
            case 1: soldierType = SoldierType::INFANTRY; break;
            case 2: soldierType = SoldierType::CAVALRY; break;
            case 3: soldierType = SoldierType::CASTER; break;   // 支持法师
            case 4: soldierType = SoldierType::DOCTOR; break;   // 支持医疗兵
            default: return false;  // 无效的unit_type
        }
        
        // 执行购买
        Position basePos = teamBases[baseId]->getPosition();
        bool success = purchaseSoldier(static_cast<Team>(team), soldierType, basePos);
        
        // 记录生成事件
        if (success && trainingLogger && gameMode == GameMode::TRAINING) {
            std::string typeName = CombatSystem::getSoldierTypeName(soldierType);
            GameEvent spawnEvent(EventType::SPAWN, team, currentTurn, "Spawn " + typeName);
            spawnEvent.soldier_id = baseId; // 在这里临时借用soldier_id存一下是从哪个基地生成的（方便计算奖励）
            trainingLogger->addEvent(spawnEvent);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse action JSON: " << e.what() << std::endl;
        return false;
    }
}

void GameController::setGameMode(GameMode mode, PlayerType team0, PlayerType team1) {
    gameMode = mode;
    team0Type = team0;
    team1Type = team1;
    
    // 重新初始化相关组件
    if (team0Type == PlayerType::AI_PYTHON || team1Type == PlayerType::AI_PYTHON) {
        if (!pythonAgent) {
            pythonAgent = std::make_unique<PythonAgent>();
        }
        pythonAgent->initialize("python/infer.py");
    }
    
    if (gameMode == GameMode::TRAINING) {
        trainingLogger = std::make_unique<TrainingLogger>();
        trainingLogger->startGame(gameMode, team0Type, team1Type);
    }
}

