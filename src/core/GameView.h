// ============================================================================
// GameView.h - 面向 UI 的只读游戏视图（纯数据模型）
// 主机端由 Game 逐帧构建；客户端由 state 消息解析填充。
// 渲染层只读取该结构，不接触权威逻辑。
// ============================================================================
#pragma once

#include <string>
#include <vector>

#include "GameState.h"

// 单个玩家的展示数据
struct ViewPlayer {
    int id = -1;
    std::string name;
    int hp = 0;
    int maxHp = 4;
    bool alive = true;
    bool connected = true;   // 掉线时为 false（AI 托管中）
    bool isTurn = false;     // 是否当前行动
    bool skipNext = false;   // 被手铐束缚
    int itemCount = 0;      // 道具数量（公开信息）
    std::vector<std::string> items;  // 道具明细（仅本人填充，他人为空）
    bool isSelf = false;     // 是否是本地玩家自己
};

struct GameView {
    bool valid = false;
    bool pvp = false;
    GameState state = GameState::Waiting;
    int round = 0;
    int turnId = -1;
    int winnerId = -1;
    std::string winnerName;
    int selfId = -1;

    // 弹仓（公开信息：数量与剩余实空；弹序不公开）
    int shellCount = 0;    // 本次装填弹数
    int shellIndex = 0;    // 已消耗指针
    int liveLeft = 0;      // 剩余实弹
    int blankLeft = 0;     // 剩余空弹
    bool sawed = false;    // 手锯生效中

    // 本人对当前弹槽的私有情报（放大镜）
    bool known = false;
    bool knownLive = false;

    std::vector<ViewPlayer> players;
    std::string roomName;
    std::string modeId;
};
