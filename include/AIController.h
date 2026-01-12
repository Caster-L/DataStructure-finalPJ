#ifndef AI_CONTROLLER_H
#define AI_CONTROLLER_H

#include "Model.h"
#include <vector>
#include <memory>
#include <mutex>
#include <random>

using namespace GameConstants;

// 前向声明
class GameController;

// AI控制器类
class AIController {
private:
    std::vector<SoldierType> purchaseQueue;
    std::mutex queueMutex;
    std::mt19937& rng;
    
public:
    explicit AIController(std::mt19937& rng);
    
    // AI购买逻辑 (尝试购买一次，需要GameController来调用purchaseSoldier)
    // 返回值: JSON格式的动作字符串（如果无法购买则返回wait动作）
    std::string tryPurchaseOnce(std::shared_ptr<GameModel> model, GameController* controller, int turnCount, Team team);
    
    // AI移动决策
    std::vector<Position> getMoveCandidates(std::shared_ptr<GameModel> model, std::shared_ptr<Soldier> soldier);
    std::vector<Position> getRetreatPositions(const Position& currentPos, const Position& enemyPos);
    
    // 辅助函数
    std::shared_ptr<Soldier> findNearestEnemy(std::shared_ptr<GameModel> model, std::shared_ptr<Soldier> soldier);
    Position findEnemyBase(std::shared_ptr<GameModel> model, Team team, const Position& fromPos);
    bool isPositionOccupied(std::shared_ptr<GameModel> model, const Position& pos, std::shared_ptr<Soldier> excludeSoldier);
    
    // 拥堵检测
    int countNearbyAllies(std::shared_ptr<GameModel> model, std::shared_ptr<Soldier> soldier, int radius);
    
    // 计算位置的拥挤度（周围己方士兵数量）
    int getCrowdednessAtPosition(std::shared_ptr<GameModel> model, const Position& pos, Team team, int radius);
};

#endif // AI_CONTROLLER_H
