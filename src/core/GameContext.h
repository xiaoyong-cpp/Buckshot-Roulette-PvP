// ============================================================================
// GameContext.h - 道具作用上下文（策略模式的"上下文"参数）
// 道具策略通过该结构访问游戏能力：弹仓、玩家表、随机数、事件通知。
// 这样 IItemStrategy 无需依赖 Game 完整定义，保持 items/ 与 core/ 解耦。
// ============================================================================
#pragma once

#include <vector>

#include "Chamber.h"
#include "GameEvent.h"
#include "Player.h"
#include "../utils/Random.h"

// 背包容量上限（常数，8 格 4x2 网格布局，不再从配置读取）
inline constexpr int MAX_BACKPACK = 8;

// 事件出口：Game 实现此接口，把道具产生的事件（退弹、偷窃等）广播出去
class IEventSink {
public:
    virtual ~IEventSink() = default;
    virtual void EmitEvent(const GameEvent& event) = 0;
};

struct GameContext {
    Chamber& chamber;           // 共享弹仓
    std::vector<Player>& players;  // 玩家表
    Random& rng;                // 随机数
    IEventSink& sink;           // 事件出口
    bool pvp;                   // 当前是否 PVP 模式（决定手铐/肾上腺素可用性）
    int turnPlayerId;           // 当前行动玩家 id（= 使用者）
    int maxItems = MAX_BACKPACK;  // 背包容量上限（常量 8）
};
