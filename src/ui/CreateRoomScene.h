// ============================================================================
// CreateRoomScene.h - 创建房间（图形界面配置房间名/最大人数/端口）
// ============================================================================
#pragma once

#include "Scene.h"
#include "SceneManager.h"
#include "Widgets.h"

class CreateRoomScene : public Scene {
public:
    explicit CreateRoomScene(SceneManager& mgr);
    void OnEnter() override;
    void Update() override;
    void Draw() override;

private:
    SceneManager& mMgr;
    TextBox mNickname{Rectangle{460, 190, 360, 40}, "Player"};
    TextBox mRoomName{Rectangle{460, 260, 360, 40}, "Demon Room"};
    TextBox mPort{Rectangle{460, 330, 360, 40}, "7777"};
    int mMaxPlayers = 4;
    float mErrorTimer = 0;
    std::string mError;
};
