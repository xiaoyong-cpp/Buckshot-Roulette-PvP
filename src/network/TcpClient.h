// ============================================================================
// TcpClient.h - TCP 客户端（非阻塞 select 模型 + 后台收发线程）
// 主线程只调用 Send / PopInbound，线程安全。
// ============================================================================
#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

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
struct ClientMsg {
    bool system = false;             // true = 断开通知
    std::string text;                // system 时为 "DISCONNECT"；否则为 JSON 文本
};

class TcpClient {
public:
    TcpClient() = default;
    ~TcpClient();

    // 连接（非阻塞 + select 等待可写，超时返回 false）
    bool Connect(const std::string& ip, int port, int timeoutMs = 5000);
    void Disconnect();
    bool IsConnected() const { return mConnected.load(); }

    void Send(const std::string& jsonText);
    bool PopInbound(ClientMsg& out);

private:
    void Run();

    std::thread mThread;
    std::atomic<bool> mConnected{false};
    std::atomic<bool> mStop{false};
    SOCKET mSocket = INVALID_SOCKET;
    std::string mRecvBuf;            // 网络线程独占
    std::deque<std::string> mOutbox; // 互斥锁保护
    std::string mSendBuf;            // 网络线程独占
    mutable std::mutex mMutex;
    std::queue<ClientMsg> mInbound;
    std::mutex mInMutex;
};
