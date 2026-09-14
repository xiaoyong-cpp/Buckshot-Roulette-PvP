#include "Items.h"

#include <algorithm>

#include "ItemFactory.h"

#include "../utils/Logger.h"

// ============================================================================
// 放大镜：把"当前弹槽信息"写入使用者的私有情报，
// 同时发出 MagnifierReveal 私有事件（仅本人可见，网络层不广播）。
// ============================================================================
void MagnifierStrategy::Apply(GameContext& ctx, Player* user, Player* /*target*/) {
    bool live = ctx.chamber.CurrentIsLive();
    user->knownShellValid = true;
    user->knownShellLive = live;
    GameEvent e;
    e.kind = EventKind::MagnifierReveal;
    e.playerId = user->id;
    e.flag = live;
    ctx.sink.EmitEvent(e);
}

// ============================================================================
// 手锯：将下一发击发的伤害提升为 2（仅对下一次"击发"生效，空弹则浪费）。
// ============================================================================
void SawStrategy::Apply(GameContext& ctx, Player* user, Player* /*target*/) {
    ctx.chamber.sawed = true;
    Logger::Instance().Info("玩家 " + std::to_string(user->id) + " 锯断了枪管");
}

// ============================================================================
// 啤酒：退掉当前弹槽并公示其内容；不消耗回合。
// 退弹后弹槽推进，所有玩家的窥探情报失效（GameContext 通过事件隐式表现）。
// ============================================================================
void BeerStrategy::Apply(GameContext& ctx, Player* user, Player* /*target*/) {
    bool live = ctx.chamber.Eject();
    // 弹槽推进后清除所有玩家的窥探情报
    for (auto& p : ctx.players) {
        p.knownShellValid = false;
        p.knownShellLive = false;
    }
    GameEvent e;
    e.kind = EventKind::BeerEject;
    e.playerId = user->id;
    e.flag = live;
    ctx.sink.EmitEvent(e);
}

// ============================================================================
// 手铐：目标下回合被跳过。
// ============================================================================
void HandcuffStrategy::Apply(GameContext& ctx, Player* /*user*/, Player* target) {
    if (target) target->skipNextTurn = true;
}

// ============================================================================
// 肾上腺素：从目标背包偷取一个随机道具（背包已满则丢弃并提示）。
// ============================================================================
void AdrenalineStrategy::Apply(GameContext& ctx, Player* user, Player* target) {
    if (!target || target->items.empty()) return;
    int idx = ctx.rng.GetInt(0, (int)target->items.size() - 1);
    std::string stolen = target->items[(size_t)idx];
    target->items.erase(target->items.begin() + (long)idx);

    // 背包已满：自动丢弃（轻量实现，详见 README）
    if ((int)user->items.size() >= ctx.maxItems) {
        GameEvent info;
        info.kind = EventKind::Info;
        info.playerId = user->id;
        info.text = stolen;
        ctx.sink.EmitEvent(info);
    } else {
        user->items.push_back(stolen);
    }

    GameEvent e;
    e.kind = EventKind::Steal;
    e.playerId = user->id;
    e.targetId = target->id;
    e.text = stolen;
    ctx.sink.EmitEvent(e);
}

// ============================================================================
// 香烟：恢复 1 点生命值（不超过上限）。
// 注：吸烟有害健康。本道具仅为游戏机制，不鼓励现实中的吸烟行为。
// ============================================================================
void CigaretteStrategy::Apply(GameContext& ctx, Player* user, Player* /*target*/) {
    if (!user) return;
    if (user->hp < user->maxHp) {
        user->hp += 1;
        GameEvent e;
        e.kind = EventKind::PlayerHealed;
        e.playerId = user->id;
        e.targetId = user->id;
        e.value = 1;
        e.flag = true;  // 正面效果
        ctx.sink.EmitEvent(e);
    } else {
        // 满血时使用：提示无效（不消耗回合，但消耗道具）
        GameEvent e;
        e.kind = EventKind::Info;
        e.playerId = user->id;
        e.text = "满血，香烟无效果";
        ctx.sink.EmitEvent(e);
    }
}

// ============================================================================
// 过期药品：1/2 概率回复 2 血，1/2 概率扣 1 血（扣血可能致死）。
// 扣血致死的淘汰检测由 Game::UseItem 末尾的 CheckGameOver 统一处理。
// ============================================================================
void ExpiredMedStrategy::Apply(GameContext& ctx, Player* user, Player* /*target*/) {
    if (!user) return;
    bool good = ctx.rng.GetInt(0, 1) == 0;  // 50% 正面
    if (good) {
        int heal = std::min(2, user->maxHp - user->hp);
        if (heal > 0) {
            user->hp += heal;
            GameEvent e;
            e.kind = EventKind::PlayerHealed;
            e.playerId = user->id;
            e.targetId = user->id;
            e.value = heal;
            e.flag = true;
            e.text = "expiredmed_good";
            ctx.sink.EmitEvent(e);
        }
    } else {
        int dmg = 1;
        user->hp -= dmg;
        if (user->hp < 0) user->hp = 0;
        GameEvent e;
        e.kind = EventKind::PlayerDamaged;
        e.playerId = user->id;
        e.targetId = user->id;
        e.value = dmg;
        e.text = "expiredmed_bad";
        ctx.sink.EmitEvent(e);
        if (user->hp <= 0) {
            user->alive = false;
            GameEvent elim;
            elim.kind = EventKind::PlayerEliminated;
            elim.targetId = user->id;
            elim.text = "expiredmed_bad";
            ctx.sink.EmitEvent(elim);
        }
    }
}

// ============================================================================
// 反转器：翻转当前弹槽的实/空属性。
// 原地修改弹仓，影响下一发判定与剩余实/空计数（公开信息随之变化）。
// 反转后：使用者知道反转结果（主动反转），其他玩家的窥探情报失效。
// ============================================================================
void ReverserStrategy::Apply(GameContext& ctx, Player* user, Player* /*target*/) {
    bool wasLive = ctx.chamber.CurrentIsLive();
    ctx.chamber.ReverseCurrent();
    bool nowLive = !wasLive;
    // 其他玩家的窥探情报失效（信息已过期）
    for (auto& p : ctx.players) {
        if (p.id != user->id) {
            p.knownShellValid = false;
            p.knownShellLive = false;
        }
    }
    // 使用者知道反转结果（主动反转，知道当前弹性质已翻转）
    user->knownShellValid = true;
    user->knownShellLive = nowLive;

    GameEvent e;
    e.kind = EventKind::Info;
    e.playerId = user->id;
    e.text = wasLive ? "反转器：实弹→空弹" : "反转器：空弹→实弹";
    ctx.sink.EmitEvent(e);
    Logger::Instance().Info("玩家 " + std::to_string(user->id) + " 使用反转器，" +
                            (wasLive ? "实弹→空弹" : "空弹→实弹"));
}

// ============================================================================
// 自注册（开闭原则）：每个静态 ItemRegistrar 在程序启动时把对应类型注册进工厂。
// 新增道具只需在此追加一行注册即可。
// ============================================================================
static ItemRegistrar reg_magnifier("magnifier",
    []() -> std::unique_ptr<IItemStrategy> { return std::make_unique<MagnifierStrategy>(); },
    "放大镜", 3, false);
static ItemRegistrar reg_saw("saw",
    []() -> std::unique_ptr<IItemStrategy> { return std::make_unique<SawStrategy>(); },
    "手锯", 2, false);
static ItemRegistrar reg_beer("beer",
    []() -> std::unique_ptr<IItemStrategy> { return std::make_unique<BeerStrategy>(); },
    "啤酒", 3, false);
static ItemRegistrar reg_handcuff("handcuff",
    []() -> std::unique_ptr<IItemStrategy> { return std::make_unique<HandcuffStrategy>(); },
    "手铐", 2, true);
static ItemRegistrar reg_adrenaline("adrenaline",
    []() -> std::unique_ptr<IItemStrategy> { return std::make_unique<AdrenalineStrategy>(); },
    "肾上腺素", 2, true);
static ItemRegistrar reg_cigarette("cigarette",
    []() -> std::unique_ptr<IItemStrategy> { return std::make_unique<CigaretteStrategy>(); },
    "香烟", 3, false);
static ItemRegistrar reg_expiredmed("expiredmed",
    []() -> std::unique_ptr<IItemStrategy> { return std::make_unique<ExpiredMedStrategy>(); },
    "过期药品", 2, false);
static ItemRegistrar reg_reverser("reverser",
    []() -> std::unique_ptr<IItemStrategy> { return std::make_unique<ReverserStrategy>(); },
    "反转器", 2, false);
