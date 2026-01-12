#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "Model.h"
#include "AIController.h"
#include "CombatSystem.h"
#include "GameTypes.h"
#include "PythonAgent.h"
#include "TrainingLogger.h"
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <random>
#include <string>

// 游戏控制器类
class GameController {
private:
    std::shared_ptr<GameModel> model;
    std::atomic<bool> running;
    std::vector<std::thread> workerThreads;
    
    // AI控制器和随机数生成器
    std::mt19937 rng;
    std::unique_ptr<AIController> aiControllerTeam0;  // Team 0的AI控制器
    std::unique_ptr<AIController> aiControllerTeam1;  // Team 1的AI控制器
    
    // 新增：游戏模式和玩家类型
    GameMode gameMode;
    PlayerType team0Type;
    PlayerType team1Type;
    
    // Python AI代理
    std::unique_ptr<PythonAgent> pythonAgent;
    
    // 训练日志记录器
    std::unique_ptr<TrainingLogger> trainingLogger;
    
    // 回合计数器
    int currentTurn;
    
    // 治疗量统计（每回合）
    int team0HealThisTurn;
    int team1HealThisTurn;
    
public:
    explicit GameController(std::shared_ptr<GameModel> model, 
                           GameMode mode = GameMode::HUMAN_VS_AI,
                           PlayerType team0 = PlayerType::HUMAN,
                           PlayerType team1 = PlayerType::AI_RULE_BASED);
    ~GameController();
    
    // 游戏控制
    void start();
    void stop();
    bool isRunning() const { return running.load(); }
    
    // 游戏循环
    void gameLoop();
    
    // 公共接口：供View调用
    bool purchaseSoldier(Team team, SoldierType type, const Position& basePos);
    
    // 获取当前回合数
    int getCurrentTurn() const { return currentTurn; }
    
    // 设置游戏模式
    void setGameMode(GameMode mode, PlayerType team0, PlayerType team1);

    
private:
    // 回合处理
    void processTurn();
    
    // 能量系统
    void generateEnergy();
    
    // 士兵购买辅助
    Position findSpawnPosition(Team team, const Position& basePos);
    
    // 士兵行为（在单独线程中处理）
    void processSoldierBehavior(std::shared_ptr<Soldier> soldier);
    
    // 清理死亡士兵
    void cleanupDeadSoldiers();
    
    // 检查游戏结束
    void checkGameOver();
    
    // 状态序列化为JSON（75维特征）
    std::string getStateJson(int team);
    
    // 执行Python AI的动作
    void executeAIAction(int team, const std::string& actionJson);
    
    // 解析动作JSON并执行
    bool parseAndExecuteAction(int team, const std::string& actionJson);
    
    // 计算基地周围的士兵数量
    void countNearbySoldiers(const Position& basePos, int& allies, int& enemies, int team);
    
    // 计算到最近基地的距离
    int getDistanceToNearestBase(const Position& pos, int team);
};

#endif // CONTROLLER_H
