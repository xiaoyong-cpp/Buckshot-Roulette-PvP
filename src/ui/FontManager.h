// ============================================================================
// FontManager.h - 字体管理单例（设计模式：单例模式）
//
// raylib 默认字体不含 CJK 字形，本类在启动时：
//   1. 扫描 Strings.h 全部界面文案 + 道具/模式名 + assets/cjk_common.txt
//      （GB2312 一级常用字 3760 字，覆盖玩家昵称等动态文本）
//   2. 优先加载项目自带 assets/simhei.ttf（黑体，可自由分发，随 exe 携带），
//      确保开箱即用、不依赖系统 msyh.ttc（版权 + 可移植性）
//   3. 系统目录兜底（simhei.ttf / simsun.ttc / Deng.ttf）
// 保证全中文界面零命令行、零乱码。
// ============================================================================
#pragma once

#include <string>
#include <vector>

#include "raylib.h"

class FontManager {
public:
    static FontManager& Instance();  // 单例模式

    // 扫描语料并加载 CJK 字体（失败时回退 raylib 默认字体）
    bool Init(int baseSize = 32);

    // 统一文本绘制（UTF-8）
    void DrawText(const std::string& utf8Text, float x, float y, float fontSize,
                  Color color, float spacing = 1.0f) const;
    void DrawTextCentered(const std::string& utf8Text, Rectangle rec, float fontSize,
                          Color color, float spacing = 1.0f) const;
    Vector2 Measure(const std::string& utf8Text, float fontSize, float spacing = 1.0f) const;

    Font& GetFont() { return mFont; }
    bool IsCjkLoaded() const { return mCjkLoaded; }

private:
    FontManager() = default;
    Font mFont{};
    bool mCjkLoaded = false;
    bool mInited = false;
};
