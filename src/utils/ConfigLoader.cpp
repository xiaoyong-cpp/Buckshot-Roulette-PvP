#include "ConfigLoader.h"

#include <fstream>
#include <iostream>

#include "Logger.h"

ConfigLoader& ConfigLoader::Instance() {
    static ConfigLoader sInstance;
    return sInstance;
}

bool ConfigLoader::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        // 首次运行：生成默认配置文件（开箱即用，用户可自行修改）
        mJson = nlohmann::json{
            {"window", {{"width", 1280}, {"height", 720}}},
            {"game",
             {{"defaultHp", 4},
              {"itemsPerRoundMin", 0},
              {"itemsPerRoundMax", 5},
              {"maxPlayers", 4},
              {"maxPlayersCap", 8},
              {"reconnectTimeout", 60}}},
            {"network", {{"port", 7777}, {"syncHz", 10}}},
            {"font", {{"size", 32}}},
            {"modes", nlohmann::json::array({"single", "multiplayer"})}};
        Save(path);
        mLoaded = true;
        return true;
    }
    try {
        file >> mJson;
        mLoaded = true;
        return true;
    } catch (const std::exception& e) {
        Logger::Instance().Error(std::string("配置解析失败: ") + e.what() + "，使用默认配置");
        mJson = nlohmann::json::object();
        return false;
    }
}

bool ConfigLoader::Save(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << mJson.dump(2);
    return true;
}
