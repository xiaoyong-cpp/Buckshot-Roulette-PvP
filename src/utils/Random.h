// ============================================================================
// Random.h - 随机数工具（线程安全的 MT19937 封装）
// ============================================================================
#pragma once

#include <random>
#include <string>
#include <vector>

// 随机数封装：全游戏共用一个引擎实例（由 Game 持有），
// 保证服务器与本地逻辑使用同一套随机序列源。
class Random {
public:
    Random();
    explicit Random(unsigned int seed);

    void Seed(unsigned int seed);

    // 返回 [min, max] 闭区间内的整数
    int GetInt(int min, int max);

    // 返回 [0.0, 1.0) 的浮点数
    double GetDouble();

    // 以概率 p 返回 true
    bool Chance(double p);

    // 从列表中随机挑选一个（列表为空返回 nullptr）
    template <typename T>
    T* Pick(std::vector<T>& list) {
        if (list.empty()) return nullptr;
        return &list[(size_t)GetInt(0, (int)list.size() - 1)];
    }

    // Fisher-Yates 洗牌
    template <typename T>
    void Shuffle(std::vector<T>& list) {
        for (int i = (int)list.size() - 1; i > 0; --i) {
            int j = GetInt(0, i);
            std::swap(list[i], list[j]);
        }
    }

    // 生成随机十六进制 token（用于断线重连验证）
    std::string Token(int bytes = 8);

private:
    std::mt19937 mEngine;
};
