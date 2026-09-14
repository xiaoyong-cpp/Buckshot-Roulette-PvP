#include "Logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>

Logger& Logger::Instance() {
    static Logger sInstance;  // C++11 保证线程安全的局部静态变量
    return sInstance;
}

bool Logger::Init(const std::string& dir) {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        std::filesystem::create_directories(dir);
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &t);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
        std::string path = dir + "/game_" + buf + ".log";
        mStream.open(path, std::ios::app);
        mReady = mStream.is_open();
        if (mReady) {
            mStream << "==== 恶魔轮盘 日志初始化 ====" << std::endl;
            mStream.flush();
        }
        return mReady;
    } catch (const std::exception&) {
        return false;
    }
}

void Logger::Log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mReady) return;
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);
    const char* tag = "INFO";
    switch (level) {
        case LogLevel::Debug: tag = "DEBUG"; break;
        case LogLevel::Info: tag = "INFO"; break;
        case LogLevel::Warn: tag = "WARN"; break;
        case LogLevel::Error: tag = "ERROR"; break;
    }
    mStream << "[" << timeBuf << "] [" << tag << "] " << msg << std::endl;
    mStream.flush();
}
