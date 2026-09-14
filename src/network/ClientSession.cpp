#include "ClientSession.h"

#include "../utils/Logger.h"

bool ClientSession::Connect(const std::string& ip, int port, const std::string& name,
                             const std::string& token) {
    mIp = ip;
    mPort = port;
    mName = name;
    mToken = token;
    mId = -1;

    if (!mClient.Connect(ip, port, 5000)) return false;

    // 发送 join（携带昵称与 token）
    nlohmann::json payload{{"name", name}};
    if (!token.empty()) payload["token"] = token;
    mClient.Send(MakeMsg("join", payload).dump());
    Logger::Instance().Info("已发送加入请求到 " + ip + ":" + std::to_string(port));
    return true;
}

void ClientSession::Disconnect() {
    mClient.Disconnect();
    mId = -1;
}

void ClientSession::SendShoot(int targetId) {
    nlohmann::json action{{"act", "shoot"}, {"target", targetId}};
    mClient.Send(MakeMsg("action", action).dump());
}

void ClientSession::SendUseItem(const std::string& itemType, int targetId) {
    nlohmann::json action{{"act", "item"}, {"item", itemType}, {"target", targetId}};
    mClient.Send(MakeMsg("action", action).dump());
}

void ClientSession::Update() {
    ClientMsg msg;
    while (mClient.PopInbound(msg)) {
        if (msg.system) {
            if (OnDisconnected) OnDisconnected();
            mClient.Disconnect();
            return;
        }
        HandleMessage(msg.text);
    }
}

void ClientSession::HandleMessage(const std::string& text) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (...) {
        Logger::Instance().Warn("收到非法消息");
        return;
    }
    std::string type = j.value("type", "");
    nlohmann::json payload = j.value("payload", nlohmann::json::object());

    if (type == "welcome") {
        mId = payload.value("playerId", -1);
        mToken = payload.value("token", "");
        Logger::Instance().Info("加入成功，玩家 id=" + std::to_string(mId));
    } else if (type == "lobby") {
        std::vector<LobbyPlayer> players;
        int hostId = 0;
        std::string roomName;
        if (ParseLobbyJson(players, hostId, roomName, payload)) {
            if (OnLobby) OnLobby(players, roomName);
        }
    } else if (type == "start") {
        if (OnStart) OnStart(payload);
    } else if (type == "state") {
        GameView view;
        if (ParseStateJson(view, payload)) {
            if (OnState) OnState(view);
        }
    } else if (type == "event") {
        GameEvent e;
        if (EventFromJson(e, payload) && OnEvent) OnEvent(e);
    } else if (type == "error") {
        if (OnError) OnError(payload.value("msg", ""));
    }
}
