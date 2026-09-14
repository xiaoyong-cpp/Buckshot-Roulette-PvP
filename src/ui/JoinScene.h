// ============================================================================
// JoinScene.h - 加入房间（输入 IP/端口/昵称，图形界面连接）
// ============================================================================
#pragma once

#include <memory>

#include "Scene.h"
#include "SceneManager.h"
#include "Widgets.h"

class ClientSession;

class JoinScene : public Scene {
public:
    explicit JoinScene(SceneManager& mgr);
    void OnEnter() override;
    void Update() override;
    void Draw() override;

private:
    SceneManager& mMgr;
    TextBox mNickname{Rectangle{460, 200, 360, 40}, "Player"};
    TextBox mIp{Rectangle{460, 270, 360, 40}, "192.168.1.100"};
    TextBox mPort{Rectangle{460, 340, 360, 40}, "7777"};
    std::shared_ptr<ClientSession> mClient;
    bool mConnecting = false;
    float mTimer = 0;
    std::string mError;
};
