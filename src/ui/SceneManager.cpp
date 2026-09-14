#include "SceneManager.h"

#include "raylib.h"
#include "Widgets.h"

void SceneManager::Run(std::unique_ptr<Scene> initial) {
    mCurrent = std::move(initial);
    mCurrent->OnEnter();

    while (!WindowShouldClose() && !mQuit) {
        if (mNext) {  // 排队的场景切换
            mCurrent = std::move(mNext);
            mCurrent->OnEnter();
        }
        mCurrent->Update();
        BeginDrawing();
        ClearBackground(Theme::Bg);
        mCurrent->Draw();
        EndDrawing();
    }
}

void SceneManager::SwitchTo(std::unique_ptr<Scene> next) { mNext = std::move(next); }
