#include "TcpClient.h"

#include "../utils/Logger.h"
#include "NetworkCore.h"

TcpClient::~TcpClient() { Disconnect(); }

bool TcpClient::Connect(const std::string& ip, int port, int timeoutMs) {
    if (mConnected.load()) Disconnect();
    if (!NetworkCore::Instance().Init()) return false;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* result = nullptr;
    if (getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &result) != 0 || !result) {
        Logger::Instance().Error("无法解析服务器地址: " + ip);
        return false;
    }

    mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        return false;
    }
    NetworkCore::SetNonBlocking(mSocket);
    int rc = connect(mSocket, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);
    if (rc == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            Logger::Instance().Error("连接失败，错误码: " + std::to_string(err));
            NetworkCore::Close(mSocket);
            return false;
        }
        // 非阻塞连接：等待可写（或异常）
        fd_set writeSet, exceptSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);
        FD_SET(mSocket, &writeSet);
        FD_SET(mSocket, &exceptSet);
        timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        rc = select(0, nullptr, &writeSet, &exceptSet, &tv);
        if (rc <= 0 || FD_ISSET(mSocket, &exceptSet)) {
            Logger::Instance().Error("连接超时或被拒绝");
            NetworkCore::Close(mSocket);
            return false;
        }
        int soError = 0;
        int len = sizeof(soError);
        getsockopt(mSocket, SOL_SOCKET, SO_ERROR, (char*)&soError, &len);
        if (soError != 0) {
            Logger::Instance().Error("连接失败: SO_ERROR=" + std::to_string(soError));
            NetworkCore::Close(mSocket);
            return false;
        }
    }

    mRecvBuf.clear();
    mSendBuf.clear();
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mOutbox.clear();
    }
    mConnected.store(true);
    mStop.store(false);
    mThread = std::thread(&TcpClient::Run, this);
    Logger::Instance().Info("已连接到 " + ip + ":" + std::to_string(port));
    return true;
}

void TcpClient::Disconnect() {
    if (!mConnected.exchange(false) && mSocket == INVALID_SOCKET) {
        if (mThread.joinable()) mThread.join();
        return;
    }
    mStop.store(true);
    if (mThread.joinable()) mThread.join();
    NetworkCore::Close(mSocket);
    std::lock_guard<std::mutex> lock(mMutex);
    mOutbox.clear();
    mSendBuf.clear();
    mRecvBuf.clear();
}

void TcpClient::Send(const std::string& jsonText) {
    if (!mConnected.load()) return;
    std::lock_guard<std::mutex> lock(mMutex);
    mOutbox.push_back(NetworkCore::Frame(jsonText));
}

bool TcpClient::PopInbound(ClientMsg& out) {
    std::lock_guard<std::mutex> lock(mInMutex);
    if (mInbound.empty()) return false;
    out = mInbound.front();
    mInbound.pop();
    return true;
}

void TcpClient::Run() {
    char buf[8192];
    while (mConnected.load() && !mStop.load()) {
        fd_set readSet, writeSet;
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        bool hasWrite = false;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mSocket != INVALID_SOCKET) {
                FD_SET(mSocket, &readSet);
                if (!mOutbox.empty() || !mSendBuf.empty()) {
                    FD_SET(mSocket, &writeSet);
                    hasWrite = true;
                }
            }
        }
        if (mSocket == INVALID_SOCKET) break;

        timeval tv{0, 20000};
        int rc = select(0, &readSet, hasWrite ? &writeSet : nullptr, nullptr, &tv);
        if (rc == SOCKET_ERROR) break;
        if (rc == 0) continue;

        // ---- 接收 ----
        if (FD_ISSET(mSocket, &readSet)) {
            int n = recv(mSocket, buf, sizeof(buf), 0);
            if (n <= 0) {
                // 服务器关闭连接
                mConnected.store(false);
                std::lock_guard<std::mutex> inLock(mInMutex);
                mInbound.push({true, "DISCONNECT"});
                break;
            }
            mRecvBuf.append(buf, (size_t)n);
            std::string payload;
            while (NetworkCore::TryUnframe(mRecvBuf, payload)) {
                if (!payload.empty()) {
                    std::lock_guard<std::mutex> inLock(mInMutex);
                    mInbound.push({false, payload});
                }
            }
        }

        // ---- 发送 ----
        if (FD_ISSET(mSocket, &writeSet)) {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mSendBuf.empty() && !mOutbox.empty()) {
                size_t total = 0;
                while (!mOutbox.empty() && total < 256 * 1024) {
                    mSendBuf += mOutbox.front();
                    total += mOutbox.front().size();
                    mOutbox.pop_front();
                }
            }
            if (!mSendBuf.empty()) {
                int n = send(mSocket, mSendBuf.data(), (int)mSendBuf.size(), 0);
                if (n == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK) {
                        mConnected.store(false);
                        std::lock_guard<std::mutex> inLock(mInMutex);
                        mInbound.push({true, "DISCONNECT"});
                        break;
                    }
                } else if (n > 0) {
                    mSendBuf.erase(0, (size_t)n);
                }
            }
        }
    }
}
