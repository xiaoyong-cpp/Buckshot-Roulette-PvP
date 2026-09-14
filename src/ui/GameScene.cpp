#include "GameScene.h"

#include <cmath>

#include "../core/Game.h"
#include "../core/Strings.h"
#include "../items/ItemFactory.h"
#include "../network/ClientSession.h"
#include "../network/HostSession.h"
#include "../utils/Logger.h"
#include "FontManager.h"
#include "MenuScene.h"

// ---- 布局常量（1280x720）----
static const Rectangle PANEL_RIGHT{1016, 74, 254, 560};
static const Rectangle LOG_PANEL{16, 90, 420, 330};
static const Vector2 GUN_POS{500, 310};  // 枪居中偏右，避免被左侧 LOG_PANEL 和右侧玩家 HUD 遮挡
static const Rectangle BANNER_REC{240, 395, 520, 68};

// ---- 背包 / 道具栏布局（屏幕底侧：横向 30%~70%，纵向 25%）----
// 8 格 = 4列 × 2行网格，在上述区域内均匀铺满
static const float INV_LEFT   = 1280.0f * 0.30f;          // 384
static const float INV_RIGHT  = 1280.0f * 0.70f;          // 896
static const float INV_TOP    = 720.0f  * (1.0f - 0.25f); // 底侧 25% = 从 y=540 开始
static const float INV_BOTTOM = 720.0f;
static const float INV_W      = INV_RIGHT - INV_LEFT;     // 512
static const float INV_H      = INV_BOTTOM - INV_TOP;     // 180
static const int   INV_COLS   = 4;
static const int   INV_ROWS   = 2;
static const float INV_GAP    = 10.0f;
static const float INV_SLOT_W = (INV_W - (INV_COLS + 1) * INV_GAP) / INV_COLS;
static const float INV_SLOT_H = (INV_H - (INV_ROWS + 1) * INV_GAP) / INV_ROWS;

// 计算第 idx（0..7）个槽的位置（row-first：0..3 第一行，4..7 第二行）
static Rectangle InventorySlotRect(int idx) {
    int r = idx / INV_COLS;
    int c = idx % INV_COLS;
    float x = INV_LEFT + INV_GAP + c * (INV_SLOT_W + INV_GAP);
    float y = INV_TOP  + INV_GAP + r * (INV_SLOT_H + INV_GAP);
    return {x, y, INV_SLOT_W, INV_SLOT_H};
}

// ============================================================================
// 构造 / 析构
// ============================================================================
GameScene::GameScene(SceneManager& mgr, std::shared_ptr<HostSession> host)
    : mMgr(mgr), mIsHost(true), mHost(std::move(host)) {}

GameScene::GameScene(SceneManager& mgr, std::shared_ptr<ClientSession> client)
    : mMgr(mgr), mIsHost(false), mClient(std::move(client)) {
    // 注册客户端回调（覆盖 LobbyScene 的回调）
    mClient->OnState = [this](const GameView& v) {
        mPendingView = v;
        mHasPending = true;
    };
    mClient->OnEvent = [this](const GameEvent& e) { HandleEvent(e); };
    mClient->OnError = [this](const std::string& msg) { ShowToast(msg); };
    mClient->OnDisconnected = [this]() {
        mDisconnected = true;
        mDisconnectAt = GetTime();
        mRetryTimer = 3.0;
        AddLog(S::StatusDisconnected, Theme::Warn);
    };
    mClient->OnLobby = nullptr;
    mClient->OnStart = nullptr;
}

GameScene::~GameScene() {
    if (mGame) mGame->RemoveObserver(this);
    if (mClient) {
        mClient->OnState = nullptr;
        mClient->OnEvent = nullptr;
        mClient->OnError = nullptr;
        mClient->OnDisconnected = nullptr;
    }
}

void GameScene::OnEnter() {
    if (mIsHost) {
        mGame = mHost->GamePtr();
        if (mGame) {
            mGame->AddObserver(this);  // 观察者模式：UI 订阅游戏事件
            BuildLocalView();
        }
    } else {
        mClient->Update();  // 应用已到达的 start / state 消息
        ApplyClientView();
    }
}

// ============================================================================
// 每帧更新：动画推进 + 会话驱动 + 视图刷新
// ============================================================================
void GameScene::Update() {
    double dt = GetFrameTime();
    double now = GetTime();

    // 动画计时
    mBannerT -= dt;
    mToastT -= dt;
    mInputLock -= dt;
    mFlashT -= dt;
    if (mShot.active) {
        mShot.t += dt;
        if (mShot.t > 1.4) mShot.active = false;
    }
    mShake *= 0.88f;
    if (mShake < 0.1f) mShake = 0;
    float da = mGunTarget - mGunAngle;
    mGunAngle += da * (float)std::min(1.0, dt * 8.0);

    if (mIsHost) {
        // 主机/单机：推进权威逻辑（含 AI 托管与状态同步）
        mHost->Update(dt);
        BuildLocalView();
    } else {
        if (mDisconnected) {
            // 断线重连：60 秒内自动重试，超时返回菜单
            mClient->Update();
            if (mClient->Joined()) {
                mDisconnected = false;
                ShowToast(S::BtnReconnect);
            } else if (!mClient->IsConnected() && mRetryTimer <= 0 && now - mDisconnectAt < 55.0) {
                mRetryTimer = 3.0;
                mClient->Connect(mClient->ServerIp(), mClient->ServerPort(),
                                 mClient->Name(), mClient->Token());
            }
            if (mRetryTimer > 0) mRetryTimer -= dt;
            if (now - mDisconnectAt > 60.0) {
                mClient->Disconnect();
                mMgr.SwitchTo(std::make_unique<MenuScene>(mMgr));
                return;
            }
        } else {
            mClient->Update();
        }
        ApplyClientView();
    }
}

// ============================================================================
// 视图构建
// ============================================================================
void GameScene::BuildLocalView() {
    if (!mGame) return;
    GameView v;
    v.valid = true;
    v.pvp = mGame->IsPvp();
    v.state = mGame->GetState();
    v.round = mGame->Round();
    v.turnId = mGame->CurrentTurnPlayerId();
    v.winnerId = mGame->WinnerId();
    v.selfId = mHost->MyId();
    v.roomName = mHost->RoomName();
    if (mGame->Mode()) v.modeId = mGame->Mode()->GetId();

    const Chamber& ch = mGame->GetChamber();
    v.shellCount = ch.Count();
    v.shellIndex = ch.Index();
    v.liveLeft = ch.LiveRemaining();
    v.blankLeft = ch.BlankRemaining();
    v.sawed = ch.sawed;

    for (const auto& p : mGame->Players()) {
        ViewPlayer vp;
        vp.id = p.id;
        vp.name = p.name;
        vp.hp = p.hp;
        vp.maxHp = p.maxHp;
        vp.alive = p.alive;
        vp.connected = p.connected;
        vp.skipNext = p.skipNextTurn;
        vp.isTurn = (p.id == v.turnId);
        vp.itemCount = p.ItemCount();
        vp.isSelf = (p.id == v.selfId);
        if (vp.isSelf) {
            vp.items = p.items;  // 道具明细仅本人可见
            v.known = p.knownShellValid;
            v.knownLive = p.knownShellLive;
        }
        v.players.push_back(vp);
    }
    for (const auto& vp : v.players)
        if (vp.id == v.winnerId) v.winnerName = vp.name;

    CheckKnownEdge(v.known, v.knownLive);
    mView = v;
}

void GameScene::ApplyClientView() {
    if (!mHasPending) return;
    mHasPending = false;
    CheckKnownEdge(mPendingView.known, mPendingView.knownLive);
    mView = mPendingView;
}

void GameScene::CheckKnownEdge(bool newKnown, bool newLive) {
    if (newKnown && !mPrevKnown) {
        ShowBanner(newLive ? S::LabelKnownLive : S::LabelKnownBlank,
                    newLive ? Theme::Live : Theme::Blank, 1.5);
    }
    mPrevKnown = newKnown;
}

// ============================================================================
// 事件处理（主机观察者 + 客户端 event 消息共用）
// ============================================================================
void GameScene::OnGameEvent(const GameEvent& event) { HandleEvent(event); }

void GameScene::HandleEvent(const GameEvent& e) {
    switch (e.kind) {
        case EventKind::GameStart:
            AddLog(S::LogStart, Theme::Good);
            mInputLock = 1.0;
            break;
        case EventKind::Reload: {
            std::string t = S::Replace(S::Replace(S::BannerReload, "l", std::to_string(e.value)), "b",
                                       std::to_string(e.value2));
            ShowBanner(t, Theme::Sub, 1.5);
            AddLog(t, Theme::Sub);
            mGunTarget = 0;
            mInputLock = 1.3;
            break;
        }
        case EventKind::Shot: {
            mShot.active = true;
            mShot.t = 0;
            mShot.shooter = e.playerId;
            mShot.target = e.targetId;
            mShot.self = (e.targetId == -1);
            mShot.live = e.flag;
            mShot.dmg = e.value;

            std::string a = LookupName(e.playerId);
            std::string b = (e.targetId == -1) ? a : LookupName(e.targetId);
            std::string text;
            if (mShot.self) {
                text = mShot.live ? S::Replace(S::Replace(S::LogShootSelfLive, "a", a), "d",
                                              std::to_string(e.value))
                                  : S::Replace(S::LogShootSelfBlank, "a", a);
            } else {
                text = mShot.live ? S::Replace(S::Replace(S::Replace(S::LogShootTargetLive, "a", a), "b", b),
                                              "d", std::to_string(e.value))
                                  : S::Replace(S::Replace(S::LogShootTargetBlank, "a", a), "b", b);
            }
            Color c = mShot.live ? Theme::Live : Theme::Blank;
            AddLog(text, c);
            ShowBanner(text, c, 1.3);

            // 枪口指向目标
            if (mShot.self) {
                mGunTarget = 100.0f;
            } else {
                Vector2 t = RowCenter(e.targetId);
                mGunTarget = atan2f(t.y - GUN_POS.y, t.x - GUN_POS.x) * RAD2DEG;
            }
            mFlashT = 0.18;
            if (mShot.live) mShake = 14.0f;
            mInputLock = 1.0;
            break;
        }
        case EventKind::ExtraTurn:
            ShowBanner(S::BannerExtra, Theme::Good, 1.2);
            break;
        case EventKind::BeerEject: {
            std::string a = LookupName(e.playerId);
            AddLog(S::Replace(e.flag ? S::LogBeerLive : S::LogBeerBlank, "a", a), Theme::Warn);
            mInputLock = 0.5;
            break;
        }
        case EventKind::ItemUsed: {
            std::string a = LookupName(e.playerId);
            if (e.text == "handcuff") {
                AddLog(S::Replace(S::Replace(S::LogHandcuff, "a", a), "b", LookupName(e.targetId)),
                       Theme::Warn);
            } else {
                AddLog(S::Replace(S::Replace(S::LogItemUsed, "a", a), "it", ItemName(e.text)),
                       Theme::Text);
            }
            mInputLock = 0.5;
            break;
        }
        case EventKind::PlayerHealed: {
            std::string a = LookupName(e.playerId);
            std::string text;
            if (e.text == "expiredmed_good") {
                text = S::Replace(S::LogExpiredMedGood, "a", a);
                text = S::Replace(text, "d", std::to_string(e.value));
                AddLog(text, Theme::Good);
            } else if (e.text == "expiredmed_bad") {
                // 已由 PlayerDamaged 处理，这里跳过避免重复
            } else {
                // 香烟
                text = S::Replace(S::LogCigarette, "a", a);
                text = S::Replace(text, "d", std::to_string(e.value));
                AddLog(text, Theme::Good);
                ShowBanner(text, Theme::Good, 1.2);
            }
            mInputLock = 0.5;
            break;
        }
        case EventKind::Steal: {
            std::string a = LookupName(e.playerId);
            std::string b = LookupName(e.targetId);
            AddLog(S::Replace(S::Replace(S::Replace(S::LogSteal, "a", a), "b", b), "it",
                              ItemName(e.text)),
                   Theme::Warn);
            mInputLock = 0.5;
            break;
        }
        case EventKind::TurnSkip:
            AddLog(S::Replace(S::LogSkip, "p", LookupName(e.playerId)), Theme::Sub);
            mInputLock = 0.6;
            break;
        case EventKind::PlayerEliminated: {
            std::string p = LookupName(e.targetId);
            AddLog(S::Replace(S::LogElim, "p", p), Theme::Live);
            ShowBanner(S::Replace(S::BannerElim, "p", p), Theme::Live, 1.6);
            mShake = 18.0f;
            break;
        }
        case EventKind::GameOver: {
            std::string p = LookupName(e.targetId);
            AddLog(S::Replace(S::LogWin, "p", p), Theme::Gold);
            ShowBanner(S::Replace(S::BannerWin, "p", p), Theme::Gold, 3.0);
            break;
        }
        case EventKind::PlayerConnect:
            AddLog(S::Replace(S::LogReconnect, "p", LookupName(e.playerId)), Theme::Good);
            break;
        case EventKind::PlayerDisconnect:
            AddLog(S::Replace(S::LogDisconnect, "p", LookupName(e.playerId)), Theme::Warn);
            break;
        case EventKind::PlayerDamaged: {
            // 过期药品扣血（子弹伤害由 Shot 事件已显示，这里只处理道具伤害）
            std::string a = LookupName(e.targetId);
            std::string text;
            if (e.text == "expiredmed_bad") {
                text = S::Replace(S::LogExpiredMedBad, "a", a);
                text = S::Replace(text, "d", std::to_string(e.value));
                AddLog(text, Theme::Live);
                ShowBanner(text, Theme::Live, 1.2);
            }
            mInputLock = 0.5;
            break;
        }
        case EventKind::Info: {
            // 区分两种 Info：
            //   1. 背包已满丢弃道具：e.text 是道具类型标识（如 "magnifier"）
            //   2. 反转器/香烟满血等提示：e.text 是中文描述
            std::string a = LookupName(e.playerId);
            if (e.playerId >= 0 && ItemFactory::Instance().FindDef(e.text)) {
                // 背包已满
                AddLog(S::Replace(S::LogBagFull, "it", ItemName(e.text)), Theme::Sub);
            } else if (e.text.rfind("反转器", 0) == 0) {
                // 反转器
                AddLog(a + " 使用了" + e.text, Theme::Warn);
                ShowBanner(a + " 使用了" + e.text, Theme::Warn, 1.2);
            } else if (e.text == "满血，香烟无效果") {
                AddLog(S::Replace(S::LogCigaretteFull, "a", a), Theme::Sub);
            } else {
                AddLog(e.text, Theme::Sub);
            }
            break;
        }
        default:
            break;
    }
}

void GameScene::AddLog(const std::string& text, Color color) {
    mLog.push_back({text, color});
    if (mLog.size() > 50) mLog.erase(mLog.begin());
    Logger::Instance().Info("日志: " + text);
}

void GameScene::ShowBanner(const std::string& text, Color color, double seconds) {
    mBanner = text;
    mBannerColor = color;
    mBannerT = seconds;
}

void GameScene::ShowToast(const std::string& text) {
    mToast = text;
    mToastT = 2.0;
}

std::string GameScene::LookupName(int id) const {
    for (const auto& vp : mView.players)
        if (vp.id == id) return vp.name;
    return "?";
}

std::string GameScene::ItemName(const std::string& type) const {
    return ItemFactory::Instance().DisplayName(type);
}

// ============================================================================
// 输入与动作分发
// ============================================================================
bool GameScene::CanAct() const {
    if (!mView.valid) return false;
    if (mView.state != GameState::Aiming) return false;
    if (mView.turnId != mView.selfId) return false;
    if (mInputLock > 0) return false;
    if (mShot.active && mShot.t < 0.9) return false;
    return true;
}

void GameScene::PerformShoot(int targetId) {
    mTargeting = false;
    if (mIsHost) {
        std::string err;
        if (!mGame->Shoot(mHost->MyId(), targetId, &err)) ShowToast(err);
    } else {
        mClient->SendShoot(targetId);
    }
}

void GameScene::PerformUseItem(const std::string& type, int targetId) {
    mTargeting = false;
    if (mIsHost) {
        std::string err;
        if (!mGame->UseItem(mHost->MyId(), type, targetId, &err)) {
            // 使用失败（如肾上腺素目标无道具）：提示并恢复到瞄准状态
            ShowToast(err);
            mGame->CancelItemTarget();  // 退出 ItemSelect 状态，回到 Aiming
        }
    } else {
        mClient->SendUseItem(type, targetId);
    }
}

void GameScene::CancelTargeting() {
    if (!mTargeting) return;
    mTargeting = false;
    mTargetItem.clear();
    if (mIsHost) mGame->CancelItemTarget();
}

Vector2 GameScene::RowCenter(int playerId) const {
    for (size_t i = 0; i < mView.players.size(); ++i) {
        if (mView.players[i].id == playerId)
            return {1022 + 121, 82 + (float)i * 68 + 29};
    }
    return {1150, 320};
}

// ============================================================================
// 渲染
// ============================================================================
void GameScene::Draw() {
    Vector2 shake{0, 0};
    if (mShake > 0.1f) {
        shake.x = GetRandomValue(-10, 10) / 10.0f * mShake;
        shake.y = GetRandomValue(-10, 10) / 10.0f * mShake;
    }
    DrawTopBar();
    DrawStage();
    DrawPlayerPanel();
    DrawItems();
    DrawLog();
    DrawOverlays();
    (void)shake;  // 震屏通过枪与横幅偏移体现
}

void GameScene::DrawTopBar() {
    auto& fm = FontManager::Instance();
    DrawRectangle(0, 0, 1280, 64, {26, 22, 30, 255});
    DrawLineEx({0, 64}, {1280, 64}, 2, Theme::PanelHi);

    // 回合
    fm.DrawText(S::Replace(S::LabelRound, "n", std::to_string(mView.round)), 20, 18, 24, Theme::Text, 1);

    // 弹仓槽位（当前槽高亮；本人窥探过的槽显示实/空颜色）
    int n = mView.shellCount;
    float slotsW = n * 30;
    float sx = 660 - slotsW / 2;
    for (int i = 0; i < n; ++i) {
        Vector2 c{sx + i * 30 + 10, 32};
        if (i < mView.shellIndex) {
            DrawCircleV(c, 9, {45, 42, 50, 255});  // 已消耗
        } else if (i == mView.shellIndex) {
            Color col{240, 200, 100, 255};
            if (mView.known) col = mView.knownLive ? Theme::Live : Theme::Blank;
            DrawCircleV(c, 11, col);
            DrawCircleLines((int)c.x, (int)c.y, 11, WHITE);
        } else {
            DrawCircleV(c, 9, {70, 66, 76, 255});  // 未知
        }
    }
    if (n == 0 && mView.valid) {
        fm.DrawTextCentered(S::LabelChamberEmpty, {560, 16, 200, 32}, 18, Theme::Warn, 1);
    }

    // 剩余实/空计数（公开信息）
    std::string cnt = std::string(S::LabelLive) + " " + std::to_string(mView.liveLeft) + "   " +
                      S::LabelBlank + " " + std::to_string(mView.blankLeft);
    fm.DrawText(cnt, 800, 20, 22, Theme::Sub, 1);

    // 手锯 / 窥探状态
    if (mView.sawed) fm.DrawText(S::LabelSawed, 960, 8, 20, Theme::Warn, 1);
    if (mView.known)
        fm.DrawText(mView.knownLive ? S::LabelKnownLive : S::LabelKnownBlank, 960, 36, 16,
                    mView.knownLive ? Theme::Live : Theme::Blank, 1);
}

void GameScene::DrawStage() {
    auto& fm = FontManager::Instance();

    // 回合提示
    std::string turnText;
    Color turnColor = Theme::Sub;
    if (mView.state == GameState::GameOver) {
        turnText.clear();
    } else if (mView.turnId == mView.selfId) {
        turnText = S::YourTurn;
        turnColor = Theme::Good;
    } else {
        turnText = S::Replace(S::WaitingTurn, "n", LookupName(mView.turnId));
    }
    if (!turnText.empty())
        fm.DrawTextCentered(turnText, {100, 80, 880, 28}, 20, turnColor, 1);

    // 手枪（带震屏偏移）
    Vector2 gun = GUN_POS;
    if (mShake > 0.1f) {
        gun.x += GetRandomValue(-10, 10) / 10.0f * mShake;
        gun.y += GetRandomValue(-10, 10) / 10.0f * mShake;
    }
    DrawRevolver(gun, mGunAngle);

    // 结果横幅
    if (mBannerT > 0 && !mBanner.empty()) {
        float alpha = mBannerT < 0.3 ? (float)(mBannerT / 0.3) : 1.0f;
        Color bg{10, 8, 12, (unsigned char)(alpha * 170)};
        Color c = mBannerColor;
        c.a = (unsigned char)(alpha * 255);
        DrawRectangleRec(BANNER_REC, bg);
        fm.DrawTextCentered(mBanner, BANNER_REC, 26, c, 1);
    }

    // 操作按钮
    if (CanAct() && !mTargeting) {
        bool doSelf = Button({560, 480, 210, 56}, S::BtnShootSelf, 24).Draw();
        if (IsKeyPressed(KEY_SPACE)) doSelf = true;
        if (doSelf) PerformShoot(-1);

        if (mView.pvp) {
            if (Button({560, 550, 210, 56}, S::BtnShootTarget, 24).Draw()) {
                mTargeting = true;
                mTargetShoot = true;
            }
        } else {
            // 单机：直接射击恶魔（唯一对手）
            if (Button({560, 550, 210, 56}, S::BtnShootDemon, 24).Draw()) {
                for (const auto& vp : mView.players) {
                    if (vp.alive && !vp.isSelf) {
                        PerformShoot(vp.id);
                        break;
                    }
                }
            }
        }
    }

    // 目标选择提示
    if (mTargeting) {
        fm.DrawTextCentered(S::HintChooseTarget, {100, 470, 880, 26}, 20, Theme::Warn, 1);
        if (Button({790, 545, 120, 46}, S::BtnCancel, 20).Draw() || IsKeyPressed(KEY_ESCAPE)) {
            CancelTargeting();
        }
    }

    // Toast
    if (mToastT > 0 && !mToast.empty()) {
        float alpha = mToastT < 0.3 ? (float)(mToastT / 0.3) : 1.0f;
        Color c = Theme::Warn;
        c.a = (unsigned char)(alpha * 255);
        fm.DrawTextCentered(mToast, {340, 116, 600, 26}, 18, c, 1);
    }
}

void GameScene::DrawPlayerPanel() {
    auto& fm = FontManager::Instance();
    DrawPanel(PANEL_RIGHT);
    fm.DrawText(S::LabelPlayers, 1022, 78, 18, Theme::Sub, 1);

    Vector2 mouse = GetMousePosition();
    int i = 0;
    for (const auto& vp : mView.players) {
        Rectangle row = {1022, 106 + (float)i * 68, 242, 58};
        mRows[vp.id] = row;

        Color border = Theme::PanelHi;
        if (!vp.alive) border = Theme::Dead;
        else if (vp.isTurn) border = Theme::Gold;
        if (vp.isSelf && vp.alive) border = Theme::AccentHi;

        Color fill = vp.alive ? Theme::Panel : (Color){26, 24, 28, 255};
        // 目标选择时：可选目标闪烁高亮
        bool selectable = mTargeting && vp.alive && !vp.isSelf;
        // 肾上腺素目标无道具时：标灰不可选
        bool noItemTarget = mTargeting && !mTargetShoot && mTargetItem == "adrenaline" &&
                            vp.itemCount == 0 && vp.alive && !vp.isSelf;
        if (noItemTarget) {
            fill = {40, 38, 44, 255};
            border = {70, 68, 72, 255};  // 灰色：无效目标
        } else if (selectable && CheckCollisionPointRec(mouse, row)) {
            fill = Theme::PanelHi;
            border = Theme::Warn;
        }
        DrawPanel(row, fill);
        DrawRectangleRoundedLinesEx(row, 0.08f, 8, 2.0f, border);

        // 名字 + 标签
        std::string name = vp.name;
        if (vp.isSelf) name += S::TagSelf;
        if (mView.pvp && vp.id == 0) name += S::TagHost;
        fm.DrawText(name, row.x + 10, row.y + 6, 17,
                    vp.alive ? Theme::Text : Theme::Dead, 1);
        // 状态标签
        float tagX = row.x + 10;
        float tagY = row.y + 30;
        if (!vp.connected) {
            fm.DrawText(S::TagDisconnected, tagX, tagY, 14, Theme::Warn, 1);
            tagX += 48;
        }
        if (vp.skipNext) {
            fm.DrawText(S::TagCuffed, tagX, tagY, 14, Theme::Sub, 1);
            tagX += 40;
        }
        // 道具数量
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "%d", vp.itemCount);
        std::string items = std::string(S::LabelItems) + "x" + cnt;
        fm.DrawText(items, row.x + row.width - 64, row.y + 30, 14, Theme::Sub, 1);

        // 生命条
        DrawHpBar(vp.hp, vp.maxHp, {row.x + 10, row.y + 44, 150, 8});

        // 目标选择点击（肾上腺素目标无道具时禁止选择并提示）
        if (selectable && !noItemTarget && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(mouse, row)) {
            if (mTargetShoot) {
                PerformShoot(vp.id);
            } else {
                PerformUseItem(mTargetItem, vp.id);
            }
        }
        if (noItemTarget && CheckCollisionPointRec(mouse, row) &&
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            ShowToast("对方没有道具，无法使用肾上腺素");
        }
        ++i;
    }
}

void GameScene::DrawItems() {
    auto& fm = FontManager::Instance();

    // 底栏背景（屏幕横向 30%-70%、纵向底侧 25% 铺满）
    DrawRectangle((int)INV_LEFT, (int)INV_TOP, (int)INV_W, (int)INV_H, {22, 18, 26, 255});
    DrawLineEx({INV_LEFT, INV_TOP}, {INV_RIGHT, INV_TOP}, 2, Theme::PanelHi);
    DrawLineEx({INV_LEFT, INV_TOP}, {INV_LEFT, INV_BOTTOM}, 2, Theme::PanelHi);
    DrawLineEx({INV_RIGHT - 1, INV_TOP}, {INV_RIGHT - 1, INV_BOTTOM}, 2, Theme::PanelHi);

    const ViewPlayer* self = nullptr;
    for (const auto& vp : mView.players)
        if (vp.isSelf) self = &vp;

    Vector2 mouse = GetMousePosition();
    // 绘制所有 8 个槽（空槽显示占位）
    for (int slot = 0; slot < MAX_BACKPACK; ++slot) {
        Rectangle slotRect = InventorySlotRect(slot);
        bool hasItem = (self != nullptr && slot < (int)self->items.size());
        std::string type = hasItem ? self->items[slot] : "";
        bool hover = hasItem && CheckCollisionPointRec(mouse, slotRect);

        Color fill  = Theme::Panel;
        Color border = Theme::PanelHi;
        if (hasItem) {
            fill = hover ? Theme::PanelHi : Theme::Panel;
            border = hover && CanAct() ? Theme::AccentHi : Theme::PanelHi;
        } else {
            // 空槽：暗底 + 虚边框
            fill = (Color){18, 15, 22, 255};
        }

        DrawPanel(slotRect, fill);
        if (hasItem && hover && CanAct()) {
            DrawRectangleRoundedLinesEx(slotRect, 0.10f, 8, 2.0f, Theme::AccentHi);
        } else if (hasItem) {
            DrawRectangleRoundedLinesEx(slotRect, 0.10f, 8, 1.5f, border);
        } else {
            DrawRectangleRoundedLinesEx(slotRect, 0.10f, 8, 1.0f, (Color){50, 46, 58, 255});
        }

        if (hasItem) {
            // 图标尺寸自适应（按较小边缩放，图标正方形居中）
            float iconSize = std::min(INV_SLOT_W, INV_SLOT_H) * 0.46f;
            float iconX    = slotRect.x + slotRect.width / 2 - iconSize / 2;
            float iconY    = slotRect.y + slotRect.height * 0.18f;
            DrawItemIcon(type, {iconX, iconY}, iconSize, Theme::Text);

            // 道具名（下 1/3 区域居中），字号根据格宽自适应
            int fontSize = (int)std::max(11.0f, INV_SLOT_W * 0.17f);
            Rectangle nameRect{slotRect.x, slotRect.y + slotRect.height * 0.60f,
                               slotRect.width, slotRect.height * 0.35f};
            fm.DrawTextCentered(ItemName(type), nameRect, fontSize, Theme::Sub, 1);

            // 点击使用
            if (hover && CanAct() && !mTargeting && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                const ItemDef* def = ItemFactory::Instance().FindDef(type);
                bool needsTarget = def ? def->needsTarget : false;
                if (needsTarget) {
                    mTargeting = true;
                    mTargetShoot = false;
                    mTargetItem = type;
                    if (mIsHost) {
                        std::string err;
                        mGame->BeginItemTarget(mHost->MyId(), type, &err);
                    }
                } else {
                    PerformUseItem(type);
                }
            }
        }
    }
}

void GameScene::DrawLog() {
    auto& fm = FontManager::Instance();
    DrawPanel(LOG_PANEL, {20, 18, 24, 200});
    size_t maxLines = 9;
    size_t start = mLog.size() > maxLines ? mLog.size() - maxLines : 0;
    float y = 100;
    for (size_t k = start; k < mLog.size(); ++k) {
        Color c = mLog[k].color;
        c.a = 200;
        fm.DrawText(mLog[k].text, 26, y, 15, c, 0.5f);
        y += 34;
    }
}

void GameScene::DrawOverlays() {
    auto& fm = FontManager::Instance();

    // 终局覆盖层
    if (mView.valid && mView.state == GameState::GameOver) {
        DrawRectangle(0, 0, 1280, 720, {0, 0, 0, 170});
        fm.DrawTextCentered(S::Replace(S::BannerWin, "p", mView.winnerName),
                            {0, 220, 1280, 80}, 48, Theme::Gold, 2);
        if (Button({510, 350, 260, 64}, S::BtnBackToMenu, 26).Draw()) {
            if (mIsHost) {
                if (mGame) {
                    mGame->RemoveObserver(this);
                    mGame = nullptr;
                }
                mHost->Stop();
            } else {
                mClient->Disconnect();
            }
            mMgr.SwitchTo(std::make_unique<MenuScene>(mMgr));
        }
    }

    // 客户端断线覆盖层
    if (!mIsHost && mDisconnected) {
        DrawRectangle(0, 0, 1280, 720, {0, 0, 0, 150});
        fm.DrawTextCentered(S::StatusDisconnected, {0, 280, 1280, 50}, 32, Theme::Warn, 2);
        fm.DrawTextCentered(S::Reconnecting, {0, 340, 1280, 30}, 20, Theme::Sub, 1);
    }
}

// ============================================================================
// 手枪绘制（纯图形，DrawRectanglePro + 正确 origin 让所有零件统一围绕 p 旋转）
//
// DrawRectanglePro(rect, origin, angle, color) 旋转中心 = {rect.x+origin.x, rect.y+origin.y}
// 规则：rect 一律使用"未旋转(angle=0)时的绝对坐标"，origin = {p.x - rect.x, p.y - rect.y}
// 这样所有零件都围绕 p 旋转，不会"散架"。
// ============================================================================
void GameScene::DrawRevolver(Vector2 p, float angleDeg) const {
    // 1) 枪管：未旋转左上角 (p.x, p.y-6)，130×12
    {
        Rectangle r{p.x, p.y - 6, 130, 12};
        Vector2 origin{p.x - r.x, p.y - r.y};  // {0, 6}
        DrawRectanglePro(r, origin, angleDeg, {80, 82, 90, 255});
    }
    // 2) 枪口加强套：未旋转左上角 (p.x+100, p.y-8)，40×16
    {
        Rectangle r{p.x + 100, p.y - 8, 40, 16};
        Vector2 origin{p.x - r.x, p.y - r.y};  // {-100, 8}
        DrawRectanglePro(r, origin, angleDeg, {65, 67, 74, 255});
    }
    // 3) 弹巢 + 6 个膛孔（用 rot 帮助定位圆的中心即可，DrawCircle 没有 origin 概念）
    float rad = angleDeg * DEG2RAD;
    auto rot = [&](float x, float y) -> Vector2 {
        return {p.x + x * cosf(rad) - y * sinf(rad), p.y + x * sinf(rad) + y * cosf(rad)};
    };
    {
        Vector2 cyl = rot(36, 0);
        DrawCircleV(cyl, 26, {55, 57, 64, 255});
        DrawCircleLines((int)cyl.x, (int)cyl.y, 26, {95, 98, 106, 255});
        for (int i = 0; i < 6; ++i) {
            float a = i * (PI / 3.0f);
            Vector2 hole = rot(36 + cosf(a) * 13, sinf(a) * 13);
            DrawCircleV(hole, 4, {35, 37, 42, 255});
        }
    }
    // 4) 握把：未旋转左上角 (p.x-16, p.y+2)，22×58，相对于枪体额外倾斜 55°
    //    旋转中心仍为 p（55° 围绕 p 而不是握把自身）
    {
        Rectangle r{p.x - 16, p.y + 2, 22, 58};
        Vector2 origin{p.x - r.x, p.y - r.y};  // {16, -2}  raylib 支持 origin 超出矩形
        DrawRectanglePro(r, origin, angleDeg + 55.0f, {105, 72, 50, 255});
    }
    // 5) 击锤：未旋转左上角 (p.x-22-7, p.y-10-6)，14×12
    //    （击锤中心相对 p 在 (-22, -10)，尺寸 14×12）
    {
        Rectangle r{p.x - 22 - 7, p.y - 10 - 6, 14, 12};
        Vector2 origin{p.x - r.x, p.y - r.y};  // {29, 16}  —— raylib 允许 origin 超出矩形
        DrawRectanglePro(r, origin, angleDeg, {60, 62, 70, 255});
    }
    // 6) 枪口闪光
    if (mFlashT > 0) {
        Vector2 mz = rot(155, 0);
        DrawCircleV(mz, 12, {255, 240, 180, 220});
        DrawPoly(mz, 3, 34, angleDeg, {255, 210, 110, 190});
        DrawPoly(mz, 3, 34, angleDeg + 180, {255, 210, 110, 140});
    }
}
