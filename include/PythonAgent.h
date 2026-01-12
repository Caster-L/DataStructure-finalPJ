#ifndef PYTHONAGENT_H
#define PYTHONAGENT_H

#include <string>
#include <memory>
#include <cstdio>

// 简化的JSON类型
using JsonString = std::string;

// Python代理类 - 通过进程通信与Python脚本交互
class PythonAgent {
private:
    FILE* pythonProcess;
    FILE* pythonInput;
    std::string scriptPath;
    bool initialized;
    
public:
    PythonAgent();
    ~PythonAgent();
    
    // 初始化Python进程
    bool initialize(const std::string& scriptPath);
    
    // 获取AI决策（发送状态JSON，接收动作JSON）
    JsonString getAction(const JsonString& stateJson);
    
    // 关闭Python进程
    void shutdown();
    
    bool isInitialized() const { return initialized; }
};

#endif // PYTHONAGENT_H
