// ============================================================================
// LobbyScene.h - 房间大厅（房主可开始游戏，客户端等待）
// 同一场景服务主机/客户端两种角色。
// ============================================================================
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../network/Protocol.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Widgets.h"

class HostSession;
class ClientSession;

class LobbyScene : public Scene {
public:
    // 主机大厅
    LobbyScene(SceneManager& mgr, std::shared_ptr<HostSession> host);
    // 客户端大厅
    LobbyScene(SceneManager& mgr, std::shared_ptr<ClientSession> client);
    ~LobbyScene() override = default;  // 不清回调：后续场景会覆盖注册

    void OnEnter() override;
    void Update() override;
    void Draw() override;

private:
    void DrawPlayerList(const std::vector<LobbyPlayer>& players, int selfId);

    SceneManager& mMgr;
    bool mIsHost = false;
    std::shared_ptr<HostSession> mHost;
    std::shared_ptr<ClientSession> mClient;

    // 客户端侧缓存的大厅数据
    std::vector<LobbyPlayer> mClientLobby;
    std::string mRoomName;
    std::string mClientError;
    float mErrorTimer = 0;
    bool mKicked = false;  // 游戏已开始（不在本机触发 start 时）
};
