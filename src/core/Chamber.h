// ============================================================================
// Chamber.h - 弹仓（转轮手枪弹仓状态机）
// 使用 std::array<bool, 8> 表示 8 格弹仓中每一发是否为实弹，
// 指针 chamberIndex 指向当前待击发的弹槽（对应设计文档"弹仓状态机"）
// ============================================================================
#pragma once

#include <array>

#include "../utils/Random.h"

class Chamber {
public:
    static constexpr int CAPACITY = 8;

    // 装弹：填入 live 发实弹与 blank 发空弹并洗牌（live+blank <= CAPACITY）
    void Load(int live, int blank, Random& rng);

    // 当前弹槽是否为实弹（弹仓为空时行为未定义，调用前先检查 IsEmpty）
    bool CurrentIsLive() const { return mShells[(size_t)mIndex]; }

    bool IsEmpty() const { return mIndex >= mCount; }

    // 击发：消耗当前弹槽并转轮，返回该弹是否为实弹；击发会重置手锯效果
    bool Fire();

    // 退弹（啤酒）：消耗当前弹槽并转轮，返回该弹是否为实弹；不重置手锯
    bool Eject();

    // 反转（反转器）：翻转当前弹槽的实/空属性（原地修改，影响下一发判定与剩余计数）
    // 弹仓为空时无效果
    void ReverseCurrent();

    // 剩余弹数 / 剩余实弹 / 剩余空弹（剩余实空数为公开信息，可广播）
    int Remaining() const { return mCount - mIndex; }
    int LiveRemaining() const;
    int BlankRemaining() const { return Remaining() - LiveRemaining(); }

    int Count() const { return mCount; }
    int Index() const { return mIndex; }

    bool sawed = false;  // 手锯：下一次击发伤害翻倍

private:
    std::array<bool, CAPACITY> mShells{};  // true = 实弹(Live)，false = 空弹(Blank)
    int mCount = 0;                        // 本次装填的弹数
    int mIndex = 0;                        // 当前弹槽指针
};
