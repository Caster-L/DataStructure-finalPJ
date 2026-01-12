// AIController.cpp - AI控制逻辑实现
#include "../include/AIController.h"
#include "../include/Controller.h"
#include "../include/Model.h"
#include "../include/CombatSystem.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <sstream>

AIController::AIController(std::mt19937& rng) : rng(rng) {
    // 初始化AI购买队列，包含所有5种兵种
    std::uniform_int_distribution<> typeDis(0, 4);  // 0-4: 包括Caster和Doctor
    for (int i = 0; i < 5; ++i) {
        int choice = typeDis(rng);
        SoldierType type;
        switch (choice) {
            case 0: type = SoldierType::ARCHER; break;
            case 1: type = SoldierType::INFANTRY; break;
            case 2: type = SoldierType::CAVALRY; break;
            case 3: type = SoldierType::CASTER; break;    // 法师
            case 4: type = SoldierType::DOCTOR; break;    // 治疗兵
            default: type = SoldierType::ARCHER;
        }
        purchaseQueue.push_back(type);
    }
}

std::string AIController::tryPurchaseOnce(std::shared_ptr<GameModel> model, GameController* controller, int turnCount, Team team) {
    // 默认返回 wait 动作
    std::string defaultAction = "{\"action_type\": 0, \"base_id\": -1, \"unit_type\": -1}";
    
    // 改进：不使用固定间隔，而是每回合都尝试购买（如果有足够能量）
    // 这样在游戏初期有初始能量时会连续出兵
    
    std::lock_guard<std::mutex> lock(queueMutex);
    
    if (purchaseQueue.empty()) return defaultAction;
    
    int currentEnergy = model->getEnergy(team);
    
    // 获取队列头部的兵种
    SoldierType type = purchaseQueue.front();
    int cost = CombatSystem::getSoldierCost(type);
    
    // 检查能量是否足够
    if (currentEnergy < cost) {
        return defaultAction;
    }
    
    // 根据队伍选择基地（随机选择）
    const auto& bases = (team == Team::TEAM_A) ? model->getBasesTeamA() : model->getBasesTeamB();
    std::vector<const Base*> aliveBases;
    int baseIndex = 0;
    for (const auto& base : bases) {
        if (base->isAlive()) {
            aliveBases.push_back(base.get());
        }
    }
    
    if (aliveBases.empty()) return defaultAction;
    
    // 随机选择一个基地
    std::uniform_int_distribution<> baseDis(0, aliveBases.size() - 1);
    int selectedBaseIdx = baseDis(rng);
    const Base* selectedBase = aliveBases[selectedBaseIdx];
    Position basePos = selectedBase->getPosition();
    
    // 使用Controller的purchaseSoldier方法
    bool success = controller->purchaseSoldier(team, type, basePos);
    
    if (success) {
        // 购买成功，从队列头部移除
        purchaseQueue.erase(purchaseQueue.begin());
        
        // 在队列尾部随机添加一个新兵种（包含所有5种兵种）
        std::uniform_int_distribution<> typeDis(0, 4);  // 0-4: 所有兵种
        int choice = typeDis(rng);
        SoldierType newType;
        switch (choice) {
            case 0: newType = SoldierType::ARCHER; break;
            case 1: newType = SoldierType::INFANTRY; break;
            case 2: newType = SoldierType::CAVALRY; break;
            case 3: newType = SoldierType::CASTER; break;    // 法师
            case 4: newType = SoldierType::DOCTOR; break;    // 治疗兵
            default: newType = SoldierType::ARCHER;
        }
        purchaseQueue.push_back(newType);
        
        // 返回实际执行的动作 JSON（映射所有5种兵种）
        int unitTypeInt = (type == SoldierType::ARCHER) ? 0 : 
                          (type == SoldierType::INFANTRY) ? 1 :
                          (type == SoldierType::CAVALRY) ? 2 :
                          (type == SoldierType::CASTER) ? 3 : 4;  // Doctor
        
        std::ostringstream oss;
        oss << "{\"action_type\": 1, \"base_id\": " << selectedBaseIdx 
            << ", \"unit_type\": " << unitTypeInt << "}";
        return oss.str();
    }
    
    return defaultAction;
}

std::vector<Position> AIController::getMoveCandidates(std::shared_ptr<GameModel> model, std::shared_ptr<Soldier> soldier) {
    Position currentPos = soldier->getPosition();
    Team team = soldier->getTeam();
    
    // 查找目标位置
    Position targetPos;
    auto nearestEnemy = findNearestEnemy(model, soldier);
    
    if (nearestEnemy) {
        targetPos = nearestEnemy->getPosition();
    } else {
        targetPos = findEnemyBase(model, soldier->getTeam(), currentPos);
    }
    
    // 计算移动方向
    int dx = 0, dy = 0;
    if (targetPos.x > currentPos.x) dx = 1;
    else if (targetPos.x < currentPos.x) dx = -1;
    
    if (targetPos.y > currentPos.y) dy = 1;
    else if (targetPos.y < currentPos.y) dy = -1;
    
    // 添加随机偏移避免完全一致的路径（30%概率）
    std::uniform_int_distribution<> randomDis(0, 9);
    if (randomDis(rng) < 3) {
        // 随机选择侧向移动
        std::uniform_int_distribution<> sideDis(0, 1);
        if (sideDis(rng) == 0) {
            if (dx != 0) dy = (dy == 0) ? (sideDis(rng) == 0 ? 1 : -1) : -dy;
        } else {
            if (dy != 0) dx = (dx == 0) ? (sideDis(rng) == 0 ? 1 : -1) : -dx;
        }
    }
    
    // 收集所有可能的移动位置及其拥挤度
    struct CandidateMove {
        Position pos;
        int priority;      // 优先级（1=最优，2=次优，3=其他）
        int crowdedness;   // 拥挤度
    };
    
    std::vector<CandidateMove> candidateMoves;
    
    // 优先级1：斜向目标（同时x和y方向移动）
    if (dx != 0 && dy != 0) {
        Position pos(currentPos.x + dx, currentPos.y + dy);
        candidateMoves.push_back({pos, 1, getCrowdednessAtPosition(model, pos, team, 2)});
    }
    
    // 优先级2：单方向向目标
    if (dx != 0) {
        Position pos(currentPos.x + dx, currentPos.y);
        candidateMoves.push_back({pos, 2, getCrowdednessAtPosition(model, pos, team, 2)});
    }
    if (dy != 0) {
        Position pos(currentPos.x, currentPos.y + dy);
        candidateMoves.push_back({pos, 2, getCrowdednessAtPosition(model, pos, team, 2)});
    }
    
    // 优先级3：其他方向
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            Position newPos(currentPos.x + i, currentPos.y + j);
            
            // 检查是否已在候选列表中
            bool alreadyAdded = false;
            for (const auto& c : candidateMoves) {
                if (c.pos == newPos) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) {
                candidateMoves.push_back({newPos, 3, getCrowdednessAtPosition(model, newPos, team, 2)});
            }
        }
    }
    
    // 按拥挤度调整优先级：
    // 1. 同优先级内，拥挤度低的优先
    // 2. 如果当前位置拥挤度很高（>=5），大幅降低向拥挤处移动的意愿
    int currentCrowdedness = getCrowdednessAtPosition(model, currentPos, team, 2);
    
    std::sort(candidateMoves.begin(), candidateMoves.end(), 
        [currentCrowdedness](const CandidateMove& a, const CandidateMove& b) {
            // 先按优先级排序
            if (a.priority != b.priority) {
                return a.priority < b.priority;
            }
            
            // 同优先级内，按拥挤度排序
            // 如果当前已经很拥挤，强烈倾向选择更空旷的位置
            if (currentCrowdedness >= 5) {
                return a.crowdedness < b.crowdedness;
            }
            
            // 正常情况下也优先选择拥挤度低的
            return a.crowdedness < b.crowdedness;
        });
    
    // 如果最优选择的拥挤度仍然很高（>=6），考虑不移动
    if (!candidateMoves.empty() && candidateMoves[0].crowdedness >= 6 && currentCrowdedness < 6) {
        // 当前位置比目标位置更好，不移动
        return std::vector<Position>();  // 返回空列表表示不移动
    }
    
    // 转换回Position列表
    std::vector<Position> candidates;
    for (const auto& cm : candidateMoves) {
        candidates.push_back(cm.pos);
    }
    
    return candidates;
}

std::vector<Position> AIController::getRetreatPositions(const Position& currentPos, const Position& enemyPos) {
    // 计算远离敌人的方向
    int dx = 0, dy = 0;
    if (enemyPos.x > currentPos.x) dx = -1;
    else if (enemyPos.x < currentPos.x) dx = 1;
    
    if (enemyPos.y > currentPos.y) dy = -1;
    else if (enemyPos.y < currentPos.y) dy = 1;
    
    std::vector<Position> candidates;
    
    // 优先级1：对角线后撤（最远）
    if (dx != 0 && dy != 0) {
        candidates.push_back(Position(currentPos.x + dx, currentPos.y + dy));
    }
    
    // 优先级2：单方向后撤
    if (dx != 0) {
        candidates.push_back(Position(currentPos.x + dx, currentPos.y));
    }
    if (dy != 0) {
        candidates.push_back(Position(currentPos.x, currentPos.y + dy));
    }
    
    // 优先级3：侧向移动
    if (dy != 0) {
        candidates.push_back(Position(currentPos.x + 1, currentPos.y + dy));
        candidates.push_back(Position(currentPos.x - 1, currentPos.y + dy));
    }
    if (dx != 0) {
        candidates.push_back(Position(currentPos.x + dx, currentPos.y + 1));
        candidates.push_back(Position(currentPos.x + dx, currentPos.y - 1));
    }
    
    return candidates;
}

std::shared_ptr<Soldier> AIController::findNearestEnemy(std::shared_ptr<GameModel> model, std::shared_ptr<Soldier> soldier) {
    auto soldiers = model->getSoldiers();
    std::shared_ptr<Soldier> nearest = nullptr;
    int minDistance = 999999;
    
    Position myPos = soldier->getPosition();
    Team myTeam = soldier->getTeam();
    
    // 获取队友共享的敌人ID列表
    std::set<int> sharedEnemyIds = soldier->getSharedVisibleEnemies();
    
    for (size_t i = 0; i < soldiers.size(); ++i) {
        const auto& other = soldiers[i];
        if (!other->isAlive()) continue;
        if (other->getTeam() == myTeam) continue;
        
        Position otherPos = other->getPosition();
        
        // 检查是否在直接视野内，或在队友共享的视野中
        bool canDetect = soldier->canSee(otherPos) || 
                        (sharedEnemyIds.find(static_cast<int>(i)) != sharedEnemyIds.end());
        
        if (!canDetect) continue;
        
        int distance = myPos.distanceTo(otherPos);
        if (distance < minDistance) {
            minDistance = distance;
            nearest = other;
        }
    }
    
    return nearest;
}

Position AIController::findEnemyBase(std::shared_ptr<GameModel> model, Team team, const Position& fromPos) {
    const auto& enemyBases = (team == Team::TEAM_A) ? 
                             model->getBasesTeamB() : model->getBasesTeamA();
    
    // 找到离fromPos最近的存活敌方基地
    const Base* nearestBase = nullptr;
    int minDistance = 999999;
    
    for (const auto& base : enemyBases) {
        if (base->isAlive()) {
            int distance = fromPos.distanceTo(base->getPosition());
            if (distance < minDistance) {
                minDistance = distance;
                nearestBase = base.get();
            }
        }
    }
    
    if (nearestBase) {
        return nearestBase->getPosition();
    }
    
    return Position(MAP_SIZE / 2, MAP_SIZE / 2);
}

bool AIController::isPositionOccupied(std::shared_ptr<GameModel> model, const Position& pos, std::shared_ptr<Soldier> excludeSoldier) {
    auto soldiers = model->getSoldiers();
    for (const auto& s : soldiers) {
        if (s != excludeSoldier && s->isAlive() && s->getPosition() == pos) {
            return true;
        }
    }
    return false;
}

int AIController::countNearbyAllies(std::shared_ptr<GameModel> model, std::shared_ptr<Soldier> soldier, int radius) {
    int count = 0;
    Position pos = soldier->getPosition();
    Team team = soldier->getTeam();
    
    auto soldiers = model->getSoldiers();
    for (const auto& s : soldiers) {
        if (s != soldier && s->isAlive() && s->getTeam() == team) {
            int dist = abs(s->getPosition().x - pos.x) + abs(s->getPosition().y - pos.y);
            if (dist <= radius) {
                count++;
            }
        }
    }
    return count;
}

int AIController::getCrowdednessAtPosition(std::shared_ptr<GameModel> model, const Position& pos, Team team, int radius) {
    int count = 0;
    
    auto soldiers = model->getSoldiers();
    for (const auto& s : soldiers) {
        if (s->isAlive() && s->getTeam() == team) {
            int dist = abs(s->getPosition().x - pos.x) + abs(s->getPosition().y - pos.y);
            if (dist <= radius) {
                count++;
            }
        }
    }
    return count;
}
