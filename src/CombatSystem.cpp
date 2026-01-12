// CombatSystem.cpp - 战斗系统实现
#include "../include/CombatSystem.h"
#include "../include/Model.h"
#include <iostream>
#include <cstdlib>
#include <map>

std::map<int, int> CombatSystem::processCombat(std::shared_ptr<GameModel> model, std::vector<GameEvent>& events, int currentTurn) {
    auto soldiers = model->getSoldiers();
    
    // 初始化治疗量统计 (team 0 和 team 1)
    std::map<int, int> healStats;
    healStats[0] = 0;
    healStats[1] = 0;
    
    // 第1步：医疗兵治疗友军
    for (auto& doctor : soldiers) {
        if (!doctor->isAlive()) continue;
        if (doctor->getType() != SoldierType::DOCTOR) continue;
        
        // 查找治疗范围内的友军
        for (auto& ally : soldiers) {
            if (!ally->isAlive()) continue;
            if (ally->getTeam() != doctor->getTeam()) continue;
            if (ally == doctor) continue;  // 不治疗自己
            
            int dx = ally->getPosition().x - doctor->getPosition().x;
            int dy = ally->getPosition().y - doctor->getPosition().y;
            int distance = std::abs(dx) + std::abs(dy);
            
            if (distance <= Doctor::HEAL_RANGE) {
                // 治疗：恢复生命值，不超过最大值
                int currentHp = ally->getHp();
                int maxHp = ally->getMaxHp();
                int healAmount = Doctor::HEAL_AMOUNT;
                int newHp = std::min(maxHp, currentHp + healAmount);
                int actualHeal = newHp - currentHp;  // 实际治疗量
                ally->setHp(newHp);
                
                // 统计治疗量
                healStats[static_cast<int>(doctor->getTeam())] += actualHeal;
            }
        }
    }
    
    // 第2步：处理士兵对士兵的攻击
    for (auto& attacker : soldiers) {
        if (!attacker->isAlive()) continue;
        
        // 法师：范围伤害
        if (attacker->getType() == SoldierType::CASTER) {
            // 查找攻击范围内的主要目标
            std::shared_ptr<Soldier> mainTarget = nullptr;
            for (auto& target : soldiers) {
                if (!target->isAlive()) continue;
                if (attacker->getTeam() == target->getTeam()) continue;
                
                if (attacker->canAttack(target->getPosition())) {
                    mainTarget = target;
                    break;
                }
            }
            
            if (mainTarget) {
                // 对主要目标及其周围敌人造成伤害
                for (auto& target : soldiers) {
                    if (!target->isAlive()) continue;
                    if (attacker->getTeam() == target->getTeam()) continue;
                    
                    int dx = target->getPosition().x - mainTarget->getPosition().x;
                    int dy = target->getPosition().y - mainTarget->getPosition().y;
                    int distance = std::abs(dx) + std::abs(dy);
                    
                    if (distance <= Caster::AOE_RANGE) {
                        bool killed = attackTarget(attacker, target, model, events, currentTurn);
                        if (killed) {
                            int cost = getSoldierCost(target->getType());
                            int reward = static_cast<int>(cost * 0.5);
                            model->addEnergy(attacker->getTeam(), reward);
                        }
                    }
                }
            }
        } 
        // 其他兵种：单体攻击
        else {
            for (auto& target : soldiers) {
                if (!target->isAlive()) continue;
                if (attacker->getTeam() == target->getTeam()) continue;
                
                if (attacker->canAttack(target->getPosition())) {
                    bool killed = attackTarget(attacker, target, model, events, currentTurn);
                    
                    // 如果击杀了敌人，给予能量奖励（80%成本）
                    if (killed) {
                        int cost = getSoldierCost(target->getType());
                        int reward = static_cast<int>(cost * 0.5);
                        model->addEnergy(attacker->getTeam(), reward);
                        
                        // 输出击杀信息（训练模式下静默）
                        const char* trainingMode = std::getenv("TRAINING_MODE");
                        if (!trainingMode || std::string(trainingMode) != "1") {
                            std::string attackerTeam = (attacker->getTeam() == Team::TEAM_A) ? "Team A" : "Team B";
                            std::string targetType = getSoldierTypeName(target->getType());
                            std::cout << "[Kill] " << attackerTeam << " killed enemy " << targetType 
                                      << " | Energy reward: " << reward << std::endl;
                        }
                    }
                    
                    break;  // 每回合只攻击一次
                }
            }
        }
    }
    
    // 处理士兵对基地的攻击
    for (auto& attacker : soldiers) {
        if (!attacker->isAlive()) continue;
        
        // 攻击所有范围内的敌方基地
        const auto& enemyBases = (attacker->getTeam() == Team::TEAM_A) ? 
                                 model->getBasesTeamB() : model->getBasesTeamA();
        
        for (const auto& enemyBase : enemyBases) {
            if (enemyBase->isAlive() && 
                attacker->canAttack(enemyBase->getPosition())) {
                attackBase(attacker, enemyBase.get(), model, events, currentTurn);
            }
        }
    }
    
    return healStats;
}

bool CombatSystem::attackTarget(std::shared_ptr<Soldier> attacker, 
                               std::shared_ptr<Soldier> target,
                               std::shared_ptr<GameModel> model,
                               std::vector<GameEvent>& events,
                               int currentTurn) {
    if (!attacker->isAlive() || !target->isAlive()) return false;
    
    int damage = attacker->getAttack();
    
    // 检查攻击者是否在己方基地6格范围内，是则伤害 x1.5
    const auto& attackerBases = (attacker->getTeam() == Team::TEAM_A) ? 
                                model->getBasesTeamA() : model->getBasesTeamB();
    
    bool nearOwnBase = false;
    for (const auto& base : attackerBases) {
        if (!base->isAlive()) continue;
        
        int dx = std::abs(attacker->getPosition().x - base->getPosition().x);
        int dy = std::abs(attacker->getPosition().y - base->getPosition().y);
        int distance = dx + dy;  // 曼哈顿距离
        
        if (distance <= BASE_DEFENSE_RANGE) {
            nearOwnBase = true;
            break;
        }
    }
    
    // 应用基地防御加成
    if (nearOwnBase) {
        damage = static_cast<int>(damage * BASE_DEFENSE_DAMAGE_MULTIPLIER);
    }
    
    target->takeDamage(damage);

    // 生成击杀事件
    if (!target->isAlive()) {
        GameEvent evt(EventType::KILL, static_cast<int>(attacker->getTeam()), currentTurn, "Kill");
        evt.damage = 0; // 击杀事件不记录伤害值，除非有需求
        events.push_back(evt);
    }
    
    // 返回目标是否被击杀
    return !target->isAlive();
}

void CombatSystem::attackBase(std::shared_ptr<Soldier> attacker, Base* base, std::shared_ptr<GameModel> model, std::vector<GameEvent>& events, int currentTurn) {
    if (!attacker->isAlive() || !base->isAlive()) return;
    
    int damage = attacker->getAttack();
    
    // 检查攻击者是否在己方基地6格范围内，是则伤害 x1.5
    const auto& attackerBases = (attacker->getTeam() == Team::TEAM_A) ? 
                                model->getBasesTeamA() : model->getBasesTeamB();
    
    bool nearOwnBase = false;
    for (const auto& ownBase : attackerBases) {
        if (!ownBase->isAlive()) continue;
        
        int dx = std::abs(attacker->getPosition().x - ownBase->getPosition().x);
        int dy = std::abs(attacker->getPosition().y - ownBase->getPosition().y);
        int distance = dx + dy;  // 曼哈顿距离
        
        if (distance <= BASE_DEFENSE_RANGE) {
            nearOwnBase = true;
            break;
        }
    }
    
    // 应用基地防御加成
    if (nearOwnBase) {
        damage = static_cast<int>(damage * BASE_DEFENSE_DAMAGE_MULTIPLIER);
    }
    
    base->takeDamage(damage);
    
    // 生成基地受损事件
    GameEvent evt(EventType::BASE_DAMAGED, static_cast<int>(base->getTeam()), currentTurn, "Base Damaged");
    evt.damage = damage;
    events.push_back(evt);
}

int CombatSystem::getSoldierCost(SoldierType type) {
    switch (type) {
        case SoldierType::ARCHER: return ARCHER_COST;
        case SoldierType::INFANTRY: return INFANTRY_COST;
        case SoldierType::CAVALRY: return CAVALRY_COST;
        case SoldierType::CASTER: return CASTER_COST;
        case SoldierType::DOCTOR: return DOCTOR_COST;
        default: return 999999;
    }
}

std::string CombatSystem::getSoldierTypeName(SoldierType type) {
    switch (type) {
        case SoldierType::ARCHER: return "Archer";
        case SoldierType::INFANTRY: return "Infantry";
        case SoldierType::CAVALRY: return "Cavalry";
        case SoldierType::CASTER: return "Caster";
        case SoldierType::DOCTOR: return "Doctor";
        default: return "Unknown";
    }
}
