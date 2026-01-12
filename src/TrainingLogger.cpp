#include "../include/TrainingLogger.h"
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <ctime>

TrainingLogger::TrainingLogger() 
    : totalTurns(0), gameStarted(false) {
}

void TrainingLogger::startGame(GameMode m, PlayerType t0, PlayerType t1) {
    mode = m;
    team0Type = t0;
    team1Type = t1;
    totalTurns = 0;
    gameStarted = true;
    startTime = std::chrono::steady_clock::now();
    
    // 初始化JSON日志
    logData = "{\n";
    logData += "  \"metadata\": {\n";
    logData += "    \"date\": \"" + getCurrentTimestamp() + "\",\n";
    logData += "    \"mode\": \"" + gameModeToString(mode) + "\",\n";
    logData += "    \"team0_type\": \"" + playerTypeToString(team0Type) + "\",\n";
    logData += "    \"team1_type\": \"" + playerTypeToString(team1Type) + "\"\n";
    logData += "  },\n";
    logData += "  \"episodes\": [\n";
}

void TrainingLogger::recordTurn(int turn, const std::string& stateJson,
                                const std::string& team0Action, 
                                const std::string& team1Action) {
    if (!gameStarted) return;
    
    if (turn > 0) {
        logData += ",\n";
    }
    
    logData += "    {\n";
    logData += "      \"turn\": " + std::to_string(turn) + ",\n";
    logData += "      \"state\": " + stateJson + ",\n";
    logData += "      \"team0_action\": " + team0Action + ",\n";
    logData += "      \"team1_action\": " + team1Action + ",\n";
    
    // 计算奖励
    float reward0 = calculateReward(0, currentTurnEvents);
    float reward1 = calculateReward(1, currentTurnEvents);
    logData += "      \"reward\": {\"team0\": " + std::to_string(reward0) + 
               ", \"team1\": " + std::to_string(reward1) + "},\n";
    
    // 记录事件
    logData += "      \"events\": [\n";
    for (size_t i = 0; i < currentTurnEvents.size(); ++i) {
        if (i > 0) logData += ",\n";
        const auto& evt = currentTurnEvents[i];
        logData += "        {\"type\": \"" + evt.description + "\", \"team\": " + 
                   std::to_string(evt.team) + "}";
    }
    logData += "\n      ]\n";
    logData += "    }";
    
    currentTurnEvents.clear();
    totalTurns = turn + 1;
}

void TrainingLogger::addEvent(const GameEvent& event) {
    currentTurnEvents.push_back(event);
}

void TrainingLogger::endGame(int winner) {
    if (!gameStarted) return;
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(endTime - startTime).count();
    
    logData += "\n  ],\n";
    logData += "  \"summary\": {\n";
    logData += "    \"total_turns\": " + std::to_string(totalTurns) + ",\n";
    logData += "    \"winner\": " + std::to_string(winner) + ",\n";
    logData += "    \"duration_seconds\": " + std::to_string(duration) + "\n";
    logData += "  }\n";
    logData += "}\n";
    
    saveToFile("game_log.json");
    gameStarted = false;
}

float TrainingLogger::calculateReward(int team, const std::vector<GameEvent>& events) {
    float reward = 0.0f;
    
    // 1. 基础事件奖励
    for (const auto& evt : events) {
        if (evt.type == EventType::KILL && evt.team == team) {
            reward += 10.0f;
        } else if (evt.type == EventType::BASE_DAMAGED && evt.team != team) {
            reward += evt.damage * 0.05f;
        } else if (evt.type == EventType::BASE_DAMAGED && evt.team == team) {
            reward -= evt.damage * 0.1f;
        } else if (evt.type == EventType::GAME_OVER) {
            if (evt.team == team) {
                reward += 1000.0f;
            } else {
                reward -= 1000.0f;
            }
        }
    }

    // 2. 战场态势分析 (如有Model权限)
    if (model) {
        // 获取所有士兵
        auto soldiers = model->getSoldiers();
        
        // 获取本队基地
        const auto& myBases = (team == 0) ? model->getBasesTeamA() : model->getBasesTeamB();
        
        // 策略修正：检查基地是否危险（附近有敌人）
        for (const auto& base : myBases) {
            if (!base->isAlive()) continue;
            
            // 检查基地周围 5 格有没有敌人
            bool enemyNear = false;
            for (const auto& s : soldiers) {
                if (!s->isAlive()) continue;
                if (static_cast<int>(s->getTeam()) == team) continue; // 忽略队友
                
                int dist = base->getPosition().chebyshevDistanceTo(s->getPosition());
                if (dist <= 5) {
                    enemyNear = true;
                    break;
                }
            }
            
            // 如果基地危险
            if (enemyNear) {
                // 原有的逻辑缺陷修正：
                // 如果基地危险，我们通过遍历 events 检查本回合有没有生成新的士兵 (SPAWN)
                // 且该士兵是在这个被围攻的基地生成的（使用 soldier_id 记录的 baseId）
                
                // 查找该基地的ID（在TeamBases数组中的索引）
                int currentBaseId = -1;
                const auto& teamBaseVec = (team == 0) ? model->getBasesTeamA() : model->getBasesTeamB();
                for(size_t i=0; i<teamBaseVec.size(); ++i) {
                     if(teamBaseVec[i] == base) {
                         currentBaseId = i;
                         break;
                     }
                }

                bool defended = false;
                for (const auto& evt : events) {
                    // 如果本回合不仅生成了兵，而且是在这个危险的基地生成的
                    if (evt.type == EventType::SPAWN && evt.team == team && evt.soldier_id == currentBaseId) {
                        defended = true;
                        break;
                    }
                }
                
                if (defended) {
                    reward += 5.0f; // 危机时刻正确出兵：奖励
                } else {
                    reward -= 5.0f; // 危机时刻不出兵/在别处出兵：惩罚
                    // 如果完全没有出兵，惩罚更重
                    bool anySpawn = false;
                    for (const auto& evt : events) {
                        if (evt.type == EventType::SPAWN && evt.team == team) {
                            anySpawn = true;
                            break;
                        }
                    }
                    if (!anySpawn) {
                        reward -= 5.0f; // 危机时刻还没出兵 这里的惩罚可以和上面叠加
                    }
                }
            }
        }
    }
    
    return reward;
}

std::string TrainingLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

void TrainingLogger::saveToFile(const std::string& filename) {
    // 使用临时文件策略，避免中断时破坏原文件
    std::string tempFilename = filename + ".tmp";
    
    // 读取现有数据（如果文件存在）
    std::ifstream inFile(filename);
    std::string existingData;
    bool fileExists = false;
    
    if (inFile.is_open()) {
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        existingData = buffer.str();
        inFile.close();
        
        // 检查是否有有效的 JSON 数组（非空且包含games字段）
        if (!existingData.empty() && existingData.find("\"games\": [") != std::string::npos) {
            fileExists = true;
        }
    }
    
    // 写入临时文件
    std::ofstream outFile(tempFilename);
    if (!outFile.is_open()) {
        std::cerr << "Failed to create temporary training log file!" << std::endl;
        return;
    }
    
    if (fileExists) {
        // 找到最后一个游戏的结束位置，在 ] 之前插入新游戏
        size_t lastBracket = existingData.rfind("]");
        if (lastBracket != std::string::npos) {
            // 插入逗号和新游戏数据
            std::string before = existingData.substr(0, lastBracket);
            // 如果已有游戏，需要加逗号
            if (before.find("{") != std::string::npos) {
                before += ",\n";
            }
            before += "    " + logData;
            before += "\n  ]\n}";
            outFile << before;
        }
    } else {
        // 创建新文件
        outFile << "{\n  \"games\": [\n";
        outFile << "    " + logData;
        outFile << "\n  ]\n}";
    }
    
    outFile.flush();  // 确保数据写入磁盘
    outFile.close();
    
    // 写入成功后，用临时文件替换原文件
    if (std::rename(tempFilename.c_str(), filename.c_str()) != 0) {
        std::cerr << "Failed to rename temporary file to " << filename << std::endl;
        // 尝试删除临时文件
        std::remove(tempFilename.c_str());
    } else {
        std::cout << "Training log saved to " << filename << std::endl;
    }
    std::cout << "Training log appended to " << filename << std::endl;
}
