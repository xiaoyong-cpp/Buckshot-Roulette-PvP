#include "CreateRoomScene.h"

#include <memory>
#include <string>

#include "../core/Strings.h"
#include "../network/HostSession.h"
#include "../utils/ConfigLoader.h"
#include "../utils/Logger.h"
#include "FontManager.h"
#include "LobbyScene.h"
#include "MenuScene.h"

CreateRoomScene::CreateRoomScene(SceneManager& mgr) : mMgr(mgr) {
    mNickname.SetMaxLen(16);
    mRoomName.SetMaxLen(20);
    mPort.SetMaxLen(5);
    mPort.SetNumeric(true);
    mMaxPlayers = ConfigLoader::Instance().DefaultMaxPlayers();
}

void CreateRoomScene::OnEnter() {
    mNickname.SetText("");
    mRoomName.SetText("");
    mPort.SetText(std::to_string(ConfigLoader::Instance().Port()));
    mError.clear();
}

void CreateRoomScene::Update() {
    mNickname.Update();
    mRoomName.Update();
    mPort.Update();
    if (mErrorTimer > 0) mErrorTimer -= GetFrameTime();
}

void CreateRoomScene::Draw() {
    auto& fm = FontManager::Instance();
    fm.DrawTextCentered(S::CreateTitle, {0, 60, 1280, 60}, 40, Theme::AccentHi, 2);

    fm.DrawText(S::LabelNickname, 300, 198, 22, Theme::Text);
    mNickname.Draw();
    fm.DrawText(S::LabelRoomName, 300, 268, 22, Theme::Text);
    mRoomName.Draw();
    fm.DrawText(S::LabelPort, 300, 338, 22, Theme::Text);
    mPort.Draw();

    // ---- 最大人数步进器 ----
    fm.DrawText(S::LabelMaxPlayers, 260, 408, 22, Theme::Text);
    Rectangle minus = {460, 400, 50, 44};
    Rectangle plus = {700, 400, 50, 44};
    if (Button(minus, "-", 26).Draw() && mMaxPlayers > 2) mMaxPlayers--;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", mMaxPlayers);
    fm.DrawTextCentered(buf, {520, 400, 170, 44}, 26, Theme::Text, 1);
    if (Button(plus, "+", 26).Draw() && mMaxPlayers < ConfigLoader::Instance().MaxPlayersCap())
        mMaxPlayers++;

    if (Button({460, 490, 360, 56}, S::BtnEnterLobby, 26).Draw()) {
        std::string name = mNickname.Text();
        if (name.empty()) name = "玩家";
        int port = atoi(mPort.Text().c_str());
        if (port <= 0 || port > 65535) {
            mError = "端口无效";
            mErrorTimer = 2;
        } else {
            GameConfig cfg;
            cfg.defaultHp = ConfigLoader::Instance().DefaultHp();
            cfg.maxItems = MAX_BACKPACK;
            cfg.itemsPerRoundMin = ConfigLoader::Instance().ItemsPerRoundMin();
            cfg.itemsPerRoundMax = ConfigLoader::Instance().ItemsPerRoundMax();
            auto session = std::make_shared<HostSession>(name, mRoomName.Text(), port,
                                                          mMaxPlayers, cfg, /*networked=*/true);
            if (!session->Start()) {
                mError = "端口被占用";
                mErrorTimer = 2;
                Logger::Instance().Error("服务器启动失败，端口: " + std::to_string(port));
            } else {
                mMgr.SwitchTo(std::make_unique<LobbyScene>(mMgr, session));
            }
        }
    }
    if (Button({460, 570, 360, 48}, S::BtnBack, 22).Draw()) {
        mMgr.SwitchTo(std::make_unique<MenuScene>(mMgr));
    }
    if (mErrorTimer > 0) fm.DrawTextCentered(mError, {0, 640, 1280, 30}, 20, Theme::Warn, 1);
}
