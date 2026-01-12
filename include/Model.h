#ifndef MODEL_H
#define MODEL_H

#include "Constants.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <set>

using namespace GameConstants;

// 位置结构
struct Position {
    int x;
    int y;
    
    Position(int x = 0, int y = 0) : x(x), y(y) {}
    
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
    
    // 计算曼哈顿距离
    int distanceTo(const Position& other) const {
        return std::abs(x - other.x) + std::abs(y - other.y);
    }
    
    // 计算切比雪夫距离（用于攻击范围）
    int chebyshevDistanceTo(const Position& other) const {
        return std::max(std::abs(x - other.x), std::abs(y - other.y));
    }
};

// 士兵类
class Soldier {
protected:
    Position position;
    SoldierType type;
    Team team;
    int hp;
    int maxHp;
    int attack;
    int attackRange;
    int visionRange;
    int moveSpeed;
    int armor;
    std::atomic<bool> alive;
    mutable std::mutex mutex;
    
    // 视野共享系统
    std::set<int> lastTurnVisibleEnemies;  // 上一回合可见的敌人ID列表
    std::set<int> sharedVisibleEnemies;     // 本回合队友共享的敌人ID列表
    mutable std::mutex visionMutex;         // 视野数据的互斥锁
    
public:
    Soldier(Position pos, SoldierType type, Team team);
    virtual ~Soldier() = default;
    
    // Getters
    Position getPosition() const;
    SoldierType getType() const { return type; }
    Team getTeam() const { return team; }
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    int getAttack() const { return attack; }
    int getAttackRange() const { return attackRange; }
    int getVisionRange() const { return visionRange; }
    int getMoveSpeed() const { return moveSpeed; }
    int getArmor() const { return armor; }
    bool isAlive() const { return alive.load(); }
    
    // Setters
    void setPosition(const Position& pos);
    void setHp(int newHp);  // 设置生命值（用于治疗）
    
    // 行为
    void takeDamage(int damage);
    bool canAttack(const Position& target) const;
    bool canSee(const Position& target) const;
    
    // 视野共享相关方法
    void updateLastTurnVision(const std::set<int>& enemyIds);
    void updateSharedVision(const std::set<int>& sharedEnemyIds);
    std::set<int> getLastTurnVisibleEnemies() const;
    std::set<int> getSharedVisibleEnemies() const;
};

// 基地类
class Base {
private:
    Position position;
    Team team;
    std::atomic<int> hp;
    int maxHp;
    mutable std::mutex mutex;
    
public:
    Base(Position pos, Team team);
    
    // Getters
    Position getPosition() const { return position; }
    Team getTeam() const { return team; }
    int getHp() const { return hp.load(); }
    int getMaxHp() const { return maxHp; }
    bool isAlive() const { return hp.load() > 0; }
    
    // 行为
    void takeDamage(int damage);
};

// 地图类
class GameMap {
private:
    std::vector<std::vector<TerrainType>> terrain;
    mutable std::mutex mutex;
    
public:
    GameMap();
    
    void initialize(); // 生成地形和障碍
    bool isWalkable(const Position& pos) const; // 判断是否可通行
    bool isValidPosition(const Position& pos) const; // 是否在地图内
    TerrainType getTerrainAt(const Position& pos) const;
    void setTerrainAt(const Position& pos, TerrainType type);
    
    int getSize() const { return MAP_SIZE; }
    
private:
    void generateObstacles();  // 生成障碍物
};

// 队伍数据结构
struct TeamData {
    int energy;
    TeamData() : energy(INITIAL_ENERGY) {}
};

// 游戏状态类（Model层的核心）
class GameModel {
private:
    std::unique_ptr<GameMap> gameMap;
    std::vector<std::unique_ptr<Base>> basesTeamA;
    std::vector<std::unique_ptr<Base>> basesTeamB;
    mutable std::mutex soldiersMutex;
    std::atomic<bool> gameOver;
    std::atomic<Team> winner;
    int turnCount;
    
    // 能量系统
    std::atomic<int> energyTeamA;
    std::atomic<int> energyTeamB;
    mutable std::mutex energyMutex;
    
public:
    // 公开的队伍数据和基地访问
    TeamData teams[2];  // Team 0 和 Team 1
    std::vector<std::shared_ptr<Base>> bases;  // 所有基地的统一列表
    std::vector<std::shared_ptr<Soldier>> soldiers;  // 所有士兵列表（公开以便AI访问）
    
    GameModel();
    
    // Getters
    GameMap* getMap() const { return gameMap.get(); }
    const std::vector<std::unique_ptr<Base>>& getBasesTeamA() const { return basesTeamA; }
    const std::vector<std::unique_ptr<Base>>& getBasesTeamB() const { return basesTeamB; }
    std::vector<std::shared_ptr<Soldier>> getSoldiers() const;
    bool isGameOver() const { return gameOver.load(); }
    Team getWinner() const { return winner.load(); }
    int getTurnCount() const { return turnCount; }
    int getEnergy(Team team) const;
    
    // 行为
    void addSoldier(std::shared_ptr<Soldier> soldier);
    void removeSoldier(std::shared_ptr<Soldier> soldier);
    void incrementTurn() { turnCount++; }
    void setGameOver(Team winningTeam);
    void addEnergy(Team team, int amount);
    bool spendEnergy(Team team, int amount);  // 返回是否成功消费
    
    void initialize();
    
    // 视野共享系统
    void updateSharedVision();  // 更新所有士兵的共享视野
};

#endif // MODEL_H
