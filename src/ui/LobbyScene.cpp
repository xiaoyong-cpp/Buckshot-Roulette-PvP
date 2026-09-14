#include "LobbyScene.h"

#include <memory>

#include "../core/Strings.h"
#include "../network/ClientSession.h"
#include "../network/HostSession.h"
#include "../utils/Logger.h"
#include "FontManager.h"
#include "GameScene.h"
#include "MenuScene.h"

LobbyScene::LobbyScene(SceneManager& mgr, std::shared_ptr<HostSession> host)
    : mMgr(mgr), mIsHost(true), mHost(std::move(host)) {}

LobbyScene::LobbyScene(SceneManager& mgr, std::shared_ptr<ClientSession> client)
    : mMgr(mgr), mIsHost(false), mClient(std::move(client)) {
    // 注册客户端回调：大厅更新 / 对局开始 / 错误 / 掉线
    mClient->OnLobby = [this](const std::vector<LobbyPlayer>& players, const std::string& room) {
        mClientLobby = players;
        mRoomName = room;
    };
    mClient->OnStart = [this](const nlohmann::json&) {
        // 房主开始游戏 -> 切换到游戏场景（客户端镜像渲染）
        mMgr.SwitchTo(std::make_unique<GameScene>(mMgr, mClient));
    };
    mClient->OnError = [this](const std::string& msg) {
        mClientError = msg;
        mErrorTimer = 3;
        if (msg == S::KickStarted || msg == S::ErrGameStarted) mKicked = true;
    };
    mClient->OnDisconnected = [this]() { mKicked = true; };
}

// LobbyScene::~LobbyScene() 在头文件中已 = default，此处无需重复定义
// （回调由后续场景覆盖注册，无需在本析构中清理）

void LobbyScene::OnEnter() {}

void LobbyScene::Update() {
    if (mErrorTimer > 0) mErrorTimer -= GetFrameTime();

    if (mIsHost) {
        mHost->Update(0.0);  // 处理加入/离开消息
    } else {
        mClient->Update();
        if (mKicked) {
            // 被踢或掉线 -> 断开返回菜单
            mClient->Disconnect();
            mMgr.SwitchTo(std::make_unique<MenuScene>(mMgr));
        }
    }
}

void LobbyScene::Draw() {
    auto& fm = FontManager::Instance();
    fm.DrawTextCentered(S::LobbyTitle, {0, 50, 1280, 50}, 36, Theme::AccentHi, 2);

    const std::vector<LobbyPlayer>* players = nullptr;
    std::string roomName;
    int selfId = -1;

    if (mIsHost) {
        players = &mHost->Lobby();
        roomName = mHost->RoomName();
        selfId = 0;
    } else {
        players = &mClientLobby;
        roomName = mRoomName;
        selfId = mClient->MyId();
    }

    if (players) {
        std::string label = std::string(S::LabelRoom) + " " + roomName;
        fm.DrawTextCentered(label, {0, 110, 1280, 30}, 22, Theme::Sub, 1);
        DrawPlayerList(*players, selfId);
    }

    if (mIsHost) {
        // 房主：开始游戏按钮（人数达标才可用）
        Button startBtn({440, 560, 400, 60}, S::BtnStart, 28);
        startBtn.SetEnabled(mHost->CanStart());
        if (startBtn.Draw()) {
            if (mHost->StartGame("multiplayer")) {
                mMgr.SwitchTo(std::make_unique<GameScene>(mMgr, mHost));
            }
        }
        if ((int)mHost->Lobby().size() < 2) {
            fm.DrawTextCentered(S::WaitPlayers, {0, 635, 1280, 24}, 18, Theme::Sub, 1);
        }
    } else {
        fm.DrawTextCentered(S::WaitHost, {0, 560, 1280, 40}, 22, Theme::Sub, 1);
    }

    if (Button({40, 640, 160, 50}, S::BtnLeave, 22).Draw()) {
        if (mIsHost) {
            mHost->Stop();
            mMgr.SwitchTo(std::make_unique<MenuScene>(mMgr));
        } else {
            mClient->Disconnect();
            mMgr.SwitchTo(std::make_unique<MenuScene>(mMgr));
        }
    }

    if (mErrorTimer > 0 && !mClientError.empty()) {
        fm.DrawTextCentered(mClientError, {0, 600, 1280, 28}, 20, Theme::Warn, 1);
    }
}

void LobbyScene::DrawPlayerList(const std::vector<LobbyPlayer>& players, int selfId) {
    auto& fm = FontManager::Instance();
    float y = 170;
    for (const auto& p : players) {
        Rectangle row = {340, y, 600, 52};
        Color border = p.host ? Theme::Gold : (p.id == selfId ? Theme::AccentHi : Theme::PanelHi);
        DrawPanel(row);
        DrawRectangleRoundedLinesEx(row, 0.08f, 8, 2.0f, border);
        std::string label = p.name;
        if (p.host) label += S::TagHost;
        fm.DrawText(label, row.x + 18, y + 12, 22, p.id == selfId ? Theme::AccentHi : Theme::Text, 1);
        y += 64;
    }
}
