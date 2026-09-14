// ============================================================================
// Logger.h - 日志单例（设计模式：单例模式）
// 输出全部写入 logs/ 目录下的文件，不占用控制台（零命令行 UI 要求）
// ============================================================================
#pragma once

#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& Instance();  // 单例模式：全局唯一实例

    bool Init(const std::string& dir = "logs");
    void Log(LogLevel level, const std::string& msg);
    void Debug(const std::string& msg) { Log(LogLevel::Debug, msg); }
    void Info(const std::string& msg) { Log(LogLevel::Info, msg); }
    void Warn(const std::string& msg) { Log(LogLevel::Warn, msg); }
    void Error(const std::string& msg) { Log(LogLevel::Error, msg); }

private:
    Logger() = default;
    std::ofstream mStream;
    std::mutex mMutex;  // 网络线程与主线程都会写日志
    bool mReady = false;
};
