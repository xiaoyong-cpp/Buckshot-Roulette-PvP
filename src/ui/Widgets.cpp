#include "Widgets.h"

#include "FontManager.h"

// ============================================================================
// 按钮绘制 + 命中检测：鼠标悬停高亮，左键释放于按钮内时触发点击
// ============================================================================
bool Button::Draw() {
    Vector2 m = GetMousePosition();
    mHover = CheckCollisionPointRec(m, mRec);
    Color fill = Theme::Panel;
    Color txt = Theme::Text;
    if (!mEnabled) {
        fill = {40, 38, 44, 255};
        txt = Theme::Sub;
    } else if (mHover) {
        fill = Theme::Accent;
        txt = WHITE;
    }
    DrawRectangleRounded(mRec, 0.25f, 8, fill);
    if (mHover && mEnabled)
        DrawRectangleRoundedLinesEx(mRec, 0.25f, 8, 2.0f, Theme::AccentHi);

    FontManager::Instance().DrawTextCentered(mLabel, mRec, mFontSize, txt, 1.0f);

    return mEnabled && mHover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

// ============================================================================
// 文本框：点击聚焦，键盘输入 ASCII 可打印字符，退格删除
// ============================================================================
void TextBox::Update() {
    Vector2 m = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        mFocused = CheckCollisionPointRec(m, mRec);
    }
    if (!mFocused) return;

    int c = GetCharPressed();
    while (c > 0) {
        if (c >= 32 && c < 127) {
            if (mNumeric && !(c >= '0' && c <= '9')) {
                c = GetCharPressed();
                continue;
            }
            if ((int)mText.size() < mMaxLen) mText.push_back((char)c);
        }
        c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !mText.empty()) mText.pop_back();
    mCaretTimer += GetFrameTime();
}

void TextBox::Draw() const {
    DrawRectangleRounded(mRec, 0.2f, 6, mFocused ? Theme::PanelHi : Theme::Panel);
    DrawRectangleRoundedLinesEx(mRec, 0.2f, 6, 2.0f, mFocused ? Theme::AccentHi : Theme::PanelHi);
    if (mText.empty() && !mPlaceholder.empty()) {
        FontManager::Instance().DrawText(mPlaceholder, mRec.x + 10, mRec.y + 8, 18, Theme::Sub, 1.0f);
    } else {
        FontManager::Instance().DrawText(mText, mRec.x + 10, mRec.y + 8, 20, Theme::Text, 1.0f);
    }
    if (mFocused && ((int)mCaretTimer * 2) % 2 == 0) {
        Vector2 sz = FontManager::Instance().Measure(mText.empty() ? "" : mText, 20, 1.0f);
        DrawLineEx({mRec.x + 12 + sz.x, mRec.y + 6}, {mRec.x + 12 + sz.x, mRec.y + mRec.height - 6},
                   2, Theme::Text);
    }
}

void DrawPanel(Rectangle rec, Color fill, float round) {
    DrawRectangleRounded(rec, 0.08f, 8, fill);
    DrawRectangleRoundedLinesEx(rec, 0.08f, 8, 1.0f, Theme::PanelHi);
}

// ============================================================================
// 道具图标：根据类型用基本图形绘制（放大镜/手锯/啤酒/手铐/肾上腺素）
// ============================================================================
void DrawItemIcon(const std::string& type, Vector2 c, float s, Color tint) {
    if (type == "magnifier") {
        DrawCircleLines((int)c.x, (int)c.y, s * 0.5f, tint);
        DrawLineEx({c.x + s * 0.35f, c.y + s * 0.35f}, {c.x + s * 0.9f, c.y + s * 0.9f}, 3, tint);
    } else if (type == "saw") {
        // 锯齿
        Vector2 prev = {c.x - s * 0.7f, c.y};
        for (int i = 0; i < 6; ++i) {
            float x = c.x - s * 0.7f + (i + 1) * (s * 1.4f / 6);
            Vector2 cur = {x, c.y + ((i % 2) ? s * 0.35f : -s * 0.1f)};
            DrawLineEx(prev, cur, 3, tint);
            prev = cur;
        }
    } else if (type == "beer") {
        DrawRectangleRounded({c.x - s * 0.35f, c.y - s * 0.5f, s * 0.7f, s}, 0.3f, 4, tint);
        DrawLineEx({c.x - s * 0.35f, c.y - s * 0.3f}, {c.x + s * 0.35f, c.y - s * 0.3f}, 2, tint);
    } else if (type == "handcuff") {
        DrawCircleLines((int)(c.x - s * 0.35f), (int)c.y, s * 0.32f, tint);
        DrawCircleLines((int)(c.x + s * 0.35f), (int)c.y, s * 0.32f, tint);
        DrawLineEx({c.x - s * 0.05f, c.y - s * 0.05f}, {c.x + s * 0.05f, c.y - s * 0.05f}, 3, tint);
    } else if (type == "adrenaline") {
        // 注射器
        DrawRectangle((int)(c.x - s * 0.1f), (int)(c.y - s * 0.4f), (int)(s * 0.2f), (int)(s * 0.7f), tint);
        DrawLineEx({c.x, c.y + s * 0.3f}, {c.x, c.y + s * 0.6f}, 2, tint);
        DrawLineEx({c.x - s * 0.15f, c.y - s * 0.4f}, {c.x + s * 0.15f, c.y - s * 0.4f}, 3, tint);
    } else if (type == "cigarette") {
        // 香烟：白色烟杆 + 橙色烟头 + 烟雾
        DrawRectangle((int)(c.x - s * 0.5f), (int)(c.y - s * 0.1f), (int)(s * 0.8f), (int)(s * 0.2f), tint);
        DrawRectangle((int)(c.x + s * 0.3f), (int)(c.y - s * 0.1f), (int)(s * 0.2f), (int)(s * 0.2f),
                     {240, 120, 60, 255});  // 烟头
        // 烟雾线
        DrawLineEx({c.x + s * 0.5f, c.y - s * 0.1f}, {c.x + s * 0.6f, c.y - s * 0.4f}, 2, tint);
        DrawLineEx({c.x + s * 0.55f, c.y - s * 0.1f}, {c.x + s * 0.7f, c.y - s * 0.5f}, 1, tint);
    } else if (type == "expiredmed") {
        // 过期药品：药瓶 + 变质标记
        DrawRectangleRounded({c.x - s * 0.3f, c.y - s * 0.35f, s * 0.6f, s * 0.7f}, 0.2f, 4, tint);
        DrawRectangle((int)(c.x - s * 0.15f), (int)(c.y - s * 0.5f), (int)(s * 0.3f), (int)(s * 0.2f), tint);
        // 变质 X 标记
        DrawLineEx({c.x - s * 0.15f, c.y - s * 0.1f}, {c.x + s * 0.15f, c.y + s * 0.2f}, 2, {200, 60, 60, 255});
        DrawLineEx({c.x + s * 0.15f, c.y - s * 0.1f}, {c.x - s * 0.15f, c.y + s * 0.2f}, 2, {200, 60, 60, 255});
    } else if (type == "reverser") {
        // 反转器：双向箭头圆环
        DrawCircleLines((int)c.x, (int)c.y, s * 0.45f, tint);
        // 上箭头（向左）
        DrawLineEx({c.x - s * 0.3f, c.y - s * 0.15f}, {c.x + s * 0.3f, c.y - s * 0.15f}, 3, tint);
        DrawLineEx({c.x - s * 0.3f, c.y - s * 0.15f}, {c.x - s * 0.1f, c.y - s * 0.3f}, 3, tint);
        DrawLineEx({c.x - s * 0.3f, c.y - s * 0.15f}, {c.x - s * 0.1f, c.y + s * 0.0f}, 3, tint);
        // 下箭头（向右）
        DrawLineEx({c.x - s * 0.3f, c.y + s * 0.15f}, {c.x + s * 0.3f, c.y + s * 0.15f}, 3, tint);
        DrawLineEx({c.x + s * 0.3f, c.y + s * 0.15f}, {c.x + s * 0.1f, c.y + s * 0.3f}, 3, tint);
        DrawLineEx({c.x + s * 0.3f, c.y + s * 0.15f}, {c.x + s * 0.1f, c.y + s * 0.0f}, 3, tint);
    } else {
        DrawCircleV(c, s * 0.4f, tint);
    }
}

// ============================================================================
// 生命值：实心心形条 + 位置（左下角缺口表示已损）
// ============================================================================
void DrawHpBar(int hp, int maxHp, Rectangle rec) {
    DrawPanel({rec.x - 2, rec.y - 2, rec.width + 4, rec.height + 4}, {20, 18, 22, 255}, 0.1f);
    float step = rec.width / maxHp;
    for (int i = 0; i < maxHp; ++i) {
        Rectangle seg = {rec.x + i * step, rec.y, step - 2, rec.height};
        Color c = (i < hp) ? (Color){220, 60, 70, 255} : (Color){60, 50, 55, 255};
        DrawRectangleRounded(seg, 0.4f, 4, c);
    }
}
