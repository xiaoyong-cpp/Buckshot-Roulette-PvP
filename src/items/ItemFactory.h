// ============================================================================
// ItemFactory.h - 道具工厂（设计模式：工厂模式 + 单例模式）
//
// 道具类型通过 ItemRegistrar 静态对象自注册（见 Items.cpp），
// 工厂再读取 items_config.json 获取显示名称 / 抽取权重 / 模式限制。
// 新增道具 = 新类 + 配置文件登记，零侵入（开闭原则）。
// ============================================================================
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../utils/Random.h"

class IItemStrategy;

// 道具定义（来自 items_config.json）
struct ItemDef {
    std::string type;   // 类型标识
    std::string name;   // 显示名称（中文）
    int weight = 1;     // 发放权重
    bool pvpOnly = false;  // 仅 PVP 模式出现
    bool needsTarget = false;  // 使用时需要指定目标
};

// 自注册辅助类：在具体道具 cpp 中声明静态实例即完成注册
class ItemRegistrar {
public:
    ItemRegistrar(const std::string& type,
                  std::unique_ptr<IItemStrategy> (*creator)(),
                  const std::string& defaultName, int defaultWeight, bool pvpOnly);
};

class ItemFactory {
public:
    static ItemFactory& Instance();  // 单例模式

    // 读取 items_config.json（文件不存在时自动生成默认配置）
    void Init(const std::string& configPath = "items_config.json");

    // 创建策略实例（未知类型返回 nullptr）
    std::unique_ptr<IItemStrategy> Create(const std::string& type) const;

    // 按权重随机抽取一个可用于当前模式的道具类型（modePvp=false 时过滤 pvpOnly）
    std::string RandomType(Random& rng, bool pvp) const;

    const ItemDef* FindDef(const std::string& type) const;
    const std::vector<ItemDef>& Defs() const { return mDefs; }
    std::string DisplayName(const std::string& type) const;

    // 收集所有已注册类型（供字体加载用）
    std::string AllNamesText() const;

private:
    ItemFactory() = default;
    struct Entry {
        std::unique_ptr<IItemStrategy> (*creator)();
        std::string defaultName;
        int defaultWeight;
        bool pvpOnly;
    };
    std::map<std::string, Entry> mCreators;   // type -> 创建器
    std::vector<ItemDef> mDefs;               // 配置（含名称与权重）
    bool mConfigLoaded = false;

    friend class ItemRegistrar;
    void Register(const std::string& type, Entry entry);
};
