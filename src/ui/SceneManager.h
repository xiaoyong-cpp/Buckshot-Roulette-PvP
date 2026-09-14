// ============================================================================
// SceneManager.h - 场景管理器 + 主循环
// 持有当前场景，处理场景切换（排队切换，避免迭代中销毁自身）。
// ============================================================================
#pragma once

#include <memory>

#include "Scene.h"

class SceneManager {
public:
    // 进入主循环（initial 为初始场景）
    void Run(std::unique_ptr<Scene> initial);

    // 请求切换场景（下一帧生效）
    void SwitchTo(std::unique_ptr<Scene> next);
    void Quit() { mQuit = true; }

private:
    std::unique_ptr<Scene> mCurrent;
    std::unique_ptr<Scene> mNext;
    bool mQuit = false;
};
