// ============================================================================
// GameScene.h - 游戏主界面（设计模式：观察者模式 + 状态模式）
//
// 三种角色复用同一渲染代码：
//   * Local/Host：持有权威 Game（HostSession），GameScene 作为观察者接收事件
//   * Client：持有 ClientSession，由服务器 state/event 消息驱动 GameView 镜像
//
// 渲染层只读取 GameView；输入根据角色分发（本地直调 Game / 网络发送 action）。
// ============================================================================
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../core/GameEvent.h"
#include "../core/GameView.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Widgets.h"

class HostSession;
class ClientSession;
class Game;  // 前向声明：mGame 仅为指针成员，完整定义见 GameScene.cpp 的 #include

class GameScene : public Scene, public IGameObserver {
public:
    // 主机 / 单机
    GameScene(SceneManager& mgr, std::shared_ptr<HostSession> host);
    // 客户端
    GameScene(SceneManager& mgr, std::shared_ptr<ClientSession> client);
    ~GameScene() override;

    void OnEnter() override;
    void Update() override;
    void Draw() override;
    void OnGameEvent(const GameEvent& event) override;  // IGameObserver

private:
    // ---- 视图构建 ----
    void BuildLocalView();          // Game -> GameView（主机/单机）
    void ApplyClientView();         // 服务器 state -> GameView（客户端）
    void CheckKnownEdge(bool newKnown, bool newLive);  // 放大镜窥探提示

    // ---- 事件与日志 ----
    void HandleEvent(const GameEvent& event);
    void AddLog(const std::string& text, Color color);
    void ShowBanner(const std::string& text, Color color, double seconds = 1.2);
    void ShowToast(const std::string& text);
    std::string LookupName(int id) const;
    std::string ItemName(const std::string& type) const;

    // ---- 输入与动作 ----
    bool CanAct() const;                        // 当前是否轮到本地玩家操作
    void PerformShoot(int targetId);            // targetId=-1 表示自己
    void PerformUseItem(const std::string& type, int targetId = -1);
    void CancelTargeting();

    // ---- 绘制 ----
    void DrawTopBar();
    void DrawPlayerPanel();
    void DrawStage();
    void DrawItems();
    void DrawLog();
    void DrawOverlays();
    void DrawRevolver(Vector2 pivot, float angleDeg) const;
    Vector2 RowCenter(int playerId) const;

    SceneManager& mMgr;
    bool mIsHost = true;
    std::shared_ptr<HostSession> mHost;    // Local/Host 角色
    std::shared_ptr<ClientSession> mClient;  // Client 角色
    Game* mGame = nullptr;                  // 权威逻辑（仅 Local/Host）

    GameView mView;            // 渲染视图（所有角色共用）
    GameView mPendingView;     // 客户端待应用的状态
    bool mHasPending = false;
    bool mPrevKnown = false;

    // ---- 动画与显示状态 ----
    struct ShotAnim {
        bool active = false;
        double t = 0;
        int shooter = -1, target = -1;
        bool self = false, live = false;
        int dmg = 0;
    } mShot;
    std::string mBanner;
    Color mBannerColor = WHITE;
    double mBannerT = 0;
    float mGunAngle = 0, mGunTarget = 0;
    double mFlashT = 0;
    float mShake = 0;
    double mInputLock = 0;
    double mToastT = 0;
    std::string mToast;

    struct LogLine {
        std::string text;
        Color color;
    };
    std::vector<LogLine> mLog;

    // ---- 目标选择 ----
    bool mTargeting = false;          // 正在选定目标
    bool mTargetShoot = true;         // true=射击目标 false=使用道具
    std::string mTargetItem;           // 待使用的道具

    // ---- 命中区域（Draw 时更新）----
    std::map<int, Rectangle> mRows;

    // ---- 客户端断线重连 ----
    bool mDisconnected = false;
    double mDisconnectAt = 0;
    double mRetryTimer = 0;
};
