#include "HostSession.h"

#include <algorithm>

#include "../core/AIController.h"
#include "../core/Strings.h"
#include "../modes/ModeFactory.h"
#include "../utils/ConfigLoader.h"
#include "../utils/Logger.h"

HostSession::HostSession(const std::string& hostName, const std::string& roomName,
                         int port, int maxPlayers, const GameConfig& cfg, bool networked)
    : mHostName(hostName),
      mRoomName(roomName.empty() ? (hostName + "的房间") : roomName),
      mPort(port),
      mMaxPlayers(maxPlayers),
      mReconnectTimeout(ConfigLoader::Instance().ReconnectTimeout()),
      mSyncHz(ConfigLoader::Instance().SyncHz()),
      mNetworked(networked),
      mCfg(cfg) {
    // 主机始终是 0 号玩家
    mLobby.push_back({0, hostName, true});
}

HostSession::~HostSession() { Stop(); }

bool HostSession::Start() {
    if (!mNetworked) return true;  // 单机模式不需要服务器
    int maxConns = mMaxPlayers * 2 + 4;  // 留出重连/拒绝余量
    return mServer.Start(mPort, maxConns);
}

void HostSession::Stop() {
    if (mGame) mGame->RemoveObserver(this);
    mGame.reset();
    if (mNetworked) mServer.Stop();
}

double HostSession::Now() const {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

bool HostSession::CanStart() const {
    if (!mNetworked) return true;  // 单机随时可开始
    return (int)mLobby.size() >= 2;
}

bool HostSession::StartGame(const std::string& modeId) {
    if (mGame) return true;
    mMode = ModeFactory::Instance().Get(modeId);
    if (!mMode) {
        Logger::Instance().Error("未知模式: " + modeId);
        return false;
    }
    mGame = std::make_unique<Game>(mMode.get(), mRng, mCfg);
    mGame->AddObserver(this);

    if (mNetworked) {
        // 大厅中的全部玩家加入对局（host id 0 + 客户端）
        for (const auto& lp : mLobby)
            mGame->AddPlayer(lp.id, lp.name, false);
        if (mGame->AliveCount() < 2) {
            mGame.reset();
            return false;
        }
    } else {
        // 单机模式：人类 + AI 恶魔
        mLobby.clear();
        mLobby.push_back({0, mHostName, true});
        mGame->AddPlayer(0, mHostName, false);
        mGame->AddPlayer(1, "恶魔", true);
    }
    mGame->Start();

    if (mNetworked) {
        // 广播对局开始
        nlohmann::json players = nlohmann::json::array();
        for (const auto& lp : mLobby) players.push_back({{"id", lp.id}, {"name", lp.name}});
        nlohmann::json startPayload{{"modeId", mMode->GetId()},
                                    {"pvp", mMode->IsPvp()},
                                    {"roomName", mRoomName},
                                    {"hostId", 0},
                                    {"players", players}};
        mServer.Broadcast(MakeMsg("start", startPayload).dump());
        BroadcastState();
        mStateDirty = false;
    }
    Logger::Instance().Info("对局已启动，模式: " + modeId);
    return true;
}

// ============================================================================
// IGameObserver：把事件转发给所有客户端。
// MagnifierReveal 为私有事件（仅在个性化 state 中体现），不广播。
// ============================================================================
void HostSession::OnGameEvent(const GameEvent& event) {
    if (event.kind == EventKind::MagnifierReveal) return;
    if (!mNetworked) return;
    mServer.Broadcast(MakeMsg("event", EventToJson(event)).dump());
    mStateDirty = true;
}

void HostSession::Update(double dt) {
    // ---- 处理入站消息（网络线程投递）----
    ServerMsg msg;
    while (mServer.PopInbound(msg)) {
        if (msg.system) {
            if (msg.text == "CONNECT") {
                mConnConnectTime[msg.connId] = Now();
            } else if (msg.text == "DISCONNECT") {
                HandleDisconnect(msg.connId);
            }
            continue;
        }
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(msg.text);
        } catch (...) {
            Logger::Instance().Warn("收到非法 JSON 消息");
            continue;
        }
        std::string type = j.value("type", "");
        if (type == "join") HandleJoin(msg.connId, j.value("payload", nlohmann::json::object()));
        else if (type == "action") HandleAction(msg.connId, j.value("payload", nlohmann::json::object()));
    }

    // ---- join 超时清理：连接后 5 秒未发送 join 则踢出 ----
    double now = Now();
    for (auto it = mConnConnectTime.begin(); it != mConnConnectTime.end();) {
        int conn = it->first;
        if (mConnToPlayer.count(conn) == 0 && now - it->second > 5.0) {
            mServer.Kick(conn);
            it = mConnConnectTime.erase(it);
        } else {
            ++it;
        }
    }

    // ---- 游戏推进 + AI 托管 + 状态同步 ----
    if (mGame) {
        mGame->Update(dt);

        // 回合切换时重置 AI 节奏计时器
        int cur = mGame->CurrentTurnPlayerId();
        if (cur != mLastTurnId) {
            mLastTurnId = cur;
            mAiTimer = 0.0;
        }

        // AI 行动（单机恶魔 / PVP 掉线玩家）
        if (mGame->GetState() == GameState::Aiming) {
            Player* actor = mGame->FindPlayer(cur);
            if (actor && (actor->isAI || !actor->connected)) {
                mAiTimer += dt;
                if (mAiTimer >= 1.0) {
                    mAiTimer = 0.0;
                    AIAction act = AIController::Decide(*mGame, actor->id, mRng);
                    std::string err;
                    if (act.type == AIAction::Type::Shoot) {
                        mGame->Shoot(actor->id, act.targetId, &err);
                    } else {
                        mGame->UseItem(actor->id, act.item, -1, &err);
                    }
                    if (!err.empty() && mNetworked) {
                        Logger::Instance().Warn("AI 动作失败: " + err);
                    }
                }
            } else {
                mAiTimer = 0.0;
            }
        }

        // 状态同步：脏标记或每秒 syncHz 次
        if (mNetworked) {
            mSyncAccum += dt;
            double interval = (mSyncHz > 0) ? (1.0 / mSyncHz) : 0.1;
            if (mStateDirty || mSyncAccum >= interval) {
                BroadcastState();
                mSyncAccum = 0.0;
                mStateDirty = false;
            }
        }
    }
}

void HostSession::HandleJoin(int connId, const nlohmann::json& payload) {
    std::string name = payload.value("name", "");
    std::string token = payload.value("token", "");

    // 去掉昵称中的控制字符与首尾空白，限长 16
    std::string clean;
    for (char ch : name) {
        if (ch >= 32 && ch < 127) clean.push_back(ch);
    }
    if (clean.empty()) clean = "玩家" + std::to_string(mNextPlayerId);
    if (clean.size() > 16) clean.resize(16);

    if (InGame()) {
        // 对局进行中：仅允许持有有效 token 的玩家重连
        if (!token.empty() && mTokenToPlayer.count(token)) {
            int pid = mTokenToPlayer[token];
            Player* p = mGame->FindPlayer(pid);
            if (p && p->alive) {
                // 解绑旧连接
                auto old = mPlayerToConn.find(pid);
                if (old != mPlayerToConn.end()) {
                    mConnToPlayer.erase(old->second);
                    mPlayerToConn.erase(old);
                }
                mConnToPlayer[connId] = pid;
                mPlayerToConn[pid] = connId;
                p->connected = true;
                mPlayerDisconnectTime.erase(pid);

                // 回送欢迎与即时状态
                nlohmann::json welcome{{"playerId", pid},
                                       {"token", token},
                                       {"roomName", mRoomName},
                                       {"hostName", mHostName},
                                       {"maxPlayers", mMaxPlayers},
                                       {"youHost", false},
                                       {"reconnect", true}};
                mServer.Send(connId, MakeMsg("welcome", welcome).dump());
                mServer.Send(connId, MakeMsg("event", EventToJson(
                    GameEvent{EventKind::PlayerConnect, pid, -1})).dump());
                mServer.Send(connId, MakeMsg("state", BuildStateJson(*mGame, pid, mRoomName)).dump());
                Logger::Instance().Info("玩家 " + clean + " 重连，id=" + std::to_string(pid));
                return;
            }
        }
        SendError(connId, S::ErrGameStarted);
        return;
    }

    // 大厅加入
    if ((int)mLobby.size() >= mMaxPlayers) {
        SendError(connId, S::ErrRoomFull);
        return;
    }
    int pid = mNextPlayerId++;
    mConnToPlayer[connId] = pid;
    mPlayerToConn[pid] = connId;
    if (token.empty()) token = mRng.Token();
    mTokenToPlayer[token] = pid;
    mLobby.push_back({pid, clean, false});
    mConnConnectTime.erase(connId);

    nlohmann::json welcome{{"playerId", pid},
                           {"token", token},
                           {"roomName", mRoomName},
                           {"hostName", mHostName},
                           {"maxPlayers", mMaxPlayers},
                           {"youHost", false}};
    mServer.Send(connId, MakeMsg("welcome", welcome).dump());
    BroadcastLobby();
    Logger::Instance().Info("玩家 " + clean + " 加入房间，id=" + std::to_string(pid));
}

void HostSession::HandleAction(int connId, const nlohmann::json& payload) {
    if (!InGame()) {
        SendError(connId, S::ErrGameNotStarted);
        return;
    }
    auto it = mConnToPlayer.find(connId);
    if (it == mConnToPlayer.end()) {
        SendError(connId, S::ErrNotJoined);
        return;
    }
    int pid = it->second;
    Player* p = mGame->FindPlayer(pid);
    if (!p || !p->connected) {
        SendError(connId, S::ErrDisconnected);
        return;
    }
    if (pid != mGame->CurrentTurnPlayerId()) {
        SendError(connId, S::ErrNotYourTurn);
        return;
    }

    std::string act = payload.value("act", "");
    std::string err;
    if (act == "shoot") {
        int target = payload.value("target", -1);
        mGame->Shoot(pid, target, &err);
    } else if (act == "item") {
        std::string item = payload.value("item", "");
        int target = payload.value("target", -1);
        mGame->UseItem(pid, item, target, &err);
    } else {
        err = S::ErrUnknown;
    }
    if (!err.empty()) SendError(connId, err);
}

void HostSession::HandleDisconnect(int connId) {
    mConnConnectTime.erase(connId);
    auto it = mConnToPlayer.find(connId);
    if (it == mConnToPlayer.end()) return;
    int pid = it->second;
    mConnToPlayer.erase(it);
    mPlayerToConn.erase(pid);

    if (InGame()) {
        Player* p = mGame->FindPlayer(pid);
        if (p) {
            p->connected = false;
            mPlayerDisconnectTime[pid] = Now();
            // 通知所有客户端该玩家掉线（将由 AI 托管）
            mServer.Broadcast(MakeMsg("event", EventToJson(
                GameEvent{EventKind::PlayerDisconnect, pid, -1})).dump());
            mStateDirty = true;
        }
        Logger::Instance().Info("玩家 id=" + std::to_string(pid) + " 掉线，AI 托管中");
    } else {
        // 大厅离开
        mLobby.erase(std::remove_if(mLobby.begin(), mLobby.end(),
                                    [pid](const LobbyPlayer& lp) { return lp.id == pid; }),
                     mLobby.end());
        BroadcastLobby();
    }
}

void HostSession::BroadcastLobby() {
    if (!mNetworked) return;
    mServer.Broadcast(MakeMsg("lobby", BuildLobbyJson(mLobby, 0, mRoomName)).dump());
}

void HostSession::BroadcastState() {
    if (!mNetworked || !mGame) return;
    // 个性化：每个连接看到自己的私有情报
    for (const auto& [connId, pid] : mConnToPlayer) {
        mServer.Send(connId, MakeMsg("state", BuildStateJson(*mGame, pid, mRoomName)).dump());
    }
}

void HostSession::SendError(int connId, const std::string& msg) {
    if (!mNetworked) return;
    mServer.Send(connId, MakeMsg("error", nlohmann::json{{"msg", msg}}).dump());
}
