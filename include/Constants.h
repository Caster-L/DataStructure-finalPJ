#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdint>

// 游戏常量
namespace GameConstants {
    constexpr int MAP_SIZE = 64;
    constexpr int WINDOW_WIDTH = 1024;
    constexpr int WINDOW_HEIGHT = 1024;
    constexpr float CELL_SIZE = static_cast<float>(WINDOW_WIDTH) / MAP_SIZE;
    constexpr int TURN_DURATION_MS = 250;  // 每回合 1 秒
    constexpr int MAX_TURNS = 500;  // 最大回合数限制
    
    // 基地数量配置
    constexpr int BASE_COUNT_PER_TEAM = 3;  // 每队基地数量
    
    // 基地常量
    constexpr int BASE_HP = 5000;
    
    // 能量系统
    constexpr int INITIAL_ENERGY = 200;      // 初始能量
    constexpr int ENERGY_PER_TURN = 30;      // 每回合产生能量
    constexpr int ARCHER_COST = 80;          // 弓箭手价格
    constexpr int INFANTRY_COST = 80;        // 步兵价格
    constexpr int CAVALRY_COST = 100;        // 骑兵价格
    constexpr int CASTER_COST = 100;         // 法师价格（范围伤害）
    constexpr int DOCTOR_COST = 80;          // 医疗兵价格（范围治疗）
    
    // 基地防御加成
    constexpr int BASE_DEFENSE_RANGE = 6;         // 基地防御范围（格子）
    constexpr float BASE_DEFENSE_DAMAGE_MULTIPLIER = 1.2f;  // 基地范围内己方攻击倍数
    
    // AI购买间隔
    constexpr int AI_PURCHASE_INTERVAL = 5;  // AI每5回合尝试购买
    
    // 士兵类型枚举
    enum class SoldierType {
        ARCHER,    // 弓箭手
        INFANTRY,  // 步兵
        CAVALRY,   // 骑兵
        CASTER,    // 法师（范围魔法伤害）
        DOCTOR     // 医疗兵（范围治疗）
    };
    
    // 地形类型枚举
    enum class TerrainType {
        PLAIN,      // 平原
        MOUNTAIN,   // 山
        RIVER,      // 河流
        BASE_A,     // 甲方基地
        BASE_B      // 乙方基地
    };
    
    // 阵营枚举
    enum class Team {
        TEAM_A,  // 甲方
        TEAM_B   // 乙方
    };
    
    // 弓箭手属性
    namespace Archer {
        constexpr int HP = 100;
        constexpr int ATTACK = 50;
        constexpr int ATTACK_RANGE = 3;
        constexpr int VISION_RANGE = 7;
        constexpr int MOVE_SPEED = 1;
        constexpr int ARMOR = 5;
    }
    
    // 步兵属性
    namespace Infantry {
        constexpr int HP = 180;
        constexpr int ATTACK = 70;
        constexpr int ATTACK_RANGE = 1;
        constexpr int VISION_RANGE = 4;
        constexpr int MOVE_SPEED = 1;
        constexpr int ARMOR = 20;
    }
    
    // 骑兵属性
    namespace Cavalry {
        constexpr int HP = 170;
        constexpr int ATTACK = 60;
        constexpr int ATTACK_RANGE = 1;
        constexpr int VISION_RANGE = 5;
        constexpr int MOVE_SPEED = 3;
        constexpr int ARMOR = 15;
    }
    
    // 法师属性（高伤害范围攻击，低防御）
    namespace Caster {
        constexpr int HP = 90;
        constexpr int ATTACK = 50;           // 魔法伤害
        constexpr int ATTACK_RANGE = 3;      // 范围3格
        constexpr int VISION_RANGE = 7;      // 视野较远
        constexpr int MOVE_SPEED = 1;
        constexpr int ARMOR = 0;             // 无护甲（脆皮）
        constexpr int AOE_RANGE = 1;         // 范围伤害半径
    }
    
    // 医疗兵属性（范围治疗，低攻击）
    namespace Doctor {
        constexpr int HP = 120;
        constexpr int ATTACK = 10;           // 低攻击力
        constexpr int ATTACK_RANGE = 1;      // 近战
        constexpr int VISION_RANGE = 3;
        constexpr int MOVE_SPEED = 1;
        constexpr int ARMOR = 5;
        constexpr int HEAL_AMOUNT = 30;      // 每回合治疗量
        constexpr int HEAL_RANGE = 2;        // 治疗范围
    }
    
    // 队友通信系统
    constexpr int COMMUNICATION_RANGE = 15;  // 队友间视野共享的最大距离
}

#endif // CONSTANTS_H
