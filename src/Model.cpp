#include "../include/Model.h"
#include <random>
#include <algorithm>

// 随机数生成器
static std::random_device rd;
static std::mt19937 gen(rd());

// Soldier 实现
Soldier::Soldier(Position pos, SoldierType type, Team team)
    : position(pos), type(type), team(team), alive(true) {
    
    // 根据类型初始化属性
    switch (type) {
        case SoldierType::ARCHER:
            maxHp = hp = Archer::HP;
            attack = Archer::ATTACK;
            attackRange = Archer::ATTACK_RANGE;
            visionRange = Archer::VISION_RANGE;
            moveSpeed = Archer::MOVE_SPEED;
            armor = Archer::ARMOR;
            break;
        case SoldierType::INFANTRY:
            maxHp = hp = Infantry::HP;
            attack = Infantry::ATTACK;
            attackRange = Infantry::ATTACK_RANGE;
            visionRange = Infantry::VISION_RANGE;
            moveSpeed = Infantry::MOVE_SPEED;
            armor = Infantry::ARMOR;
            break;
        case SoldierType::CAVALRY:
            maxHp = hp = Cavalry::HP;
            attack = Cavalry::ATTACK;
            attackRange = Cavalry::ATTACK_RANGE;
            visionRange = Cavalry::VISION_RANGE;
            moveSpeed = Cavalry::MOVE_SPEED;
            armor = Cavalry::ARMOR;
            break;
        case SoldierType::CASTER:
            maxHp = hp = Caster::HP;
            attack = Caster::ATTACK;
            attackRange = Caster::ATTACK_RANGE;
            visionRange = Caster::VISION_RANGE;
            moveSpeed = Caster::MOVE_SPEED;
            armor = Caster::ARMOR;
            break;
        case SoldierType::DOCTOR:
            maxHp = hp = Doctor::HP;
            attack = Doctor::ATTACK;
            attackRange = Doctor::ATTACK_RANGE;
            visionRange = Doctor::VISION_RANGE;
            moveSpeed = Doctor::MOVE_SPEED;
            armor = Doctor::ARMOR;
            break;
    }
}

Position Soldier::getPosition() const {
    std::lock_guard<std::mutex> lock(mutex);
    return position;
}

void Soldier::setPosition(const Position& pos) {
    std::lock_guard<std::mutex> lock(mutex);
    position = pos;
}

void Soldier::setHp(int newHp) {
    std::lock_guard<std::mutex> lock(mutex);
    hp = std::max(0, std::min(newHp, maxHp));  // 限制在 [0, maxHp] 范围内
    if (hp <= 0) {
        alive.store(false);
    }
}

void Soldier::takeDamage(int damage) {
    std::lock_guard<std::mutex> lock(mutex);
    int actualDamage = std::max(0, damage - armor);
    hp -= actualDamage;
    if (hp <= 0) {
        hp = 0;
        alive.store(false);
    }
}

bool Soldier::canAttack(const Position& target) const {
    std::lock_guard<std::mutex> lock(mutex);
    return position.chebyshevDistanceTo(target) <= attackRange;
}

bool Soldier::canSee(const Position& target) const {
    std::lock_guard<std::mutex> lock(mutex);
    return position.chebyshevDistanceTo(target) <= visionRange;
}

// 视野共享相关方法
void Soldier::updateLastTurnVision(const std::set<int>& enemyIds) {
    std::lock_guard<std::mutex> lock(visionMutex);
    lastTurnVisibleEnemies = enemyIds;
}

void Soldier::updateSharedVision(const std::set<int>& sharedEnemyIds) {
    std::lock_guard<std::mutex> lock(visionMutex);
    sharedVisibleEnemies = sharedEnemyIds;
}

std::set<int> Soldier::getLastTurnVisibleEnemies() const {
    std::lock_guard<std::mutex> lock(visionMutex);
    return lastTurnVisibleEnemies;
}

std::set<int> Soldier::getSharedVisibleEnemies() const {
    std::lock_guard<std::mutex> lock(visionMutex);
    return sharedVisibleEnemies;
}

// Base 实现
Base::Base(Position pos, Team team)
    : position(pos), team(team), hp(BASE_HP), maxHp(BASE_HP) {}

void Base::takeDamage(int damage) {
    int currentHp = hp.load();
    int newHp = std::max(0, currentHp - damage);
    hp.store(newHp);
}

// GameMap 实现
GameMap::GameMap() : terrain(MAP_SIZE, std::vector<TerrainType>(MAP_SIZE, TerrainType::PLAIN)) {}

void GameMap::initialize() {
    std::lock_guard<std::mutex> lock(mutex);
    
    // 初始化为平原
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            terrain[i][j] = TerrainType::PLAIN;
        }
    }
    
    // 基地位置将在GameModel::initialize中设置
    
    // 生成障碍物
    generateObstacles();
}

void GameMap::generateObstacles() {
    std::uniform_int_distribution<> dis(0, MAP_SIZE - 1);
    std::uniform_int_distribution<> typeDis(0, 1);
    
    // 生成约2%的障碍物，使用4-相邻聚类（上下左右相连）
    int obstacleCount = (MAP_SIZE * MAP_SIZE) / 70;
    
    for (int i = 0; i < obstacleCount; i++) {
        int x = dis(gen);
        int y = dis(gen);
        
        // 不在基地附近生成障碍物
        if ((x >= 2 && x <= 8 && y >= 2 && y <= 8) ||
            (x >= MAP_SIZE - 9 && x <= MAP_SIZE - 3 && 
             y >= MAP_SIZE - 9 && y <= MAP_SIZE - 3)) {
            continue;
        }
        
        if (terrain[x][y] == TerrainType::PLAIN) {
            // 随机选择障碍物类型（山脉或河流）
            TerrainType obstacleType = typeDis(gen) == 0 ? TerrainType::MOUNTAIN : TerrainType::RIVER;
            terrain[x][y] = obstacleType;
            
            // 创建4-相邻聚类：只有上下左右有概率是同一种类型（不包括对角线）
            std::uniform_int_distribution<> clusterDis(0, 100);
            
            // 4-相邻：上、下、左、右
            int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (int d = 0; d < 4; d++) {
                int nx = x + directions[d][0];
                int ny = y + directions[d][1];
                
                // 检查边界和基地范围
                if (nx < 0 || nx >= MAP_SIZE || ny < 0 || ny >= MAP_SIZE) continue;
                if ((nx >= 2 && nx <= 8 && ny >= 2 && ny <= 8) ||
                    (nx >= MAP_SIZE - 9 && nx <= MAP_SIZE - 3 && 
                     ny >= MAP_SIZE - 9 && ny <= MAP_SIZE - 3)) continue;
                
                // 相邻格子：70%概率与中心相同类型（强边连接）
                if (terrain[nx][ny] == TerrainType::PLAIN && clusterDis(gen) < 70) {
                    terrain[nx][ny] = obstacleType;
                }
            }
            
            // 次级扩展：上下左右的相邻格子再向外扩展一层（概率降低）
            for (int d = 0; d < 4; d++) {
                int nx = x + directions[d][0] * 2;
                int ny = y + directions[d][1] * 2;
                
                if (nx < 0 || nx >= MAP_SIZE || ny < 0 || ny >= MAP_SIZE) continue;
                if ((nx >= 2 && nx <= 8 && ny >= 2 && ny <= 8) ||
                    (nx >= MAP_SIZE - 9 && nx <= MAP_SIZE - 3 && 
                     ny >= MAP_SIZE - 9 && ny <= MAP_SIZE - 3)) continue;
                
                // 距离2倍的格子：40%概率
                if (terrain[nx][ny] == TerrainType::PLAIN && clusterDis(gen) < 40) {
                    terrain[nx][ny] = obstacleType;
                }
            }
        }
    }
    
    // 后处理：清除基地附近3格内的所有障碍物，确保基地有活动空间
    // Team A基地（左上角，约5,5）
    for (int x = 2; x <= 8; x++) {
        for (int y = 2; y <= 8; y++) {
            if (terrain[x][y] == TerrainType::MOUNTAIN || 
                terrain[x][y] == TerrainType::RIVER) {
                terrain[x][y] = TerrainType::PLAIN;
            }
        }
    }
    
    // Team B基地（右下角，约14,14）
    for (int x = MAP_SIZE - 9; x <= MAP_SIZE - 3; x++) {
        for (int y = MAP_SIZE - 9; y <= MAP_SIZE - 3; y++) {
            if (terrain[x][y] == TerrainType::MOUNTAIN || 
                terrain[x][y] == TerrainType::RIVER) {
                terrain[x][y] = TerrainType::PLAIN;
            }
        }
    }
}

bool GameMap::isWalkable(const Position& pos) const {
    if (!isValidPosition(pos)) return false;
    
    std::lock_guard<std::mutex> lock(mutex);
    TerrainType type = terrain[pos.x][pos.y];
    return type == TerrainType::PLAIN || 
           type == TerrainType::BASE_A || 
           type == TerrainType::BASE_B;
}

bool GameMap::isValidPosition(const Position& pos) const {
    return pos.x >= 0 && pos.x < MAP_SIZE && pos.y >= 0 && pos.y < MAP_SIZE;
}

TerrainType GameMap::getTerrainAt(const Position& pos) const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!isValidPosition(pos)) return TerrainType::MOUNTAIN;
    return terrain[pos.x][pos.y];
}

void GameMap::setTerrainAt(const Position& pos, TerrainType type) {
    std::lock_guard<std::mutex> lock(mutex);
    if (isValidPosition(pos)) {
        terrain[pos.x][pos.y] = type;
    }
}

// GameModel 实现
GameModel::GameModel() 
    : gameOver(false), winner(Team::TEAM_A), turnCount(0),
      energyTeamA(INITIAL_ENERGY), energyTeamB(INITIAL_ENERGY) {}

void GameModel::initialize() {
    // 初始化地图
    gameMap = std::make_unique<GameMap>();
    gameMap->initialize();
    
    // 初始化基地 - Team A（蓝色，上半平面）
    basesTeamA.clear();
    if (BASE_COUNT_PER_TEAM == 1) {
        basesTeamA.push_back(std::make_unique<Base>(Position(5, 5), Team::TEAM_A));
    } else if (BASE_COUNT_PER_TEAM == 3) {
        // 3个基地不对称分布在上半平面
        basesTeamA.push_back(std::make_unique<Base>(Position(8, 12), Team::TEAM_A));      // 左侧基地
        basesTeamA.push_back(std::make_unique<Base>(Position(32, 5), Team::TEAM_A));      // 中央基地（偏上）
        basesTeamA.push_back(std::make_unique<Base>(Position(55, 18), Team::TEAM_A));     // 右侧基地（偏下）
    } else {
        // 兼容其他数量
        for (int i = 0; i < BASE_COUNT_PER_TEAM; i++) {
            int offset = i * 15;
            basesTeamA.push_back(std::make_unique<Base>(
                Position(5 + offset, 8 + (i % 2) * 8), Team::TEAM_A));
        }
    }
    
    // 初始化基地 - Team B（红色，下半平面）
    basesTeamB.clear();
    if (BASE_COUNT_PER_TEAM == 1) {
        basesTeamB.push_back(std::make_unique<Base>(
            Position(MAP_SIZE - 6, MAP_SIZE - 6), Team::TEAM_B));
    } else if (BASE_COUNT_PER_TEAM == 3) {
        // 3个基地不对称分布在下半平面（与TeamA呈镜像但不完全对称）
        basesTeamB.push_back(std::make_unique<Base>(Position(10, MAP_SIZE - 15), Team::TEAM_B));    // 左侧基地（偏上）
        basesTeamB.push_back(std::make_unique<Base>(Position(35, MAP_SIZE - 8), Team::TEAM_B));     // 中央基地（偏下）
        basesTeamB.push_back(std::make_unique<Base>(Position(58, MAP_SIZE - 20), Team::TEAM_B));    // 右侧基地
    } else {
        // 兼容其他数量
        for (int i = 0; i < BASE_COUNT_PER_TEAM; i++) {
            int offset = i * 15;
            basesTeamB.push_back(std::make_unique<Base>(
                Position(MAP_SIZE - 6 - offset, MAP_SIZE - 8 - (i % 2) * 8), Team::TEAM_B));
        }
    }
    
    // 在地图上标记基地位置
    for (const auto& base : basesTeamA) {
        gameMap->setTerrainAt(base->getPosition(), TerrainType::BASE_A);
    }
    for (const auto& base : basesTeamB) {
        gameMap->setTerrainAt(base->getPosition(), TerrainType::BASE_B);
    }
    
    // 填充统一的bases列表（供AI使用）
    bases.clear();
    for (const auto& base : basesTeamA) {
        bases.push_back(std::shared_ptr<Base>(base.get(), [](Base*){}));  // 非拥有型shared_ptr
    }
    for (const auto& base : basesTeamB) {
        bases.push_back(std::shared_ptr<Base>(base.get(), [](Base*){}));
    }
    
    // 清空士兵列表
    {
        std::lock_guard<std::mutex> lock(soldiersMutex);
        soldiers.clear();
    }
    
    gameOver.store(false);
    turnCount = 0;
    
    // 初始化队伍能量
    teams[0].energy = INITIAL_ENERGY;
    teams[1].energy = INITIAL_ENERGY;
}

std::vector<std::shared_ptr<Soldier>> GameModel::getSoldiers() const {
    std::lock_guard<std::mutex> lock(soldiersMutex);
    return soldiers;
}

void GameModel::addSoldier(std::shared_ptr<Soldier> soldier) {
    std::lock_guard<std::mutex> lock(soldiersMutex);
    soldiers.push_back(soldier);
}

void GameModel::removeSoldier(std::shared_ptr<Soldier> soldier) {
    std::lock_guard<std::mutex> lock(soldiersMutex);
    soldiers.erase(
        std::remove(soldiers.begin(), soldiers.end(), soldier),
        soldiers.end()
    );
}

void GameModel::setGameOver(Team winningTeam) {
    gameOver.store(true);
    winner.store(winningTeam);
}

int GameModel::getEnergy(Team team) const {
    return (team == Team::TEAM_A) ? energyTeamA.load() : energyTeamB.load();
}

void GameModel::addEnergy(Team team, int amount) {
    std::lock_guard<std::mutex> lock(energyMutex);
    if (team == Team::TEAM_A) {
        energyTeamA.store(energyTeamA.load() + amount);
    } else {
        energyTeamB.store(energyTeamB.load() + amount);
    }
}

bool GameModel::spendEnergy(Team team, int amount) {
    std::lock_guard<std::mutex> lock(energyMutex);
    if (team == Team::TEAM_A) {
        int current = energyTeamA.load();
        if (current >= amount) {
            energyTeamA.store(current - amount);
            return true;
        }
    } else {
        int current = energyTeamB.load();
        if (current >= amount) {
            energyTeamB.store(current - amount);
            return true;
        }
    }
    return false;
}

void GameModel::updateSharedVision() {
    std::lock_guard<std::mutex> lock(soldiersMutex);
    
    // 第一步：保存所有士兵当前可见的敌人ID到lastTurnVisibleEnemies
    for (auto& soldier : soldiers) {
        if (!soldier->isAlive()) continue;
        
        std::set<int> currentVisibleEnemies;
        Position myPos = soldier->getPosition();
        Team myTeam = soldier->getTeam();
        
        // 遍历所有敌人，找出当前视野内的敌人
        for (size_t i = 0; i < soldiers.size(); ++i) {
            const auto& other = soldiers[i];
            if (!other->isAlive()) continue;
            if (other->getTeam() == myTeam) continue;
            
            Position otherPos = other->getPosition();
            if (soldier->canSee(otherPos)) {
                currentVisibleEnemies.insert(static_cast<int>(i));
            }
        }
        
        soldier->updateLastTurnVision(currentVisibleEnemies);
    }
    
    // 第二步：为每个士兵收集队友共享的视野
    for (auto& soldier : soldiers) {
        if (!soldier->isAlive()) continue;
        
        std::set<int> sharedEnemies;
        Position myPos = soldier->getPosition();
        Team myTeam = soldier->getTeam();
        
        // 遍历所有队友，收集距离<=COMMUNICATION_RANGE的队友的lastTurnVisibleEnemies
        for (const auto& teammate : soldiers) {
            if (!teammate->isAlive()) continue;
            if (teammate->getTeam() != myTeam) continue;
            if (teammate.get() == soldier.get()) continue;  // 跳过自己
            
            Position teammatePos = teammate->getPosition();
            int distance = myPos.chebyshevDistanceTo(teammatePos);
            
            if (distance <= COMMUNICATION_RANGE) {
                // 合并队友上回合看到的敌人
                std::set<int> teammateVision = teammate->getLastTurnVisibleEnemies();
                sharedEnemies.insert(teammateVision.begin(), teammateVision.end());
            }
        }
        
        soldier->updateSharedVision(sharedEnemies);
    }
}


