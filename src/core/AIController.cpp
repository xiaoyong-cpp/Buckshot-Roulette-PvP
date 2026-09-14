#include "AIController.h"

#include "Game.h"

AIAction AIController::Decide(const Game& game, int aiPlayerId, Random& rng) {
    const Player* p = game.FindPlayer(aiPlayerId);
    if (!p) {
        AIAction a;
        a.type = AIAction::Type::Shoot;
        a.targetId = -1;
        return a;
    }
    const Chamber& ch = game.GetChamber();
    bool known = p->knownShellValid && !ch.IsEmpty();

    // 1) 未知当前弹且有放大镜 -> 窥探
    if (!known && !ch.IsEmpty() && p->HasItem("magnifier")) {
        AIAction a;
        a.type = AIAction::Type::UseItem;
        a.item = "magnifier";
        return a;
    }

    // 2) 已知实弹 + 手锯 + 尚未锯断 -> 锯断（搭配实弹使用）
    if (known && p->knownShellLive && p->HasItem("saw") && !ch.sawed) {
        AIAction a;
        a.type = AIAction::Type::UseItem;
        a.item = "saw";
        return a;
    }

    // 3) 弹未知且血量低（<= 一半）+ 啤酒 -> 退掉一发未知弹
    if (!known && p->hp <= p->maxHp / 2 && p->HasItem("beer") && !ch.IsEmpty()) {
        AIAction a;
        a.type = AIAction::Type::UseItem;
        a.item = "beer";
        return a;
    }

    // 3.5) 濒死（<=1）+ 香烟 -> 抽烟自救（保守使用：多人局回血过强会导致对局僵持）
    if (p->hp <= 1 && p->HasItem("cigarette")) {
        AIAction a;
        a.type = AIAction::Type::UseItem;
        a.item = "cigarette";
        return a;
    }

    // 3.6) 血量危急（<= 1）+ 过期药品 -> 赌命（50% 回 2 血或扣 1 血）
    if (p->hp <= 1 && p->HasItem("expiredmed")) {
        AIAction a;
        a.type = AIAction::Type::UseItem;
        a.item = "expiredmed";
        return a;
    }

    // 选一个对手：单机为人类玩家，PVP 为随机存活他人
    int targetId = -1;
    std::vector<int> others;
    for (const auto& other : game.Players())
        if (other.alive && other.id != aiPlayerId) others.push_back(other.id);
    if (!others.empty()) targetId = others[(size_t)rng.GetInt(0, (int)others.size() - 1)];

    // 3.7) 已知空弹 + 反转器 + 有对手 -> 反转成实弹射对手
    if (known && !p->knownShellLive && p->HasItem("reverser") && !ch.IsEmpty() && targetId != -1) {
        AIAction a;
        a.type = AIAction::Type::UseItem;
        a.item = "reverser";
        return a;
    }

    AIAction a;
    a.type = AIAction::Type::Shoot;
    if (known) {
        // 4) 已知信息：实弹打对手，空弹打自己
        a.targetId = p->knownShellLive ? targetId : -1;
    } else {
        // 5) 无信息：60% 射自己（赌空弹），40% 射对手
        a.targetId = rng.Chance(0.6) ? -1 : targetId;
    }
    return a;
}
