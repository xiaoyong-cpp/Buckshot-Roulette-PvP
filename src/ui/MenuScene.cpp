#include "MenuScene.h"

#include <memory>

#include "../core/Strings.h"
#include "../modes/ModeFactory.h"
#include "../network/HostSession.h"
#include "../utils/ConfigLoader.h"
#include "CreateRoomScene.h"
#include "FontManager.h"
#include "GameScene.h"
#include "JoinScene.h"

void MenuScene::Update() {
    // 主菜单无逐帧逻辑（按钮的输入与绘制统一在 Draw 内完成，
    // 因为 raylib 要求所有绘制位于 BeginDrawing/EndDrawing 之间）
}

void MenuScene::Draw() {
    auto& fm = FontManager::Instance();

    // 标题
    Vector2 ts = fm.Measure(S::Title, 64, 2);
    fm.DrawText(S::Title, 640 - ts.x / 2, 100, 64, Theme::AccentHi, 2);
    Vector2 ss = fm.Measure(S::Subtitle, 20, 2);
    fm.DrawText(S::Subtitle, 640 - ss.x / 2, 175, 20, Theme::Sub, 2);
    fm.DrawText(S::MenuHint, 545, 210, 16, Theme::Sub, 1);

    if (Button({440, 250, 400, 70}, S::BtnSingle, 30).Draw()) {
        // 单机模式：本地会话（不启动服务器），玩家 vs 恶魔 AI
        GameConfig cfg;
        cfg.defaultHp = ConfigLoader::Instance().DefaultHp();
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = ConfigLoader::Instance().ItemsPerRoundMin();
        cfg.itemsPerRoundMax = ConfigLoader::Instance().ItemsPerRoundMax();
        auto session = std::make_shared<HostSession>("玩家", "", 0, 2, cfg, /*networked=*/false);
        if (session->StartGame("single")) {
            mMgr.SwitchTo(std::make_unique<GameScene>(mMgr, session));
        }
    }
    if (Button({440, 340, 400, 70}, S::BtnCreate, 30).Draw()) {
        mMgr.SwitchTo(std::make_unique<CreateRoomScene>(mMgr));
    }
    if (Button({440, 430, 400, 70}, S::BtnJoin, 30).Draw()) {
        mMgr.SwitchTo(std::make_unique<JoinScene>(mMgr));
    }
    if (Button({440, 550, 400, 50}, S::BtnQuit, 24).Draw()) {
        mMgr.Quit();
    }
}
