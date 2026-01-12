#ifndef TRAININGLOGGER_H
#define TRAININGLOGGER_H

#include "GameTypes.h"
#include "Model.h"
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <memory>

// 训练日志记录器
class TrainingLogger {
private:
    std::string logData;  // 存储JSON格式的日志
    std::vector<GameEvent> currentTurnEvents;
    int totalTurns;
    std::chrono::steady_clock::time_point startTime;
    GameMode mode;
    PlayerType team0Type;
    PlayerType team1Type;
    bool gameStarted;
    
    // 为了支持更复杂的奖励计算，持有Model指针
    std::shared_ptr<GameModel> model;
    
public:
    TrainingLogger();
    
    // 开始游戏记录
    void startGame(GameMode mode, PlayerType team0, PlayerType team1);
    
    // 记录一个回合
    void recordTurn(int turn, const std::string& stateJson,
                    const std::string& team0Action, const std::string& team1Action);
    
    // 添加事件到当前回合
    void addEvent(const GameEvent& event);
    
    // 结束游戏，保存日志
    void endGame(int winner);
    
    // 计算奖励
    float calculateReward(int team, const std::vector<GameEvent>& events);
    
    void setModel(std::shared_ptr<GameModel> m) { model = m; }
    
private:
    std::string getCurrentTimestamp();
    void saveToFile(const std::string& filename);
};

#endif // TRAININGLOGGER_H
