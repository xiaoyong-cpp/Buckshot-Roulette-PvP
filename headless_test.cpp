// ============================================================================
// headless_test.cpp - 无头逻辑测试（不依赖 raylib/Winsock）
// 自动模拟单机对局与 PVP 对局，验证核心规则与不变量：
//   1. 生命值非负、不超上限
//   2. 对局必然到达 GameOver
//   3. 获胜者存活
//   4. 单机模式禁用手铐/肾上腺素；PVP 模式可用
// ============================================================================
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "src/core/AIController.h"
#include "src/core/Game.h"
#include "src/core/GameEvent.h"
#include "src/core/Player.h"
#include "src/items/ItemFactory.h"
#include "src/modes/ModeFactory.h"
#include "src/modes/SingleMode.h"
#include "src/modes/MultiplayerMode.h"
#include "src/utils/ConfigLoader.h"
#include "src/utils/Logger.h"
#include "src/utils/Random.h"

// 观察者：统计事件用于断言
class StatsObserver : public IGameObserver {
public:
    int reloads = 0;
    int shots = 0;
    int liveShots = 0;
    int blankShots = 0;
    int extraTurns = 0;
    int eliminations = 0;
    int itemUsed = 0;
    int itemFail = 0;

    void OnGameEvent(const GameEvent& e) override {
        switch (e.kind) {
            case EventKind::Reload: ++reloads; break;
            case EventKind::Shot:
                ++shots;
                if (e.flag) ++liveShots; else ++blankShots;
                break;
            case EventKind::ExtraTurn: ++extraTurns; break;
            case EventKind::PlayerEliminated: ++eliminations; break;
            case EventKind::ItemUsed: ++itemUsed; break;
            case EventKind::ItemFail: ++itemFail; break;
            default: break;
        }
    }
};

// 推进 Game 直到进入可操作状态（Aiming/ItemSelect/GameOver）
static void PumpToStable(Game& g, int maxFrames = 200) {
    for (int i = 0; i < maxFrames; ++i) {
        g.Update(0.05);
        if (g.GetState() == GameState::Aiming || g.GetState() == GameState::ItemSelect ||
            g.GetState() == GameState::GameOver)
            return;
    }
}

// 用 AIController 自动执行一局，返回总步数
static int PlayOneGame(Game& game, Random& rng, int maxSteps = 20000) {
    StatsObserver stats;
    game.AddObserver(&stats);
    game.Start();
    PumpToStable(game);

    int steps = 0;
    int lastTurn = -1;
    int sameTurnCount = 0;
    while (game.GetState() != GameState::GameOver && steps < maxSteps) {
        int turn = game.CurrentTurnPlayerId();
        const Player* p = game.FindPlayer(turn);
        if (!p || !p->alive) {
            game.Update(0.05);
            PumpToStable(game);
            ++steps;
            continue;
        }
        // AI 决策
        AIAction act = AIController::Decide(game, turn, rng);
        std::string err;
        if (act.type == AIAction::Type::UseItem) {
            // 单机模式手铐/肾上腺素应被拒绝
            game.UseItem(turn, act.item, -1, &err);
        } else {
            game.Shoot(turn, act.targetId, &err);
        }
        game.Update(0.05);
        PumpToStable(game);
        ++steps;

        // 诊断：每 1000 步打印对局快照（stderr + 立即刷新，防 abort 丢缓冲）
        if (steps % 1000 == 0) {
            fprintf(stderr, "[快照 step=%d] 状态=%d 轮=%d 回合=P%d 弹仓余=%d(实%d)\n",
                    steps, (int)game.GetState(), game.Round(), game.CurrentTurnPlayerId(),
                    game.GetChamber().Remaining(), game.GetChamber().LiveRemaining());
            for (const auto& pl : game.Players()) {
                fprintf(stderr, "  P%d alive=%d hp=%d 道具n=%d known=%d live=%d\n",
                        pl.id, pl.alive ? 1 : 0, pl.hp, pl.ItemCount(),
                        pl.knownShellValid ? 1 : 0, pl.knownShellLive ? 1 : 0);
                fprintf(stderr, "    道具表:");
                for (const auto& it : pl.items) fprintf(stderr, " %s", it.c_str());
                fprintf(stderr, "\n");
            }
            fflush(stderr);
        }

        // 诊断：同一玩家连续行动过多（可能卡循环）
        int curTurn = game.CurrentTurnPlayerId();
        if (curTurn == turn) {
            sameTurnCount++;
            if (sameTurnCount > 300) {
                fprintf(stderr, "[诊断] 玩家 %d 连续行动 300+ 次\n", turn);
                fflush(stderr);
                break;
            }
        } else {
            sameTurnCount = 0;
        }
    }
    game.RemoveObserver(&stats);

    // 断言：不变量
    assert(steps < maxSteps && "对局未在合理步数内结束");
    assert(game.GetState() == GameState::GameOver);
    assert(stats.eliminations >= 1);
    int alive = 0;
    for (const auto& pl : game.Players()) if (pl.alive) ++alive;
    assert(alive == 1);
    for (const auto& pl : game.Players()) {
        assert(pl.hp >= 0 && pl.hp <= pl.maxHp);
    }
    return steps;
}

int main() {
    // 初始化各单例（与 main.cpp 一致，但不创建窗口）
    ConfigLoader::Instance().Load("config.json");
    Logger::Instance().Init("logs");
    ItemFactory::Instance().Init();
    ModeFactory::Instance().Init();

    int totalTests = 0, passed = 0;

    auto run = [&](const char* name, auto fn) {
        ++totalTests;
        try {
            fn();
            ++passed;
            std::printf("[PASS] %s\n", name);
        } catch (const std::exception& e) {
            std::printf("[FAIL] %s: %s\n", name, e.what());
        } catch (...) {
            std::printf("[FAIL] %s: 未知异常\n", name);
        }
    };

    // ---- 测试 1：单机模式自动对局（多次随机种子）----
    run("单机模式自动对局 x10", [&]() {
        for (int seed = 1; seed <= 10; ++seed) {
            Random rng(seed * 7 + 1);
            auto mode = std::make_shared<SingleMode>();
            GameConfig cfg;
            cfg.defaultHp = 4;
            cfg.maxItems = MAX_BACKPACK;
            cfg.itemsPerRoundMin = 0;
            cfg.itemsPerRoundMax = 2;
            Game game(mode.get(), rng, cfg);
            game.AddPlayer(0, "玩家", false);
            game.AddPlayer(1, "恶魔", true);
            int steps = PlayOneGame(game, rng);
            assert(game.WinnerId() == 0 || game.WinnerId() == 1);
            (void)steps;
        }
    });

    // ---- 测试 2：PVP 模式 2 人自动对局 ----
    run("PVP 2人自动对局 x10", [&]() {
        for (int seed = 1; seed <= 10; ++seed) {
            Random rng(seed * 13 + 5);
            auto mode = std::make_shared<MultiplayerMode>();
            GameConfig cfg;
            cfg.defaultHp = 4;
            cfg.maxItems = MAX_BACKPACK;
            cfg.itemsPerRoundMin = 1;
            cfg.itemsPerRoundMax = 5;
            Game game(mode.get(), rng, cfg);
            game.AddPlayer(0, "玩家1", false);
            game.AddPlayer(1, "玩家2", false);
            PlayOneGame(game, rng);
        }
    });

    // ---- 测试 3：PVP 模式 4 人自动对局（N 人支持）----
    run("PVP 4人自动对局 x5", [&]() {
        for (int seed = 1; seed <= 5; ++seed) {
            Random rng(seed * 31 + 9);
            auto mode = std::make_shared<MultiplayerMode>();
            GameConfig cfg;
            cfg.defaultHp = 4;
            cfg.maxItems = MAX_BACKPACK;
            cfg.itemsPerRoundMin = 1;
            cfg.itemsPerRoundMax = 5;
            Game game(mode.get(), rng, cfg);
            for (int i = 0; i < 4; ++i) game.AddPlayer(i, "P" + std::to_string(i), false);
            PlayOneGame(game, rng);
        }
    });

    // ---- 测试 4：弹仓机制单元测试 ----
    run("弹仓装填与击发", [&]() {
        Random rng(42);
        Chamber ch;
        // 2 实 3 空，共 5 发
        ch.Load(2, 3, rng);
        assert(ch.Count() == 5);
        assert(ch.Remaining() == 5);
        assert(ch.LiveRemaining() == 2);
        assert(ch.BlankRemaining() == 3);
        // 击发 5 发，统计实弹数
        int live = 0, blank = 0;
        for (int i = 0; i < 5; ++i) {
            if (ch.Fire()) ++live; else ++blank;
            assert(ch.Remaining() == 4 - i);
        }
        assert(live == 2 && blank == 3);
        assert(ch.IsEmpty());
    });

    // ---- 测试 5：手锯伤害翻倍 ----
    run("手锯使下一次实弹伤害翻倍", [&]() {
        Random rng(100);
        auto mode = std::make_shared<SingleMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家", false);
        game.AddPlayer(1, "恶魔", true);
        game.Start();
        PumpToStable(game);
        Player* p0 = game.FindPlayer(0);

        // 策略：窥探当前弹。空弹 -> 自射（空弹自射得额外回合，保留先手）；
        // 实弹 -> 锯断后射对手，验证伤害为 2。单机模式至少 1 发实弹，必能命中。
        int observedDmg = 0;
        int attempts = 0;
        while (attempts < 30 && observedDmg == 0 &&
               game.GetState() != GameState::GameOver) {
            ++attempts;
            if (!p0->HasItem("magnifier")) p0->items.push_back("magnifier");
            if (!p0->HasItem("saw")) p0->items.push_back("saw");
            std::string err;
            bool ok = game.UseItem(0, "magnifier", -1, &err);
            assert(ok && "放大镜应可用");
            if (!p0->knownShellLive) {
                // 空弹：自射以保留回合（空弹自射不结束回合）
                game.Shoot(0, -1, &err);
                game.Update(0.05);
                PumpToStable(game);
                continue;
            }
            // 已知实弹：锯断 + 射对手
            game.UseItem(0, "saw", -1, &err);
            assert(game.GetChamber().sawed && "锯断标志应已置位");
            int hp1 = game.FindPlayer(1)->hp;
            game.Shoot(0, 1, &err);
            int hp2 = game.FindPlayer(1)->hp;
            if (hp1 > hp2) observedDmg = hp1 - hp2;
            game.Update(0.05);
            PumpToStable(game);
        }
        assert(observedDmg == 2 && "手锯后实弹应造成 2 点伤害");
    });

    // ---- 测试 6：单机模式禁用手铐 ----
    run("单机模式拒绝手铐", [&]() {
        Random rng(7);
        auto mode = std::make_shared<SingleMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家", false);
        game.AddPlayer(1, "恶魔", true);
        game.Start();
        PumpToStable(game);
        Player* p0 = game.FindPlayer(0);
        p0->items.push_back("handcuff");
        std::string err;
        bool ok = game.UseItem(0, "handcuff", 1, &err);
        assert(!ok && "单机模式应拒绝手铐");
    });

    // ---- 测试 7：放大镜窥探结果正确 ----
    run("放大镜揭示当前弹", [&]() {
        Random rng(999);
        auto mode = std::make_shared<SingleMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家", false);
        game.AddPlayer(1, "恶魔", true);
        game.Start();
        PumpToStable(game);
        Player* p0 = game.FindPlayer(0);
        p0->items.push_back("magnifier");
        // 使用前不知道
        assert(!p0->knownShellValid);
        std::string err;
        bool ok = game.UseItem(0, "magnifier", -1, &err);
        assert(ok);
        // 使用后应知道当前弹
        assert(p0->knownShellValid);
        bool chLive = game.GetChamber().CurrentIsLive();
        assert(p0->knownShellLive == chLive);
    });

    // ---- 测试 8：PVP 手铐使目标跳过下回合 ----
    run("PVP 手铐使目标跳过下回合", [&]() {
        Random rng(55);
        auto mode = std::make_shared<MultiplayerMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家1", false);
        game.AddPlayer(1, "玩家2", false);
        game.Start();
        PumpToStable(game);
        assert(game.CurrentTurnPlayerId() == 0);
        Player* p0 = game.FindPlayer(0);
        p0->items.push_back("handcuff");
        // 玩家0 对玩家1 使用手铐
        std::string err;
        bool ok = game.UseItem(0, "handcuff", 1, &err);
        assert(ok && "PVP 模式手铐应可用");
        assert(game.FindPlayer(1)->skipNextTurn && "目标应被标记跳过");
        // 射对手一次以结束玩家0的回合（无论实空，射他人均结束回合）
        game.Shoot(0, 1, &err);
        game.Update(0.05);
        PumpToStable(game);
        // 手铐应使玩家1被跳过，回合直接回到玩家0
        assert(game.CurrentTurnPlayerId() == 0 && "手铐应使玩家1被跳过");
        // 跳过标记应已消费清除
        assert(!game.FindPlayer(1)->skipNextTurn && "跳过标记应已清除");
    });

    // ---- 测试 9：PVP 肾上腺素偷取道具 ----
    run("PVP 肾上腺素偷取目标道具", [&]() {
        Random rng(77);
        auto mode = std::make_shared<MultiplayerMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家1", false);
        game.AddPlayer(1, "玩家2", false);
        game.Start();
        PumpToStable(game);
        Player* p0 = game.FindPlayer(0);
        Player* p1 = game.FindPlayer(1);
        p0->items.push_back("adrenaline");
        p1->items.push_back("saw");
        p1->items.push_back("beer");
        int before0 = (int)p0->items.size();
        int before1 = (int)p1->items.size();
        std::string err;
        bool ok = game.UseItem(0, "adrenaline", 1, &err);
        assert(ok && "PVP 模式肾上腺素应可用");
        assert((int)p0->items.size() == before0 && "玩家0 道具数应不变（消耗肾上腺素+偷1）");
        assert((int)p1->items.size() == before1 - 1 && "玩家1 应被偷走1个道具");
    });

    // ---- 测试 10：肾上腺素目标无道具时失败 ----
    run("肾上腺素目标无道具时失败并提示", [&]() {
        Random rng(88);
        auto mode = std::make_shared<MultiplayerMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家1", false);
        game.AddPlayer(1, "玩家2", false);
        game.Start();
        PumpToStable(game);
        Player* p0 = game.FindPlayer(0);
        p0->items.push_back("adrenaline");
        // 玩家2 无道具
        std::string err;
        bool ok = game.UseItem(0, "adrenaline", 1, &err);
        assert(!ok && "目标无道具时肾上腺素应失败");
        assert(!err.empty() && "应返回错误提示");
        // 道具不应被消耗
        assert(p0->HasItem("adrenaline") && "失败时不应消耗道具");
    });

    // ---- 测试 11：香烟恢复血量 ----
    run("香烟恢复1点血量", [&]() {
        Random rng(111);
        auto mode = std::make_shared<SingleMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家", false);
        game.AddPlayer(1, "恶魔", true);
        game.Start();
        PumpToStable(game);
        Player* p0 = game.FindPlayer(0);
        p0->hp = 2;  // 手动扣血
        p0->items.push_back("cigarette");
        std::string err;
        bool ok = game.UseItem(0, "cigarette", -1, &err);
        assert(ok);
        assert(p0->hp == 3 && "香烟应恢复1点血量");
    });

    // ---- 测试 12：过期药品概率效果 ----
    run("过期药品回复或扣血", [&]() {
        Random rng(222);
        auto mode = std::make_shared<SingleMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家", false);
        game.AddPlayer(1, "恶魔", true);
        game.Start();
        PumpToStable(game);
        Player* p0 = game.FindPlayer(0);
        p0->hp = 2;
        int hpBefore = p0->hp;
        p0->items.push_back("expiredmed");
        std::string err;
        bool ok = game.UseItem(0, "expiredmed", -1, &err);
        assert(ok);
        // 结果：回2血(hp=4) 或 扣1血(hp=1)
        assert((p0->hp == 4 || p0->hp == 1) && "过期药品应回2血或扣1血");
        assert(p0->hp != hpBefore && "血量应有变化");
    });

    // ---- 测试 13：反转器翻转弹槽 ----
    run("反转器翻转当前弹槽", [&]() {
        Random rng(333);
        auto mode = std::make_shared<SingleMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 0;
        Game game(mode.get(), rng, cfg);
        game.AddPlayer(0, "玩家", false);
        game.AddPlayer(1, "恶魔", true);
        game.Start();
        PumpToStable(game);
        Player* p0 = game.FindPlayer(0);
        // 先用放大镜查看当前弹
        p0->items.push_back("magnifier");
        std::string err;
        game.UseItem(0, "magnifier", -1, &err);
        bool beforeLive = p0->knownShellLive;
        int liveBefore = game.GetChamber().LiveRemaining();
        // 使用反转器
        p0->items.push_back("reverser");
        bool ok = game.UseItem(0, "reverser", -1, &err);
        assert(ok);
        // 反转后：当前弹应是翻转的值
        bool afterLive = game.GetChamber().CurrentIsLive();
        assert(afterLive == !beforeLive && "反转后当前弹槽应翻转");
        // 剩余实弹数应相应变化（+1 或 -1）
        int liveAfter = game.GetChamber().LiveRemaining();
        assert(std::abs(liveAfter - liveBefore) == 1 && "剩余实弹数应变化1");
    });

    // ---- 测试 14：道具分配均衡性（每轮所有玩家相同数量）----
    run("每轮所有玩家获得相同数量道具", [&]() {
        Random rng(444);
        auto mode = std::make_shared<MultiplayerMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 5;
        Game game(mode.get(), rng, cfg);
        for (int i = 0; i < 3; ++i) game.AddPlayer(i, "P" + std::to_string(i), false);
        game.Start();
        // 进入装弹阶段
        game.Update(0.6);
        PumpToStable(game);
        // 装弹后所有存活玩家道具数应相同（考虑背包上限，但初始为空所以相同）
        int count0 = game.FindPlayer(0)->ItemCount();
        int count1 = game.FindPlayer(1)->ItemCount();
        int count2 = game.FindPlayer(2)->ItemCount();
        assert(count0 == count1 && count1 == count2 && "首轮所有玩家道具数应相同");
        assert(count0 >= 0 && count0 <= 5);
    });

    // ---- 测试 15：道具数加权分布偏向 3/4/5 ----
    run("道具数加权分布偏向3/4/5", [&]() {
        Random rng(555);
        auto mode = std::make_shared<SingleMode>();
        GameConfig cfg;
        cfg.defaultHp = 4;
        cfg.maxItems = MAX_BACKPACK;
        cfg.itemsPerRoundMin = 0;
        cfg.itemsPerRoundMax = 5;
        // 统计多轮道具数分布
        int dist[6] = {0};
        for (int trial = 0; trial < 1000; ++trial) {
            Game game(mode.get(), rng, cfg);
            game.AddPlayer(0, "玩家", false);
            game.AddPlayer(1, "恶魔", true);
            game.Start();
            game.Update(0.6);  // 触发首轮装弹
            PumpToStable(game);
            int cnt = game.FindPlayer(0)->ItemCount();
            if (cnt >= 0 && cnt <= 5) dist[cnt]++;
        }
        // 3+4+5 的总占比应明显超过 0+1+2
        int lowSum = dist[0] + dist[1] + dist[2];
        int highSum = dist[3] + dist[4] + dist[5];
        assert(highSum > lowSum && "3/4/5 个道具的爆率应高于 0/1/2");
        // 3 应是最多的（权重 5 最高）
        bool threeIsMax = true;
        for (int i = 0; i <= 5; ++i) {
            if (i != 3 && dist[i] > dist[3]) threeIsMax = false;
        }
        assert(threeIsMax && "3个道具应出现频率最高");
    });

    std::printf("\n==== 测试结果: %d/%d 通过 ====\n", passed, totalTests);
    return passed == totalTests ? 0 : 1;
}
