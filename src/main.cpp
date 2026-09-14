// ============================================================================
// main.cpp - 入口：仅负责初始化各单例并启动场景管理器（零命令行 UI）
// ============================================================================
#include <memory>

#include "raylib.h"

#include "core/Strings.h"
#include "items/ItemFactory.h"
#include "modes/ModeFactory.h"
#include "network/NetworkCore.h"
#include "ui/FontManager.h"
#include "ui/MenuScene.h"
#include "ui/SceneManager.h"
#include "utils/ConfigLoader.h"
#include "utils/Logger.h"

int main() {
    // 1. 配置（单例）：config.json，不存在则生成默认
    ConfigLoader::Instance().Load();

    // 2. 日志（单例）：全部输出到 logs/ 目录，无控制台输出
    Logger::Instance().Init("logs");
    Logger::Instance().Info("==== 恶魔轮盘启动 ====");

    // 3. Winsock 初始化（单例）
    NetworkCore::Instance().Init();

    // 4. 工厂初始化：道具（items_config.json）与模式（config.json）
    ItemFactory::Instance().Init();
    ModeFactory::Instance().Init();

    // 5. raylib 窗口
    int w = ConfigLoader::Instance().WindowWidth();
    int h = ConfigLoader::Instance().WindowHeight();
    InitWindow(w, h, "恶魔轮盘 Buckshot Roulette");
    SetTargetFPS(60);

    // 6. 中文字体（必须在 InitWindow 之后：依赖 raylib 的纹理系统）
    FontManager::Instance().Init();

    // 7. 场景管理器主循环
    SceneManager mgr;
    mgr.Run(std::make_unique<MenuScene>(mgr));

    // 8. 清理
    CloseWindow();
    NetworkCore::Instance().Shutdown();
    Logger::Instance().Info("==== 恶魔轮盘退出 ====");
    return 0;
}
