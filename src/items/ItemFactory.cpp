#include "ItemFactory.h"

#include <fstream>
#include <functional>

#include <nlohmann/json.hpp>

#include "IItemStrategy.h"
#include "../utils/Logger.h"

// ---- ItemRegistrar 实现：构造时把创建器注册到工厂 ----
ItemRegistrar::ItemRegistrar(const std::string& type,
                              std::unique_ptr<IItemStrategy> (*creator)(),
                              const std::string& defaultName, int defaultWeight, bool pvpOnly) {
    ItemFactory::Instance().Register(type, {creator, defaultName, defaultWeight, pvpOnly});
}

ItemFactory& ItemFactory::Instance() {
    static ItemFactory sInstance;
    return sInstance;
}

void ItemFactory::Register(const std::string& type, Entry entry) {
    mCreators[type] = std::move(entry);
}

void ItemFactory::Init(const std::string& configPath) {
    if (mConfigLoaded) return;

    nlohmann::json cfg;
    std::ifstream file(configPath);
    bool wroteDefault = false;
    if (!file.is_open()) {
        // 首次运行：以已注册类型生成默认配置文件（开箱即用）
        nlohmann::json itemsArr = nlohmann::json::array();
        for (const auto& [type, entry] : mCreators) {
            itemsArr.push_back({{"type", type},
                                {"name", entry.defaultName},
                                {"weight", entry.defaultWeight},
                                {"pvpOnly", entry.pvpOnly}});
        }
        cfg = {{"items", itemsArr}};
        std::ofstream out(configPath);
        if (out.is_open()) out << cfg.dump(2);
        wroteDefault = true;
        Logger::Instance().Info("未找到 items_config.json，已生成默认配置");
    } else {
        try {
            file >> cfg;
        } catch (const std::exception& e) {
            Logger::Instance().Error(std::string("items_config.json 解析失败: ") + e.what());
            cfg = nlohmann::json::object();
        }
    }

    // 装配道具定义：以配置文件中的条目为准，但只接受已注册的类型
    mDefs.clear();
    if (cfg.contains("items") && cfg["items"].is_array()) {
        for (const auto& it : cfg["items"]) {
            std::string type = it.value("type", "");
            if (mCreators.find(type) == mCreators.end()) {
                Logger::Instance().Warn("items_config.json 中的未知道具类型: " + type);
                continue;
            }
            ItemDef def;
            def.type = type;
            def.name = it.value("name", mCreators[type].defaultName);
            def.weight = it.value("weight", mCreators[type].defaultWeight);
            def.pvpOnly = it.value("pvpOnly", mCreators[type].pvpOnly);
            mDefs.push_back(def);
        }
    }
    // 兜底：若配置文件为空但注册了类型，补回默认
    if (mDefs.empty()) {
        for (const auto& [type, entry] : mCreators)
            mDefs.push_back({type, entry.defaultName, entry.defaultWeight, entry.pvpOnly, false});
    }
    // 从策略实例获取"是否需要目标"属性
    for (auto& d : mDefs) {
        auto s = Create(d.type);
        d.needsTarget = s ? s->NeedsTarget() : false;
    }
    mConfigLoaded = true;
    (void)wroteDefault;
}

std::unique_ptr<IItemStrategy> ItemFactory::Create(const std::string& type) const {
    auto it = mCreators.find(type);
    if (it == mCreators.end()) return nullptr;
    return it->second.creator();
}

std::string ItemFactory::RandomType(Random& rng, bool pvp) const {
    int total = 0;
    for (const auto& d : mDefs) {
        if (d.pvpOnly && !pvp) continue;
        total += d.weight;
    }
    if (total <= 0) return "";
    int r = rng.GetInt(0, total - 1);
    for (const auto& d : mDefs) {
        if (d.pvpOnly && !pvp) continue;
        r -= d.weight;
        if (r < 0) return d.type;
    }
    return "";
}

const ItemDef* ItemFactory::FindDef(const std::string& type) const {
    for (const auto& d : mDefs)
        if (d.type == type) return &d;
    return nullptr;
}

std::string ItemFactory::DisplayName(const std::string& type) const {
    const ItemDef* d = FindDef(type);
    return d ? d->name : type;
}

std::string ItemFactory::AllNamesText() const {
    std::string s;
    for (const auto& d : mDefs) s += d.name;
    return s;
}
