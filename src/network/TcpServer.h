// ============================================================================
// TcpServer.h - TCP 房间服务器（主机端，Winsock select 非阻塞模型）
//
// 架构（设计文档 2.2 / 3.1 节）：
//   * 主机同时充当游戏服务器（逻辑仲裁），客户端只发送 action 并接收 state
//   * 网络收发在独立后台线程（select 循环），主线程通过消息队列通信，
//     保证 raylib 主循环线程安全
// ============================================================================
#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// 屏蔽 windows.h 与 raylib 的符号冲突（详见 NetworkCore.h 注释）
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

// 网络线程 -> 主线程 的入站消息
struct ServerMsg {
    int connId = -1;
    bool system = false;               // true = 连接/断开通知
    std::string text;                  // system 时为 "CONNECT"/"DISCONNECT"；否则为 JSON 文本
};

class TcpServer {
public:
    TcpServer() = default;
    ~TcpServer();

    bool Start(int port, int maxClients);
    void Stop();
    bool IsRunning() const { return mRunning.load(); }

    // 出站（主线程调用）：发送给指定连接 / 广播给全部连接
    void Send(int connId, const std::string& jsonText);
    void Broadcast(const std::string& jsonText);

    // 入站（主线程轮询）
    bool PopInbound(ServerMsg& out);

    // 主动断开某连接（主线程调用，实际关闭由网络线程执行）
    void Kick(int connId);

    bool HasClient(int connId) const;

private:
    struct Conn {
        SOCKET s = INVALID_SOCKET;
        std::string recvBuf;              // 网络线程独占
        std::deque<std::string> outbox;   // 待发送帧（互斥锁保护）
        std::string sendBuf;              // 当前发送缓冲（网络线程独占）
        std::atomic<bool> kick{false};    // 主线程请求断开
    };
    void Run();
    void PushInbound(int connId, bool system, const std::string& text);

    std::thread mThread;
    std::atomic<bool> mRunning{false};
    SOCKET mListen = INVALID_SOCKET;
    std::vector<std::unique_ptr<Conn>> mConns;  // 下标即 connId
    mutable std::mutex mMutex;                   // 保护 mConns / outbox
    std::queue<ServerMsg> mInbound;
    std::mutex mInMutex;
    int mMaxClients = 8;
};
