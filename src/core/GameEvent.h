// ============================================================================
// GameEvent.h - 游戏事件（设计模式：观察者模式的"事件"载体）
// Game 在状态变化时产生事件，通过 IGameObserver 分发给：
//   1. UI（刷新界面、播放动画、写消息日志）
//   2. 网络中继（主机端转发给所有客户端）
// ============================================================================
#pragma once

#include <string>

enum class EventKind {
    GameStart,        // 对局开始          value=玩家数
    TurnStart,        // 轮到某玩家        playerId
    TurnSkip,         // 玩家被手铐跳过    playerId
    Reload,           // 重新装弹          value=实弹数 value2=空弹数
    Shot,             // 有人开枪          playerId=射手 targetId=目标(-1自己)
                      //                   flag=是否实弹 value=伤害
    ExtraTurn,        // 空弹自射获得额外回合 playerId
    BeerEject,        // 啤酒退弹          playerId flag=该弹是否实弹
    ItemUsed,         // 使用道具          playerId text=道具类型 targetId
    ItemFail,         // 道具使用失败      playerId text=原因（仅本地记录）
    MagnifierReveal,  // 放大镜窥探结果（私有事件，只通知使用者本人）
                      //                   playerId flag=是否实弹
    Steal,            // 肾上腺素偷道具    playerId=小偷 targetId=受害者 text=道具
    PlayerDamaged,    // 扣血              targetId value=伤害值
    PlayerHealed,     // 回血（香烟/过期药品）targetId value=回复量 flag=是否为正面效果
    PlayerEliminated, // 淘汰              targetId
    GameOver,         // 游戏结束          targetId=获胜者
    PlayerConnect,    // 玩家上线（重连）  playerId
    PlayerDisconnect, // 玩家掉线          playerId
    Info,             // 一般提示          text
};

struct GameEvent {
    EventKind kind = EventKind::Info;
    int playerId = -1;
    int targetId = -1;
    bool flag = false;
    int value = 0;
    int value2 = 0;
    std::string text;
};

// 观察者接口：UI 与网络中继都实现此接口以接收游戏事件
class IGameObserver {
public:
    virtual ~IGameObserver() = default;
    virtual void OnGameEvent(const GameEvent& event) = 0;
};
