// ============================================================================
// Game.h - 游戏核心（设计模式：状态模式 + 观察者模式）
//  * 状态模式：GameState 描述对局流程（等待/装弹/瞄准/选目标/射击/回合结束/终局）
//  * 观察者模式：所有状态变化以 GameEvent 形式通知 IGameObserver（UI / 网络中继）
//  * Game 是主机端唯一的权威逻辑（客户端只做镜像渲染，不运行 Game）
// ============================================================================
#pragma once

#include <string>
#include <vector>

#include "Chamber.h"
#include "GameContext.h"  // IEventSink 定义于此：Game 继承它以接收道具产生的事件
#include "GameEvent.h"
#include "GameState.h"
#include "Player.h"
#include "../modes/IMode.h"
#include "../utils/Random.h"

// 对局参数（来自 config.json）
struct GameConfig {
    int defaultHp = 4;
    int maxItems = MAX_BACKPACK;  // 仅保留字段兼容
    int itemsPerRoundMin = 0;
    int itemsPerRoundMax = 5;  // 提高上限以支持 3/4/5 个道具的爆率
};

class Game : public IEventSink {
public:
    Game(IMode* mode, Random& rng, const GameConfig& cfg);

    void AddPlayer(int id, const std::string& name, bool isAI);
    void Start();            // 开始对局（需 >= 2 名玩家）
    void Update(double dt);  // 推进定时状态迁移（RoundEnd -> Reloading -> Aiming）

    // ---- 动作接口（全部带校验；失败返回 false 并写入 err）----
    bool Shoot(int shooterId, int targetId, std::string* err);           // targetId=-1 表示射击自己
    bool BeginItemTarget(int playerId, const std::string& itemType, std::string* err);  // 进入 ItemSelect
    void CancelItemTarget();
    bool UseItem(int playerId, const std::string& itemType, int targetId, std::string* err);

    // ---- 查询接口 ----
    Player* FindPlayer(int id);
    const Player* FindPlayer(int id) const;
    const std::vector<Player>& Players() const { return mPlayers; }
    const Chamber& GetChamber() const { return mChamber; }
    GameState GetState() const { return mState; }
    int CurrentTurnPlayerId() const { return mTurnPlayerId; }
    int Round() const { return mRound; }
    int WinnerId() const { return mWinnerId; }
    int AliveCount() const;
    bool IsPvp() const;
    bool IsStarted() const { return mState != GameState::Waiting; }
    IMode* Mode() const { return mMode; }
    const std::string& PendingItem() const { return mPendingItem; }
    const GameConfig& Config() const { return mConfig; }

    // ---- 观察者管理 ----
    void AddObserver(IGameObserver* observer);
    void RemoveObserver(IGameObserver* observer);
    void EmitEvent(const GameEvent& event) override;  // IEventSink：道具效果产生的事件

private:
    void DoReload();         // 装弹 + 发放道具
    void EndTurn(int afterPlayerId);  // 轮转到 afterPlayerId 之后的下一位存活玩家
    void GrantItems(Player& p, int count);  // 给玩家发放 count 个道具（每轮所有玩家同数）
    int RollItemCount();     // 加权随机决定本轮道具数（偏向 3/4/5）
    void ResetKnowledge();   // 弹槽推进后，清除所有玩家的窥探情报
    bool CheckGameOver();    // 存活人数 <= 1 时进入终局
    void EnterRoundEnd();    // 弹仓耗尽 -> 回合结束阶段

    std::vector<Player> mPlayers;
    std::vector<IGameObserver*> mObservers;
    Chamber mChamber;
    IMode* mMode;
    Random& mRng;
    GameConfig mConfig;

    GameState mState = GameState::Waiting;
    int mTurnPlayerId = -1;
    int mLastActorId = -1;       // 最后行动者（决定装弹后轮到谁）
    int mRound = 0;
    int mWinnerId = -1;
    bool mContinuePending = false;  // 弹仓打空时当前玩家的回合尚未结束（空弹自射/啤酒退弹后）
    double mStateTimer = 0.0;       // RoundEnd / Reloading 阶段计时
    std::string mPendingItem;       // ItemSelect 状态下等待指定目标的道具
};
