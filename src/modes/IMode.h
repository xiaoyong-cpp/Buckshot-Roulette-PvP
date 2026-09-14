// ============================================================================
// IMode.h - 游戏模式接口（设计模式：策略模式 + 开闭原则）
//
// 【新增模式】实现 IMode 接口 + 用 ModeRegistrar 自注册，
// 再在 config.json 的 "modes" 数组登记即可出现在主菜单，无需修改其它文件。
// ============================================================================
#pragma once

#include <string>

#include "../utils/Random.h"

// 装弹信息：一次装弹的实弹/空弹数量
struct ReloadInfo {
    int live = 1;
    int blank = 1;
};

class IMode {
public:
    virtual ~IMode() = default;

    virtual std::string GetId() const = 0;    // 模式标识（配置文件用）
    virtual std::string GetName() const = 0;  // 主菜单显示名

    // 装弹策略：根据模式与当前回合数随机生成实/空弹数量（<= Chamber::CAPACITY）
    virtual ReloadInfo GenerateReload(int round, Random& rng) const = 0;

    // 是否为 PVP 模式（决定手铐/肾上腺素是否可用）
    virtual bool IsPvp() const = 0;
};
