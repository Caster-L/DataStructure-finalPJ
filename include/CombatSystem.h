#ifndef COMBAT_SYSTEM_H
#define COMBAT_SYSTEM_H

#include "Model.h"
#include "GameTypes.h"
#include <memory>
#include <map>
#include <vector>

using namespace GameConstants;

// 战斗系统类
class CombatSystem {
public:
    // 处理所有战斗，返回每个队伍的治疗量统计 {team -> heal_amount}
    // 收集战斗事件，需要当前回合数
    static std::map<int, int> processCombat(std::shared_ptr<GameModel> model, std::vector<GameEvent>& events, int currentTurn);
    
    // 政击目标，返回是否击杀
    static bool attackTarget(std::shared_ptr<Soldier> attacker, std::shared_ptr<Soldier> target, std::shared_ptr<GameModel> model, std::vector<GameEvent>& events, int currentTurn);
    
    // 攻击基地
    static void attackBase(std::shared_ptr<Soldier> attacker, Base* base, std::shared_ptr<GameModel> model, std::vector<GameEvent>& events, int currentTurn);
    
    // 获取士兵价格
    static int getSoldierCost(SoldierType type);
    
    // 获取士兵类型名称
    static std::string getSoldierTypeName(SoldierType type);
};

#endif // COMBAT_SYSTEM_H
