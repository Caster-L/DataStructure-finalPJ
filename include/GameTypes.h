#ifndef GAMETYPES_H
#define GAMETYPES_H

#include <string>

// 游戏模式枚举
enum class GameMode {
    TRAINING,        // 训练模式：AI自对战，无渲染，最快速度
    AI_VS_AI,        // 观战模式：AI对战，有渲染，正常速度
    HUMAN_VS_AI      // 对战模式：人类 vs AI，有渲染，正常速度
};

// 玩家类型枚举
enum class PlayerType {
    HUMAN,           // 人类玩家
    AI_PYTHON,       // Python强化学习AI
    AI_RULE_BASED    // C++规则AI（当前的AIController）
};

// 游戏事件类型，用于强化学习
enum class EventType {
    SPAWN,           // 生成士兵
    MOVE,            // 移动
    ATTACK,          // 攻击
    KILL,            // 击杀
    BASE_DAMAGED,    // 基地受损
    GAME_OVER        // 游戏结束
};

// 游戏事件结构，用于在强化学习中表示发生了什么事件，写入内容
struct GameEvent {
    EventType type;
    int team;
    int turn;
    std::string description;
    
    // 可选字段
    int soldier_id = -1;
    int target_id = -1;
    int damage = 0;
    int reward_energy = 0;
    
    GameEvent(EventType t, int tm, int tn, const std::string& desc)
        : type(t), team(tm), turn(tn), description(desc) {}
};

// 辅助函数
inline std::string gameModeToString(GameMode mode) {
    switch (mode) {
        case GameMode::TRAINING: return "training";
        case GameMode::AI_VS_AI: return "ai_vs_ai";
        case GameMode::HUMAN_VS_AI: return "human_vs_ai";
        default: return "unknown";
    }
}

inline std::string playerTypeToString(PlayerType type) {
    switch (type) {
        case PlayerType::HUMAN: return "human";
        case PlayerType::AI_PYTHON: return "ai_python";
        case PlayerType::AI_RULE_BASED: return "ai_rule_based";
        default: return "unknown";
    }
}

#endif // GAMETYPES_H
