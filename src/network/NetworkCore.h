// ============================================================================
// NetworkCore.h - Winsock 初始化与帧化工具（单例模式）
// 所有 TCP 消息使用 4 字节大端长度前缀 + JSON 负载的帧格式。
// ============================================================================
#pragma once

#include <string>

// raylib 与 windows.h 存在符号冲突（Rectangle/CloseWindow/ShowCursor），
// 在包含 winsock2.h/windows.h 之前定义以下宏，屏蔽冲突的 GDI/USER 声明：
//   NOGDI  -> 屏蔽 wingdi.h 的 Rectangle 函数（与 raylib 的 Rectangle 类型冲突）
//   NOUSER -> 屏蔽 winuser.h 的 CloseWindow/ShowCursor（与 raylib 同名函数冲突）
//   NOMINMAX -> 阻止 windows.h 定义 min/max 宏干扰 std::min/max
//   WIN32_LEAN_AND_MEAN -> 精简 windows.h，减少无关头文件
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

class NetworkCore {
public:
    static NetworkCore& Instance();  // 单例模式：WSAStartup 只执行一次

    bool Init();
    void Shutdown();
    bool IsReady() const { return mReady; }

    // 工具函数
    static bool SetNonBlocking(SOCKET s);
    static void Close(SOCKET& s);

    // 帧化：4 字节长度前缀 + 负载
    static std::string Frame(const std::string& payload);
    // 尝试从 buf 头部解出一帧；成功时输出 payload 并从 buf 移除已消费字节
    static bool TryUnframe(std::string& buf, std::string& payload);

    static constexpr size_t MAX_PAYLOAD = 1 << 20;  // 1MB 上限（防御异常数据）

private:
    NetworkCore() = default;
    ~NetworkCore();
    bool mReady = false;
};
