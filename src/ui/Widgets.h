// ============================================================================
// Widgets.h - 轻量 UI 控件（零命令行：全部鼠标操作）
// 自绘按钮 / 文本框 / 面板，不依赖 raygui，保证编译命令简洁。
// ============================================================================
#pragma once

#include <string>

#include "raylib.h"

// ---- 主题配色（暗色 + 血红，契合"恶魔轮盘"氛围）----
namespace Theme {
inline constexpr Color Bg{18, 16, 20, 255};           // 背景
inline constexpr Color Panel{32, 28, 36, 255};        // 面板
inline constexpr Color PanelHi{52, 44, 56, 255};       // 面板高亮
inline constexpr Color Accent{178, 34, 44, 255};       // 血红主色
inline constexpr Color AccentHi{220, 60, 70, 255};    // 血红高亮
inline constexpr Color Text{230, 225, 220, 255};      // 主文字
inline constexpr Color Sub{150, 145, 140, 255};       // 次文字
inline constexpr Color Live{220, 70, 70, 255};        // 实弹
inline constexpr Color Blank{150, 150, 160, 255};      // 空弹
inline constexpr Color Good{100, 200, 120, 255};       // 正面提示
inline constexpr Color Warn{240, 190, 90, 255};        // 警告
inline constexpr Color Dead{90, 88, 92, 255};          // 已淘汰
inline constexpr Color Gold{240, 200, 100, 255};      // 胜利
}  // namespace Theme

// ============================================================================
// 按钮：Draw() 每帧调用，返回本帧是否被点击
// ============================================================================
class Button {
public:
    Button() = default;
    Button(Rectangle rec, const std::string& label, float fontSize = 26)
        : mRec(rec), mLabel(label), mFontSize(fontSize) {}

    bool Draw();                       // 绘制 + 命中检测
    void SetEnabled(bool on) { mEnabled = on; }
    void SetLabel(const std::string& s) { mLabel = s; }
    void SetRec(Rectangle r) { mRec = r; }
    Rectangle Rec() const { return mRec; }
    bool IsHovered() const { return mHover; }

private:
    Rectangle mRec{};
    std::string mLabel;
    float mFontSize = 26;
    bool mEnabled = true;
    bool mHover = false;
};

// ============================================================================
// 文本框：点击聚焦后键盘输入（仅 ASCII 可打印字符，保证字体覆盖）
// ============================================================================
class TextBox {
public:
    TextBox() = default;
    TextBox(Rectangle rec, const std::string& placeholder = "", int maxLen = 16)
        : mRec(rec), mPlaceholder(placeholder), mMaxLen(maxLen) {}

    void Update();                     // 每帧调用：聚焦/输入
    void Draw() const;
    const std::string& Text() const { return mText; }
    void SetText(const std::string& t) { mText = t; }
    void SetNumeric(bool on) { mNumeric = on; }
    void SetMaxLen(int n) { mMaxLen = n; }
    bool Focused() const { return mFocused; }
    void Unfocus() { mFocused = false; }

private:
    Rectangle mRec{};
    std::string mText;
    std::string mPlaceholder;
    int mMaxLen = 16;
    bool mNumeric = false;
    bool mFocused = false;
    float mCaretTimer = 0;
};

// 面板背景
void DrawPanel(Rectangle rec, Color fill = Theme::Panel, float round = 8);

// 道具图标（纯图形绘制，type 见 items_config.json）
void DrawItemIcon(const std::string& type, Vector2 center, float size, Color tint);

// 生命值心形
void DrawHpBar(int hp, int maxHp, Rectangle rec);
