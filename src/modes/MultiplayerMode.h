// ============================================================================
// MultiplayerMode.h - 联网模式（PVP，2~8 人）
// 装弹策略：实弹数随回合数提升（2 起），空弹随机补足至 8 格上限
// ============================================================================
#pragma once

#include "IMode.h"

class MultiplayerMode : public IMode {
public:
    std::string GetId() const override { return "multiplayer"; }
    std::string GetName() const override { return "联网模式"; }
    bool IsPvp() const override { return true; }

    // 实弹 max = min(8, 2 + (round-1))；空弹随机 1..(8-live)
    ReloadInfo GenerateReload(int round, Random& rng) const override {
        int maxLive = std::min(8, 2 + (round - 1));
        if (maxLive < 2) maxLive = 2;
        int live = rng.GetInt(2, maxLive);
        int maxBlank = 8 - live;
        int blank = maxBlank > 0 ? rng.GetInt(1, maxBlank) : 0;
        return {live, blank};
    }
};
