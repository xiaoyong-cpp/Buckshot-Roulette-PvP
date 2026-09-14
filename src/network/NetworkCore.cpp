#include "NetworkCore.h"

#include "../utils/Logger.h"

#pragma comment(lib, "ws2_32.lib")

NetworkCore& NetworkCore::Instance() {
    static NetworkCore sInstance;
    return sInstance;
}

NetworkCore::~NetworkCore() { Shutdown(); }

bool NetworkCore::Init() {
    if (mReady) return true;
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0) {
        Logger::Instance().Error("WSAStartup 失败，错误码: " + std::to_string(rc));
        return false;
    }
    mReady = true;
    Logger::Instance().Info("Winsock 2.2 初始化完成");
    return true;
}

void NetworkCore::Shutdown() {
    if (mReady) {
        WSACleanup();
        mReady = false;
    }
}

bool NetworkCore::SetNonBlocking(SOCKET s) {
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}

void NetworkCore::Close(SOCKET& s) {
    if (s != INVALID_SOCKET) {
        closesocket(s);
        s = INVALID_SOCKET;
    }
}

std::string NetworkCore::Frame(const std::string& payload) {
    std::string out;
    uint32_t len = (uint32_t)payload.size();
    out.reserve(payload.size() + 4);
    out.push_back((char)((len >> 24) & 0xFF));
    out.push_back((char)((len >> 16) & 0xFF));
    out.push_back((char)((len >> 8) & 0xFF));
    out.push_back((char)(len & 0xFF));
    out += payload;
    return out;
}

bool NetworkCore::TryUnframe(std::string& buf, std::string& payload) {
    if (buf.size() < 4) return false;
    uint32_t len = ((uint32_t)(unsigned char)buf[0] << 24) | ((uint32_t)(unsigned char)buf[1] << 16) |
                   ((uint32_t)(unsigned char)buf[2] << 8) | (uint32_t)(unsigned char)buf[3];
    if (len > MAX_PAYLOAD) {
        // 帧头异常：丢弃整个缓冲（视为脏连接，由上层断开）
        buf.clear();
        return false;
    }
    if (buf.size() < 4 + (size_t)len) return false;
    payload.assign(buf, 4, (size_t)len);
    buf.erase(0, 4 + (size_t)len);
    return true;
}
