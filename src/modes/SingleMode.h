// ============================================================================
// SingleMode.h - 单机模式（PVE，玩家 vs 恶魔 AI）
// 装弹策略：每局固定 6 发，实弹 1~4 发随回合数增加（设计文档 2.1 节）
// ============================================================================
#pragma once

#include "IMode.h"

class SingleMode : public IMode {
public:
    std::string GetId() const override { return "single"; }
    std::string GetName() const override { return "单机模式"; }
    bool IsPvp() const override { return false; }

    // 总数固定 6 发；实弹数 = clamp(1 + (round-1)/2 + 随机扰动, 1, 4)
    ReloadInfo GenerateReload(int round, Random& rng) const override {
        int base = 1 + (round - 1) / 2;
        int live = std::min(4, base + rng.GetInt(0, 1));
        if (live < 1) live = 1;
        int blank = 6 - live;
        return {live, blank};
    }
};
