// ============================================================================
// ModeFactory.h - 模式工厂（设计模式：工厂模式 + 单例模式）
// 模式自注册 + 从 config.json 动态加载主菜单按钮（开闭原则）
// ============================================================================
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

class IMode;

// 自注册辅助：在具体模式 cpp 中声明静态实例即完成注册
class ModeRegistrar {
public:
    ModeRegistrar(const std::string& id, std::shared_ptr<IMode> instance);
};

class ModeFactory {
public:
    static ModeFactory& Instance();  // 单例模式

    // 从 config.json 的 "modes" 数组加载启用的模式
    void Init();

    // 获取模式实例（未注册返回 nullptr）
    std::shared_ptr<IMode> Get(const std::string& id) const;

    // 菜单显示用的模式 id 列表（按配置顺序）
    const std::vector<std::string>& EnabledIds() const { return mEnabled; }

    // 收集所有模式名称（供字体加载）
    std::string AllNamesText() const;

private:
    ModeFactory() = default;
    std::map<std::string, std::shared_ptr<IMode>> mModes;
    std::vector<std::string> mEnabled;
    friend class ModeRegistrar;
};
