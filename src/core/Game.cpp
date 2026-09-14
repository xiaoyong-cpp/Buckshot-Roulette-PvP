#include "Game.h"

#include <algorithm>

#include "../items/ItemFactory.h"
#include "../items/IItemStrategy.h"
#include "../utils/Logger.h"

Game::Game(IMode* mode, Random& rng, const GameConfig& cfg)
    : mMode(mode), mRng(rng), mConfig(cfg) {}

void Game::AddPlayer(int id, const std::string& name, bool isAI) {
    Player p;
    p.id = id;
    p.name = name;
    p.hp = mConfig.defaultHp;
    p.maxHp = mConfig.defaultHp;
    p.isAI = isAI;
    p.connected = !isAI;  // AI 视为常驻在线
    mPlayers.push_back(p);
}

void Game::Start() {
    if ((int)mPlayers.size() < 2) {
        Logger::Instance().Warn("Game::Start 调用时玩家不足 2 人");
        return;
    }
    mRound = 0;
    mTurnPlayerId = mPlayers.front().id;
    mLastActorId = -1;
    mWinnerId = -1;
    mContinuePending = false;
    mPendingItem.clear();
    mState = GameState::RoundEnd;
    mStateTimer = 0.5;  // 短暂展示"对局开始"

    GameEvent e;
    e.kind = EventKind::GameStart;
    e.value = (int)mPlayers.size();
    EmitEvent(e);
    Logger::Instance().Info("对局开始，玩家数: " + std::to_string(mPlayers.size()));
}

void Game::Update(double dt) {
    if (mState == GameState::Waiting || mState == GameState::GameOver) return;

    if (mState == GameState::RoundEnd) {
        mStateTimer -= dt;
        if (mStateTimer <= 0.0) DoReload();
        return;
    }
    if (mState == GameState::Reloading) {
        mStateTimer -= dt;
        if (mStateTimer <= 0.0) {
            mState = GameState::Aiming;
            // 决定装弹后由谁行动
            if (mContinuePending) {
                mContinuePending = false;  // 空弹自射/啤酒：保留当前玩家
            } else if (mLastActorId != -1) {
                EndTurn(mLastActorId);  // 非首轮：轮转到下一位存活玩家
            }
            // else: 首轮保留 Start() 设定的首位玩家（按加入顺序先手）
            GameEvent e;
            e.kind = EventKind::TurnStart;
            e.playerId = mTurnPlayerId;
            EmitEvent(e);
        }
        return;
    }
}

bool Game::Shoot(int shooterId, int targetId, std::string* err) {
    if (mState != GameState::Aiming && mState != GameState::ItemSelect) {
        if (err) *err = "现在不能射击";
        return false;
    }
    Player* shooter = FindPlayer(shooterId);
    if (!shooter || shooter->id != mTurnPlayerId) {
        if (err) *err = "不是你的回合";
        return false;
    }
    if (mChamber.IsEmpty()) {
        if (err) *err = "弹仓已空";
        return false;
    }
    // 校验目标：-1 = 自己；否则必须是其他存活玩家
    Player* target = nullptr;
    if (targetId != -1) {
        target = FindPlayer(targetId);
        if (!target || !target->alive || target->id == shooter->id) {
            if (err) *err = "无效目标";
            return false;
        }
    }

    mState = GameState::Shooting;
    mPendingItem.clear();

    bool sawedBeforeFire = mChamber.sawed;
    bool isLive = mChamber.Fire();
    ResetKnowledge();

    bool self = (target == nullptr);  // targetId == -1
    GameEvent shot;
    shot.kind = EventKind::Shot;
    shot.playerId = shooterId;
    shot.targetId = self ? -1 : target->id;
    shot.flag = isLive;
    shot.value = isLive ? (sawedBeforeFire ? 2 : 1) : 0;
    EmitEvent(shot);

    if (isLive) {
        Player& victim = self ? *shooter : *target;
        int dmg = sawedBeforeFire ? 2 : 1;
        victim.hp -= dmg;
        if (victim.hp < 0) victim.hp = 0;
        GameEvent dmg2;
        dmg2.kind = EventKind::PlayerDamaged;
        dmg2.targetId = victim.id;
        dmg2.value = dmg;
        EmitEvent(dmg2);
        if (victim.hp <= 0) {
            victim.alive = false;
            GameEvent elim;
            elim.kind = EventKind::PlayerEliminated;
            elim.targetId = victim.id;
            EmitEvent(elim);
            if (CheckGameOver()) return true;
        }
    } else if (self) {
        // 空弹自射 -> 获得额外回合
        GameEvent extra;
        extra.kind = EventKind::ExtraTurn;
        extra.playerId = shooterId;
        EmitEvent(extra);
    }

    mLastActorId = shooterId;
    if (mChamber.IsEmpty()) {
        // 弹仓耗尽：空弹自射/啤酒保留"继续回合"，其它情况交给装弹后轮转
        mContinuePending = (!isLive && self);
        EnterRoundEnd();
    } else {
        // 回合是否结束？实弹射击结束回合；空弹自射继续；空弹射他人结束
        bool turnEnds = isLive ? true : !self;
        mState = GameState::Aiming;
        if (turnEnds) EndTurn(shooterId);
    }
    return true;
}

bool Game::BeginItemTarget(int playerId, const std::string& itemType, std::string* err) {
    if (mState != GameState::Aiming) {
        if (err) *err = "现在不能使用道具";
        return false;
    }
    if (playerId != mTurnPlayerId) {
        if (err) *err = "不是你的回合";
        return false;
    }
    Player* p = FindPlayer(playerId);
    if (!p || !p->HasItem(itemType)) {
        if (err) *err = "没有该道具";
        return false;
    }
    mPendingItem = itemType;
    mState = GameState::ItemSelect;
    return true;
}

void Game::CancelItemTarget() {
    if (mState == GameState::ItemSelect) {
        mPendingItem.clear();
        mState = GameState::Aiming;
    }
}

bool Game::UseItem(int playerId, const std::string& itemType, int targetId, std::string* err) {
    if (mState != GameState::Aiming && mState != GameState::ItemSelect) {
        if (err) *err = "现在不能使用道具";
        return false;
    }
    if (playerId != mTurnPlayerId) {
        if (err) *err = "不是你的回合";
        return false;
    }
    Player* p = FindPlayer(playerId);
    if (!p || !p->HasItem(itemType)) {
        if (err) *err = "没有该道具";
        return false;
    }
    if (mChamber.IsEmpty() && (itemType == "magnifier" || itemType == "beer" ||
                              itemType == "saw" || itemType == "reverser")) {
        if (err) *err = "弹仓已空";
        return false;
    }
    if (mChamber.sawed && itemType == "saw") {
        if (err) *err = "已经锯断";
        return false;
    }
    if (itemType == "handcuff" || itemType == "adrenaline") {
        if (!IsPvp()) {
            if (err) *err = "该模式无法使用";
            return false;
        }
        Player* target = FindPlayer(targetId);
        if (!target || !target->alive || target->id == playerId) {
            if (err) *err = "无效目标";
            return false;
        }
        if (itemType == "adrenaline" && target->items.empty()) {
            if (err) *err = "对方没有道具";
            return false;
        }
    }

    // 消耗道具
    p->RemoveItem(itemType);
    mState = GameState::Aiming;

    // 公开"使用道具"事件
    GameEvent used;
    used.kind = EventKind::ItemUsed;
    used.playerId = playerId;
    used.text = itemType;
    used.targetId = (itemType == "handcuff" || itemType == "adrenaline") ? targetId : -1;
    EmitEvent(used);

    // 通过工厂取得策略实例并执行（策略模式 + 工厂模式）
    auto strategy = ItemFactory::Instance().Create(itemType);
    GameContext ctx{mChamber, mPlayers, mRng, *this, IsPvp(), mTurnPlayerId, MAX_BACKPACK};
    Player* target = (itemType == "handcuff" || itemType == "adrenaline") ? FindPlayer(targetId) : nullptr;
    strategy->Apply(ctx, p, target);

    // 啤酒可能耗尽弹仓
    if (mChamber.IsEmpty()) {
        mLastActorId = playerId;
        mContinuePending = true;  // 啤酒不结束回合，装弹后该玩家继续
        EnterRoundEnd();
    }
    // 道具可能导致玩家死亡（过期药品扣血致死）：检查是否触发终局
    if (CheckGameOver()) return true;
    // 未终局但当前回合玩家已死亡（服毒自杀）：
    // 立即把回合转移给下一位存活玩家，否则回合指向死者将死锁对局
    Player* cur = FindPlayer(mTurnPlayerId);
    if (cur && !cur->alive) EndTurn(cur->id);
    return true;
}

void Game::AddObserver(IGameObserver* observer) {
    if (observer) mObservers.push_back(observer);
}

void Game::RemoveObserver(IGameObserver* observer) {
    mObservers.erase(std::remove(mObservers.begin(), mObservers.end(), observer), mObservers.end());
}

void Game::EmitEvent(const GameEvent& event) {
    // 立即分发给所有观察者（UI 与网络中继）
    for (auto* o : mObservers) o->OnGameEvent(event);
}

Player* Game::FindPlayer(int id) {
    for (auto& p : mPlayers)
        if (p.id == id) return &p;
    return nullptr;
}

const Player* Game::FindPlayer(int id) const {
    for (const auto& p : mPlayers)
        if (p.id == id) return &p;
    return nullptr;
}

int Game::AliveCount() const {
    int n = 0;
    for (const auto& p : mPlayers)
        if (p.alive) ++n;
    return n;
}

bool Game::IsPvp() const { return mMode ? mMode->IsPvp() : false; }

void Game::EnterRoundEnd() {
    mState = GameState::RoundEnd;
    mStateTimer = 1.0;
}

void Game::DoReload() {
    ++mRound;
    auto [live, blank] = mMode->GenerateReload(mRound, mRng);
    mChamber.Load(live, blank, mRng);
    ResetKnowledge();
    // 本轮所有存活玩家获得"相同数量"的道具（均衡分配）
    int count = RollItemCount();
    for (auto& p : mPlayers)
        if (p.alive) GrantItems(p, count);

    GameEvent e;
    e.kind = EventKind::Reload;
    e.value = live;
    e.value2 = blank;
    EmitEvent(e);
    Logger::Instance().Info("第 " + std::to_string(mRound) + " 轮装弹: " +
                            std::to_string(live) + " 实弹, " + std::to_string(blank) + " 空弹, " +
                            "每人 " + std::to_string(count) + " 个道具");

    mState = GameState::Reloading;
    mStateTimer = 1.2;
}

void Game::EndTurn(int afterPlayerId) {
    int n = (int)mPlayers.size();
    if (n == 0) return;
    int startIdx = 0;
    for (int i = 0; i < n; ++i)
        if (mPlayers[(size_t)i].id == afterPlayerId) { startIdx = i; break; }

    int chosen = -1;
    for (int step = 1; step <= n; ++step) {
        int idx = (startIdx + step) % n;
        if (mPlayers[(size_t)idx].alive) {
            chosen = idx;
            break;
        }
    }
    if (chosen < 0) return;  // 仅自己存活（理论上 GameOver 已处理）
    mTurnPlayerId = mPlayers[(size_t)chosen].id;

    // 手铐：被铐者跳过其回合并清除标记
    Player* cur = FindPlayer(mTurnPlayerId);
    while (cur && cur->skipNextTurn) {
        cur->skipNextTurn = false;
        GameEvent skip;
        skip.kind = EventKind::TurnSkip;
        skip.playerId = cur->id;
        EmitEvent(skip);
        // 继续找下一位
        int nextIdx = (chosen + 1) % n;
        while (!mPlayers[(size_t)nextIdx].alive) nextIdx = (nextIdx + 1) % n;
        if (nextIdx == chosen) break;  // 只剩自己存活
        chosen = nextIdx;
        mTurnPlayerId = mPlayers[(size_t)chosen].id;
        cur = FindPlayer(mTurnPlayerId);
    }
}

void Game::GrantItems(Player& p, int count) {
    for (int i = 0; i < count; ++i) {
        std::string type = ItemFactory::Instance().RandomType(mRng, IsPvp());
        if (type.empty()) break;
        if ((int)p.items.size() >= MAX_BACKPACK) {
            // 背包已满：自动丢弃超出（轻量实现，详见 README）
            GameEvent info;
            info.kind = EventKind::Info;
            info.playerId = p.id;
            info.text = type;
            EmitEvent(info);
        } else {
            p.items.push_back(type);
        }
    }
}

// 加权随机表：偏向 3/4/5 个道具（用户要求增加其爆率）
//   count:  0  1  2  3  4  5
//   weight: 1  2  3  5  4  3   (总权重 18)
int Game::RollItemCount() {
    static const int weights[] = {1, 2, 3, 5, 4, 3};
    static const int n = (int)(sizeof(weights) / sizeof(weights[0]));
    int total = 0;
    for (int i = 0; i < n; ++i) total += weights[i];
    int r = mRng.GetInt(0, total - 1);
    int acc = 0;
    int count = 2;
    for (int i = 0; i < n; ++i) {
        acc += weights[i];
        if (r < acc) { count = i; break; }
    }
    // 尊重配置范围：min==max 时固定数量（min==max==0 表示本轮不发道具）
    int lo = mConfig.itemsPerRoundMin < 0 ? 0 : mConfig.itemsPerRoundMin;
    int hi = mConfig.itemsPerRoundMax < lo ? lo : mConfig.itemsPerRoundMax;
    if (count < lo) count = lo;
    if (count > hi) count = hi;
    return count;
}

void Game::ResetKnowledge() {
    for (auto& p : mPlayers) {
        p.knownShellValid = false;
        p.knownShellLive = false;
    }
}

bool Game::CheckGameOver() {
    int alive = AliveCount();
    if (alive <= 1) {
        mState = GameState::GameOver;
        for (const auto& p : mPlayers) {
            if (p.alive) mWinnerId = p.id;
        }
        GameEvent e;
        e.kind = EventKind::GameOver;
        e.targetId = mWinnerId;
        EmitEvent(e);
        Logger::Instance().Info(std::string("对局结束，获胜者 id=") + std::to_string(mWinnerId));
        return true;
    }
    return false;
}
