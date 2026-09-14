#include "TcpServer.h"

#include "../utils/Logger.h"
#include "NetworkCore.h"

TcpServer::~TcpServer() { Stop(); }

bool TcpServer::Start(int port, int maxClients) {
    if (mRunning.load()) return true;
    if (!NetworkCore::Instance().Init()) return false;

    mMaxClients = maxClients;

    mListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mListen == INVALID_SOCKET) {
        Logger::Instance().Error("创建监听 socket 失败");
        return false;
    }
    // 允许快速重启（地址复用）
    int opt = 1;
    setsockopt(mListen, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((u_short)port);
    if (bind(mListen, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        Logger::Instance().Error("绑定端口失败: " + std::to_string(port));
        NetworkCore::Close(mListen);
        return false;
    }
    if (listen(mListen, 8) == SOCKET_ERROR) {
        Logger::Instance().Error("listen 失败");
        NetworkCore::Close(mListen);
        return false;
    }
    NetworkCore::SetNonBlocking(mListen);

    mRunning.store(true);
    mThread = std::thread(&TcpServer::Run, this);
    Logger::Instance().Info("服务器已启动，端口: " + std::to_string(port));
    return true;
}

void TcpServer::Stop() {
    if (!mRunning.exchange(false)) return;
    if (mThread.joinable()) mThread.join();
    NetworkCore::Close(mListen);
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto& c : mConns) {
        if (c && c->s != INVALID_SOCKET) NetworkCore::Close(c->s);
    }
    mConns.clear();
    Logger::Instance().Info("服务器已停止");
}

void TcpServer::PushInbound(int connId, bool system, const std::string& text) {
    std::lock_guard<std::mutex> lock(mInMutex);
    mInbound.push({connId, system, text});
}

bool TcpServer::PopInbound(ServerMsg& out) {
    std::lock_guard<std::mutex> lock(mInMutex);
    if (mInbound.empty()) return false;
    out = mInbound.front();
    mInbound.pop();
    return true;
}

void TcpServer::Send(int connId, const std::string& jsonText) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (connId < 0 || connId >= (int)mConns.size()) return;
    Conn* c = mConns[(size_t)connId].get();
    if (!c || c->s == INVALID_SOCKET || c->kick.load()) return;
    c->outbox.push_back(NetworkCore::Frame(jsonText));
}

void TcpServer::Broadcast(const std::string& jsonText) {
    std::string frame = NetworkCore::Frame(jsonText);
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto& c : mConns) {
        if (c && c->s != INVALID_SOCKET && !c->kick.load()) c->outbox.push_back(frame);
    }
}

void TcpServer::Kick(int connId) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (connId < 0 || connId >= (int)mConns.size()) return;
    Conn* c = mConns[(size_t)connId].get();
    if (c) {
        c->kick.store(true);
        c->outbox.clear();
    }
}

bool TcpServer::HasClient(int connId) const {
    std::lock_guard<std::mutex> lock(mMutex);
    if (connId < 0 || connId >= (int)mConns.size()) return false;
    const Conn* c = mConns[(size_t)connId].get();
    return c && c->s != INVALID_SOCKET && !c->kick.load();
}

// ============================================================================
// 网络线程：select 循环（收发均在后台线程，主线程只操作队列）
// ============================================================================
void TcpServer::Run() {
    char buf[8192];
    while (mRunning.load()) {
        fd_set readSet, writeSet;
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);

        // 快照 socket 集合（互斥锁内完成，锁外 select）
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mListen != INVALID_SOCKET) FD_SET(mListen, &readSet);
            for (auto& c : mConns) {
                if (!c || c->s == INVALID_SOCKET) continue;
                FD_SET(c->s, &readSet);
                if (!c->outbox.empty() || !c->sendBuf.empty()) FD_SET(c->s, &writeSet);
            }
        }

        timeval tv{0, 20000};  // 20ms
        int rc = select(0, &readSet, &writeSet, nullptr, &tv);
        if (rc == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEINTR) Logger::Instance().Warn("select 错误: " + std::to_string(err));
            continue;
        }

        // ---- 接受新连接 ----
        if (mListen != INVALID_SOCKET && FD_ISSET(mListen, &readSet)) {
            sockaddr_in client{};
            int len = sizeof(client);
            SOCKET cs = accept(mListen, (sockaddr*)&client, &len);
            if (cs != INVALID_SOCKET) {
                std::lock_guard<std::mutex> lock(mMutex);
                if ((int)mConns.size() >= mMaxClients) {
                    closesocket(cs);  // 超出容量直接拒绝
                } else {
                    NetworkCore::SetNonBlocking(cs);
                    auto conn = std::make_unique<Conn>();
                    conn->s = cs;
                    mConns.push_back(std::move(conn));
                    int id = (int)mConns.size() - 1;
                    PushInbound(id, true, "CONNECT");
                }
            }
        }

        // ---- 处理各连接的读写 ----
        std::vector<int> dead;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (int i = 0; i < (int)mConns.size(); ++i) {
                Conn* c = mConns[(size_t)i].get();
                if (!c || c->s == INVALID_SOCKET) {
                    if (!c) dead.push_back(i);  // 空槽
                    continue;
                }
                if (c->kick.load()) {
                    dead.push_back(i);
                    continue;
                }
                // 可写：继续发送 sendBuf / 从 outbox 取新数据
                if (FD_ISSET(c->s, &writeSet) && c->sendBuf.empty() && !c->outbox.empty()) {
                    size_t total = 0;
                    while (!c->outbox.empty() && total < 256 * 1024) {
                        c->sendBuf += c->outbox.front();
                        total += c->outbox.front().size();
                        c->outbox.pop_front();
                    }
                }
                if (FD_ISSET(c->s, &writeSet) && !c->sendBuf.empty()) {
                    int n = send(c->s, c->sendBuf.data(), (int)c->sendBuf.size(), 0);
                    if (n == SOCKET_ERROR) {
                        int err = WSAGetLastError();
                        if (err != WSAEWOULDBLOCK) dead.push_back(i);
                    } else if (n > 0) {
                        c->sendBuf.erase(0, (size_t)n);
                    }
                }
                // 可读：接收 + 解帧
                if (FD_ISSET(c->s, &readSet)) {
                    int n = recv(c->s, buf, sizeof(buf), 0);
                    if (n <= 0) {
                        dead.push_back(i);  // 对端关闭或错误
                    } else {
                        c->recvBuf.append(buf, (size_t)n);
                        std::string payload;
                        while (NetworkCore::TryUnframe(c->recvBuf, payload)) {
                            if (!payload.empty()) PushInbound(i, false, payload);
                        }
                    }
                }
            }
            // 清理失效连接（由网络线程统一关闭 socket）
            for (int i : dead) {
                Conn* c = mConns[(size_t)i].get();
                if (c) {
                    if (c->s != INVALID_SOCKET) NetworkCore::Close(c->s);
                    PushInbound(i, true, "DISCONNECT");
                    mConns[(size_t)i].reset();
                }
            }
        }
    }
}
