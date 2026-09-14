#include "JoinScene.h"

#include <memory>
#include <string>

#include "../core/Strings.h"
#include "../network/ClientSession.h"
#include "../utils/ConfigLoader.h"
#include "../utils/Logger.h"
#include "FontManager.h"
#include "LobbyScene.h"
#include "MenuScene.h"

JoinScene::JoinScene(SceneManager& mgr) : mMgr(mgr) {
    mNickname.SetMaxLen(16);
    mIp.SetMaxLen(20);
    mPort.SetMaxLen(5);
    mPort.SetNumeric(true);
}

void JoinScene::OnEnter() {
    mNickname.SetText("");
    mIp.SetText("");
    mPort.SetText(std::to_string(ConfigLoader::Instance().Port()));
    mError.clear();
    mConnecting = false;
    mTimer = 0;
    mClient.reset();
}

void JoinScene::Update() {
    if (mConnecting && mClient) {
        // 等待 welcome 或失败
        mTimer += GetFrameTime();
        mClient->Update();
        if (mClient->Joined()) {
            // 加入成功 -> 进入大厅
            mMgr.SwitchTo(std::make_unique<LobbyScene>(mMgr, mClient));
            return;
        }
        if (!mClient->IsConnected() || mTimer > 6.0f) {
            mClient->Disconnect();
            mClient.reset();
            mConnecting = false;
            mError = S::ConnectFail;
        }
        return;
    }
    mNickname.Update();
    mIp.Update();
    mPort.Update();
}

void JoinScene::Draw() {
    auto& fm = FontManager::Instance();
    fm.DrawTextCentered(S::JoinTitle, {0, 60, 1280, 60}, 40, Theme::AccentHi, 2);

    fm.DrawText(S::LabelNickname, 300, 208, 22, Theme::Text);
    mNickname.Draw();
    fm.DrawText(S::LabelServerIP, 260, 278, 22, Theme::Text);
    mIp.Draw();
    fm.DrawText(S::LabelServerPort, 260, 348, 22, Theme::Text);
    mPort.Draw();

    if (!mConnecting) {
        if (Button({460, 430, 360, 56}, S::BtnConnect, 26).Draw()) {
            std::string ip = mIp.Text();
            int port = atoi(mPort.Text().c_str());
            if (ip.empty() || port <= 0 || port > 65535) {
                mError = S::ConnectFail;
            } else {
                std::string name = mNickname.Text();
                if (name.empty()) name = "玩家";
                mClient = std::make_shared<ClientSession>();
                if (mClient->Connect(ip, port, name)) {
                    mConnecting = true;
                    mTimer = 0;
                } else {
                    mClient.reset();
                    mError = S::ConnectFail;
                }
            }
        }
        if (Button({460, 510, 360, 48}, S::BtnBack, 22).Draw()) {
            mMgr.SwitchTo(std::make_unique<MenuScene>(mMgr));
        }
        if (!mError.empty()) fm.DrawTextCentered(mError, {0, 590, 1280, 30}, 20, Theme::Warn, 1);
    } else {
        fm.DrawTextCentered(S::Connecting, {0, 450, 1280, 40}, 26, Theme::Sub, 1);
    }
}
