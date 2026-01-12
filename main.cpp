#include <iostream>
#include <memory>
#include <exception>
#include <string>
#include <cstring>
#include <csignal>
#include <atomic>
#include <unistd.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <climits>
#include "include/Model.h"
#include "include/Controller.h"
#include "include/View.h"
#include "include/GameTypes.h"

// 全局标志：用于处理Ctrl+C中断
std::atomic<bool> g_interrupted(false);
std::shared_ptr<GameController> g_controller = nullptr;

// 信号处理函数
void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\n\n️ Received interrupt signal (Ctrl+C)" << std::endl;
        std::cout << "Saving training data gracefully..." << std::endl;
        g_interrupted = true;
        
        // 停止游戏控制器，触发保存
        if (g_controller) {
            g_controller->stop();
        }
    }
}

// 解析命令行参数
struct GameConfig {
    GameMode mode = GameMode::HUMAN_VS_AI;
    PlayerType team0 = PlayerType::HUMAN;
    PlayerType team1 = PlayerType::AI_RULE_BASED;
};

GameConfig parseArgs(int argc, char* argv[]) {
    GameConfig config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--mode" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "training") config.mode = GameMode::TRAINING;
            else if (mode == "ai_vs_ai") config.mode = GameMode::AI_VS_AI;
            else if (mode == "human_vs_ai") config.mode = GameMode::HUMAN_VS_AI;
        }
        else if (arg == "--team0" && i + 1 < argc) {
            std::string type = argv[++i];
            if (type == "human") config.team0 = PlayerType::HUMAN;
            else if (type == "ai_python") config.team0 = PlayerType::AI_PYTHON;
            else if (type == "ai_rule") config.team0 = PlayerType::AI_RULE_BASED;
        }
        else if (arg == "--team1" && i + 1 < argc) {
            std::string type = argv[++i];
            if (type == "human") config.team1 = PlayerType::HUMAN;
            else if (type == "ai_python") config.team1 = PlayerType::AI_PYTHON;
            else if (type == "ai_rule") config.team1 = PlayerType::AI_RULE_BASED;
        }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --mode <mode>       Game mode: training, ai_vs_ai, human_vs_ai (default)\n";
            std::cout << "  --team0 <type>      Team 0 type: human, ai_python, ai_rule (default: human)\n";
            std::cout << "  --team1 <type>      Team 1 type: human, ai_python, ai_rule (default: ai_rule)\n";
            std::cout << "  --help, -h          Show this help message\n\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " --mode ai_vs_ai --team0 ai_python --team1 ai_rule\n";
            std::cout << "  " << argv[0] << " --mode training --team0 ai_python --team1 ai_rule\n";
            exit(0);
        }
    }
    
    return config;
}

int main(int argc, char* argv[]) {
    try {
        // 切换到项目根目录（可执行文件的上一级目录）
        // 这样相对路径 python/infer.py 就能正确找到
        char exePath[PATH_MAX];
        uint32_t size = sizeof(exePath);
        if (_NSGetExecutablePath(exePath, &size) == 0) {
            char* dirPath = dirname(exePath);  // 获取目录部分
            char* parentPath = dirname(dirPath);  // 再上一级
            if (chdir(parentPath) == 0) {
                std::cout << "Working directory: " << parentPath << std::endl;
            }
        }
        
        // 注册信号处理函数
        std::signal(SIGINT, signalHandler);
        
        // 解析命令行参数
        GameConfig config = parseArgs(argc, argv);
        
        std::cout << "Strategy Game Starting..." << std::endl;
        std::cout << "Mode: " << gameModeToString(config.mode) << std::endl;
        std::cout << "Team 0: " << playerTypeToString(config.team0) << std::endl;
        std::cout << "Team 1: " << playerTypeToString(config.team1) << std::endl;
        
        // 创建MVC组件
        std::cout << "Creating Model..." << std::endl;
        auto model = std::make_shared<GameModel>();
        
        std::cout << "Creating Controller..." << std::endl;
        auto controller = std::make_shared<GameController>(model, config.mode, config.team0, config.team1);
        g_controller = controller;  // 保存到全局变量，供信号处理使用
        
        // 只在非训练模式下创建View
        std::shared_ptr<GameView> view = nullptr;
        if (config.mode != GameMode::TRAINING) {
            std::cout << "Creating View..." << std::endl;
            view = std::make_shared<GameView>(model, controller);
        }
        
        // 启动游戏控制器
        std::cout << "Starting Game Controller..." << std::endl;
        controller->start();
        std::cout << "Game Controller Started!" << std::endl;
        
        // 主循环
        std::cout << "Entering main loop..." << std::endl;
        if (view) {
            // 有渲染的模式
            while (view->isOpen() && !model->isGameOver()) {
                view->handleEvents();
                view->render();
            }
            
            // 游戏结束后继续显示结果
            std::cout << "Game ended, showing results..." << std::endl;
            while (view->isOpen() && model->isGameOver()) {
                view->handleEvents();
                view->render();
            }
        } else {
            // 训练模式：无渲染，等待游戏结束（不sleep，让Controller全速运行）
            int lastReportedTurn = 0;
            while (!model->isGameOver() && !g_interrupted) {
                int currentTurn = controller->getCurrentTurn();
                // 每1000回合报告一次进度
                if (currentTurn > 0 && currentTurn % 1000 == 0 && currentTurn != lastReportedTurn) {
                    std::cout << "Turn " << currentTurn << " - Game still running..." << std::endl;
                    lastReportedTurn = currentTurn;
                }
                // 短暂让出CPU，避免100%占用
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // 如果是被中断，显示提示
            if (g_interrupted) {
                std::cout << "Training interrupted by user. Data has been saved." << std::endl;
            }
        }
        
        // 停止游戏控制器
        std::cout << "Stopping Controller..." << std::endl;
        controller->stop();
        g_controller = nullptr;  // 清除全局引用
        std::cout << "Game Over!" << std::endl;
        
        if (model->isGameOver()) {
            std::string winner = (model->getWinner() == Team::TEAM_A) ? "Team A" : "Team B";
            std::cout << winner << " Wins!" << std::endl;
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred!" << std::endl;
        return 1;
    }
}