// ============================================================================
// HostSession.h - 主机端会话（设计模式：观察者模式 + 单例式仲裁）
//
// 职责：
//   * 持有权威 Game 与 TcpServer，作为游戏服务器（逻辑仲裁）
//   * 实现 IGameObserver，把游戏事件转发给所有客户端（网络中继）
//   * 处理客户端的 join / action / 断线，广播 lobby 与 state
//   * 驱动 AI（单机恶魔 / PVP 掉线玩家托管）
//
// 同时承担单机模式（networked=false 时不开服务器，仅本地 Game + AI 恶魔）。
// ============================================================================
#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>

#include "../core/Game.h"
#include "Protocol.h"
#include "TcpServer.h"

class HostSession : public IGameObserver {
public:
    // networked=false 表示单机模式（不启动服务器，自带一个 AI 恶魔对手）
    HostSession(const std::string& hostName, const std::string& roomName,
                int port, int maxPlayers, const GameConfig& cfg, bool networked = true);
    ~HostSession();

    bool Start();                 // 启动服务器（单机模式跳过）
    void Stop();

    // ---- 大厅 ----
    const std::vector<LobbyPlayer>& Lobby() const { return mLobby; }
    bool CanStart() const;        // 人数达标
    bool InGame() const { return mGame != nullptr; }
    bool StartGame(const std::string& modeId);  // 创建 Game 并开始
    const std::string& RoomName() const { return mRoomName; }
    const std::string& HostName() const { return mHostName; }
    int Port() const { return mPort; }
    int MaxPlayers() const { return mMaxPlayers; }

    // ---- 游戏 ----
    Game* GamePtr() { return mGame.get(); }
    int MyId() const { return 0; }  // 主机恒为 0 号玩家
    bool IsNetworked() const { return mNetworked; }

    // 主循环：收消息 / AI 托管 / 状态同步
    void Update(double dt);

    // IGameObserver：把事件转发给客户端（私有事件除外）
    void OnGameEvent(const GameEvent& event) override;

private:
    void HandleJoin(int connId, const nlohmann::json& payload);
    void HandleAction(int connId, const nlohmann::json& payload);
    void HandleDisconnect(int connId);
    void BroadcastLobby();
    void BroadcastState();
    void SendError(int connId, const std::string& msg);

    double Now() const;  // 单调时钟秒

    TcpServer mServer;
    std::unique_ptr<Game> mGame;
    std::shared_ptr<IMode> mMode;
    std::vector<LobbyPlayer> mLobby;
    std::string mHostName, mRoomName;
    int mPort, mMaxPlayers, mReconnectTimeout, mSyncHz;
    bool mNetworked;
    GameConfig mCfg;
    Random mRng;

    std::map<int, int> mConnToPlayer;          // connId -> playerId
    std::map<int, int> mPlayerToConn;          // playerId -> connId
    std::map<std::string, int> mTokenToPlayer; // token -> playerId（断线重连）
    std::map<int, double> mConnConnectTime;    // connId -> 连接时刻（join 超时）
    std::map<int, double> mPlayerDisconnectTime;
    int mNextPlayerId = 1;

    double mSyncAccum = 0.0;
    bool mStateDirty = false;
    double mAiTimer = 0.0;
    int mLastTurnId = -1;
};
