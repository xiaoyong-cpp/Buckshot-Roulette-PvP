// ============================================================================
// ClientSession.h - 客户端会话
// 持有 TcpClient，负责 join / 重连 / 收发消息，把服务器数据转换为
// GameView（状态镜像）与 GameEvent（动画/日志事件）供 UI 消费。
// ============================================================================
#pragma once

#include <functional>
#include <string>

#include "../core/GameEvent.h"
#include "../core/GameView.h"
#include "Protocol.h"
#include "TcpClient.h"

class ClientSession {
public:
    // ---- UI 设置的回调 ----
    std::function<void(const std::vector<LobbyPlayer>&, const std::string&)> OnLobby;
    std::function<void(const nlohmann::json&)> OnStart;
    std::function<void(const GameView&)> OnState;
    std::function<void(const GameEvent&)> OnEvent;
    std::function<void(const std::string&)> OnError;
    std::function<void()> OnDisconnected;  // socket 断开

    // 连接并加入房间（token 用于断线重连）
    bool Connect(const std::string& ip, int port, const std::string& name,
                 const std::string& token = "");
    void Disconnect();
    bool IsConnected() const { return mClient.IsConnected(); }

    // 发送操作指令：{"act":"shoot","target":id} 或 {"act":"item","item":type,"target":id}
    void SendShoot(int targetId);
    void SendUseItem(const std::string& itemType, int targetId = -1);

    void Update();  // 主循环：轮询入站消息并分发

    int MyId() const { return mId; }
    const std::string& Token() const { return mToken; }
    const std::string& ServerIp() const { return mIp; }
    int ServerPort() const { return mPort; }
    const std::string& Name() const { return mName; }
    bool Joined() const { return mId >= 0; }

private:
    void HandleMessage(const std::string& text);

    TcpClient mClient;
    std::string mIp;
    int mPort = 0;
    std::string mName;
    std::string mToken;
    int mId = -1;
};
