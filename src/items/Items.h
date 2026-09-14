// ============================================================================
// Items.h - 具体道具策略实现（设计模式：策略模式的具体策略类）
// 每个道具继承 IItemStrategy；通过 ItemRegistrar 自注册到工厂，新增道具
// 无需改动 Game/UI/网络。本文件实现 MVP 4 种 + 扩展肾上腺素共 5 种道具。
// ============================================================================
#pragma once

#include "IItemStrategy.h"

// 1. 放大镜：查看当前弹槽是实弹还是空弹（仅对自己可见）
class MagnifierStrategy : public IItemStrategy {
public:
    std::string GetType() const override { return "magnifier"; }
    void Apply(GameContext& ctx, Player* user, Player* target) override;
};

// 2. 手锯：下一发击发伤害翻倍（若击发的是空弹则无效并消耗）
class SawStrategy : public IItemStrategy {
public:
    std::string GetType() const override { return "saw"; }
    void Apply(GameContext& ctx, Player* user, Player* target) override;
};

// 3. 啤酒：退掉当前弹槽（不消耗回合，该弹丢弃并公示）
class BeerStrategy : public IItemStrategy {
public:
    std::string GetType() const override { return "beer"; }
    void Apply(GameContext& ctx, Player* user, Player* target) override;
};

// 4. 手铐（仅 PVP）：使目标下回合被跳过
class HandcuffStrategy : public IItemStrategy {
public:
    std::string GetType() const override { return "handcuff"; }
    bool NeedsTarget() const override { return true; }
    void Apply(GameContext& ctx, Player* user, Player* target) override;
};

// 5. 肾上腺素（仅 PVP）：偷取目标一个随机道具
class AdrenalineStrategy : public IItemStrategy {
public:
    std::string GetType() const override { return "adrenaline"; }
    bool NeedsTarget() const override { return true; }
    void Apply(GameContext& ctx, Player* user, Player* target) override;
};

// 6. 香烟：恢复 1 点生命值（不能超过上限；吸烟有害健康）
class CigaretteStrategy : public IItemStrategy {
public:
    std::string GetType() const override { return "cigarette"; }
    void Apply(GameContext& ctx, Player* user, Player* target) override;
};

// 7. 过期药品：1/2 概率回复 2 血，1/2 概率扣 1 血（扣血可能致死并触发淘汰）
class ExpiredMedStrategy : public IItemStrategy {
public:
    std::string GetType() const override { return "expiredmed"; }
    void Apply(GameContext& ctx, Player* user, Player* target) override;
};

// 8. 反转器：翻转当前弹槽的实/空属性（影响下一发判定与剩余计数）
class ReverserStrategy : public IItemStrategy {
public:
    std::string GetType() const override { return "reverser"; }
    void Apply(GameContext& ctx, Player* user, Player* target) override;
};
