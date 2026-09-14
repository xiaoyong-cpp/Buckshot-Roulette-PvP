// ============================================================================
// ConfigLoader.h - 配置管理单例（设计模式：单例模式）
// 读取/生成 config.json，所有游戏参数（生命值、道具上限、端口等）集中管理
// ============================================================================
#pragma once

#include <string>

#include <nlohmann/json.hpp>

class ConfigLoader {
public:
    static ConfigLoader& Instance();  // 单例模式

    bool Load(const std::string& path = "config.json");
    bool Save(const std::string& path = "config.json");

    // ---- 各类配置项（带默认值的访问器）----
    int WindowWidth() const { return mJson.value("/window/width"_json_pointer, 1280); }
    int WindowHeight() const { return mJson.value("/window/height"_json_pointer, 720); }
    int DefaultHp() const { return mJson.value("/game/defaultHp"_json_pointer, 4); }
    int ItemsPerRoundMin() const { return mJson.value("/game/itemsPerRoundMin"_json_pointer, 0); }
    int ItemsPerRoundMax() const { return mJson.value("/game/itemsPerRoundMax"_json_pointer, 2); }
    int DefaultMaxPlayers() const { return mJson.value("/game/maxPlayers"_json_pointer, 4); }
    int MaxPlayersCap() const { return mJson.value("/game/maxPlayersCap"_json_pointer, 8); }
    int ReconnectTimeout() const { return mJson.value("/game/reconnectTimeout"_json_pointer, 60); }
    int Port() const { return mJson.value("/network/port"_json_pointer, 7777); }
    int SyncHz() const { return mJson.value("/network/syncHz"_json_pointer, 10); }
    int FontSize() const { return mJson.value("/font/size"_json_pointer, 32); }

    nlohmann::json& Data() { return mJson; }

private:
    ConfigLoader() = default;
    nlohmann::json mJson;
    bool mLoaded = false;
};
