// ============================================================================
// Player.h - 玩家数据（生命值、道具背包、私有情报等）
// ============================================================================
#pragma once

#include <string>
#include <vector>

struct Player {
    int id = -1;                 // 唯一编号（主机分配，按加入顺序）
    std::string name;            // 昵称（限制 ASCII，便于字体渲染）
    int hp = 4;                  // 当前生命值
    int maxHp = 4;               // 生命值上限（可配置）
    bool alive = true;           // 是否存活
    bool isAI = false;           // 是否为 AI（单机恶魔 / 掉线托管）
    bool connected = true;       // 是否在线（PVP 掉线时置 false 并托管）
    std::vector<std::string> items;  // 背包：道具类型字符串列表

    bool skipNextTurn = false;   // 手铐：下回合被跳过
    double disconnectedAt = 0.0; // 掉线时间戳（秒）

    // 私有情报：放大镜看到的"当前弹槽"信息（仅自己可见，不随 state 广播）
    bool knownShellValid = false;  // 是否已窥探当前弹槽
    bool knownShellLive = false;   // 窥探结果：true=实弹

    bool HasItem(const std::string& type) const;
    bool RemoveItem(const std::string& type);
    int ItemCount() const { return (int)items.size(); }
};
