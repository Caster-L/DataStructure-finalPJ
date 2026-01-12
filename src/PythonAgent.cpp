#include "../include/PythonAgent.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <climits>

// 读取Python解释器路径配置
static std::string getPythonPath() {
    std::ifstream configFile(".python_path");
    if (configFile.is_open()) {
        std::string path;
        std::getline(configFile, path);
        configFile.close();
        // 去除前后空格和换行符
        path.erase(0, path.find_first_not_of(" \t\n\r"));
        path.erase(path.find_last_not_of(" \t\n\r") + 1);
        if (!path.empty()) {
            std::cout << "Using Python from config: " << path << std::endl;
            return path;
        }
    }
    // 默认使用python3
    return "python3";
}

PythonAgent::PythonAgent() 
    : pythonProcess(nullptr), pythonInput(nullptr), initialized(false) {
}

PythonAgent::~PythonAgent() {
    shutdown();
}

bool PythonAgent::initialize(const std::string& path) {
    // 转换为绝对路径
    char absPath[PATH_MAX];
    if (realpath(path.c_str(), absPath) != nullptr) {
        scriptPath = absPath;
    } else {
        // 如果 realpath 失败，尝试拼接当前目录
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            scriptPath = std::string(cwd) + "/" + path;
        } else {
            scriptPath = path;  // 降级使用原始路径
        }
    }
    
    std::cout << "Python agent initialized: " << scriptPath << std::endl;
    
    // 获取Python解释器路径
    std::string pythonCmd = getPythonPath();
    
    // 使用popen启动Python进程
    std::string command = pythonCmd + " " + scriptPath;
    
    pythonProcess = popen(command.c_str(), "w");
    if (!pythonProcess) {
        std::cerr << "Failed to start Python process: " << scriptPath << std::endl;
        return false;
    }
    
    initialized = true;
    std::cout << "Python agent initialized: " << scriptPath << std::endl;
    return true;
}

JsonString PythonAgent::getAction(const JsonString& stateJson) {
    if (!initialized) {
        std::cerr << "Python agent not initialized!" << std::endl;
        return "{\"action_type\": 0, \"base_id\": -1, \"unit_type\": -1}";
    }

    // 写入状态
    std::ofstream stateFile("/tmp/game_state.json");
    stateFile << stateJson;
    stateFile.close();
    
    // 调用Python推理
    std::string pythonCmd = getPythonPath();
    // 将stderr重定向到文件以便调试，避免丢弃
    std::string command = pythonCmd + " " + scriptPath + " /tmp/game_state.json > /tmp/game_action.json 2>/tmp/python_stderr.log";

    std::cerr << "[DEBUG PythonAgent] Command: " << command << std::endl;
    int result = system(command.c_str());
    std::cerr << "[DEBUG PythonAgent] System result: " << result << std::endl;

    // 读取stderr日志（如果有错误）
    std::ifstream stderrFile("/tmp/python_stderr.log");
    if (stderrFile.is_open()) {
        std::string errorContent((std::istreambuf_iterator<char>(stderrFile)),
                                std::istreambuf_iterator<char>());
        if (!errorContent.empty()) {
            std::cerr << "[DEBUG PythonAgent] Python stderr: " << errorContent << std::endl;
        }
        stderrFile.close();
    }
    
    if (result != 0) {
        std::cerr << "Python inference failed with code: " << result << std::endl;
        return "{\"action_type\": 0, \"base_id\": -1, \"unit_type\": -1}";
    }
    
    // 读取动作
    std::ifstream actionFile("/tmp/game_action.json");
    if (!actionFile.is_open()) {
        std::cerr << "[DEBUG PythonAgent] Failed to open action file!" << std::endl;
        return "{\"action_type\": 0, \"base_id\": -1, \"unit_type\": -1}";
    }
    
    std::stringstream buffer;
    buffer << actionFile.rdbuf();
    std::string actionStr = buffer.str();
    std::cerr << "[DEBUG PythonAgent] Read action: " << actionStr << " (length=" << actionStr.length() << ")" << std::endl;
    
    return actionStr;
}

void PythonAgent::shutdown() {
    if (pythonProcess) {
        pclose(pythonProcess);
        pythonProcess = nullptr;
    }
    initialized = false;
}
