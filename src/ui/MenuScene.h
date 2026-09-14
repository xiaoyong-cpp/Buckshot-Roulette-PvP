// ============================================================================
// MenuScene.h - 主菜单（零命令行：全部鼠标操作）
// 三个大按钮：单机模式 / 创建房间 / 加入房间；额外模式按配置动态加载。
// ============================================================================
#pragma once

#include <vector>

#include "Scene.h"
#include "SceneManager.h"
#include "Widgets.h"

class MenuScene : public Scene {
public:
    explicit MenuScene(SceneManager& mgr) : mMgr(mgr) {}
    void Update() override;
    void Draw() override;

private:
    SceneManager& mMgr;
    std::vector<Button> mModeButtons;  // 配置中的扩展模式（动态加载）
};
