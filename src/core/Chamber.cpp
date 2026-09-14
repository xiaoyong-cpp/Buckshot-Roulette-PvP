#include "Chamber.h"

void Chamber::Load(int live, int blank, Random& rng) {
    if (live < 0) live = 0;
    if (blank < 0) blank = 0;
    if (live + blank > CAPACITY) blank = CAPACITY - live;  // 防御性截断

    mShells.fill(false);
    mCount = live + blank;
    mIndex = 0;
    sawed = false;  // 重新装弹后手锯效果清空

    for (int i = 0; i < live; ++i) mShells[(size_t)i] = true;
    // Fisher-Yates 洗牌：保证弹序随机
    for (int i = mCount - 1; i > 0; --i) {
        int j = rng.GetInt(0, i);
        std::swap(mShells[(size_t)i], mShells[(size_t)j]);
    }
}

bool Chamber::Fire() {
    if (IsEmpty()) return false;
    bool isLive = mShells[(size_t)mIndex];
    ++mIndex;
    sawed = false;  // 手锯只作用于下一次"击发"
    return isLive;
}

bool Chamber::Eject() {
    if (IsEmpty()) return false;
    bool isLive = mShells[(size_t)mIndex];
    ++mIndex;  // 退弹不重置手锯：锯断效果随下一次击发生效
    return isLive;
}

void Chamber::ReverseCurrent() {
    if (IsEmpty()) return;
    mShells[(size_t)mIndex] = !mShells[(size_t)mIndex];  // 原地翻转当前弹槽
}

int Chamber::LiveRemaining() const {
    int n = 0;
    for (int i = mIndex; i < mCount; ++i)
        if (mShells[(size_t)i]) ++n;
    return n;
}
