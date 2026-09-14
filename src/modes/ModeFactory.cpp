#include "ModeFactory.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "IMode.h"  // 需要完整类型以调用 IMode::GetName（ModeFactory.h 仅为前向声明）
#include "../utils/ConfigLoader.h"
#include "../utils/Logger.h"

ModeRegistrar::ModeRegistrar(const std::string& id, std::shared_ptr<IMode> instance) {
    ModeFactory::Instance().mModes[id] = instance;  // 友元访问（见 ModeFactory.h）
}

ModeFactory& ModeFactory::Instance() {
    static ModeFactory sInstance;
    return sInstance;
}

void ModeFactory::Init() {
    mEnabled.clear();
    auto& cfg = ConfigLoader::Instance().Data();
    if (cfg.contains("modes") && cfg["modes"].is_array()) {
        for (const auto& id : cfg["modes"]) {
            std::string s = id.get<std::string>();
            if (mModes.count(s)) mEnabled.push_back(s);
        }
    }
    // 兜底：若配置为空，启用所有已注册模式
    if (mEnabled.empty()) {
        for (const auto& [id, mode] : mModes) mEnabled.push_back(id);
    }
    for (const auto& id : mEnabled)
        Logger::Instance().Info("启用模式: " + id + " (" + mModes[id]->GetName() + ")");
}

std::shared_ptr<IMode> ModeFactory::Get(const std::string& id) const {
    auto it = mModes.find(id);
    return it != mModes.end() ? it->second : nullptr;
}

std::string ModeFactory::AllNamesText() const {
    std::string s;
    for (const auto& [id, mode] : mModes) s += mode->GetName();
    return s;
}
