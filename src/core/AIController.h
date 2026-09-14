// ============================================================================
// AIController.h - AI 决策器
// 单机模式驱动"恶魔"，PVP 模式托管掉线玩家。
// 每次调用只返回"一个动作"，由上层以固定节奏逐步执行（便于表现 AI 思考）。
// ============================================================================
#pragma once

#include <string>  // AIAction::item 为 std::string

class Game;
class Random;

struct AIAction {
    enum class Type { UseItem, Shoot } type = Type::Shoot;
    std::string item;   // UseItem: 道具类型
    int targetId = -1;  // Shoot: 目标（-1 = 自己）
};

class AIController {
public:
    // 决策规则（见设计文档 2.1 节）：
    //  1) 有放大镜且当前弹未知 -> 先窥探
    //  2) 已知实弹且有手锯 -> 先锯断（手锯一定搭配实弹使用）
    //  3) 弹未知且血量低（<= 一半）且有啤酒 -> 退掉一发未知弹
    //  4) 已知实弹 -> 射击对手；已知空弹 -> 射击自己（赚额外回合）
    //  5) 无信息 -> 60% 射自己 / 40% 射对手
    static AIAction Decide(const Game& game, int aiPlayerId, Random& rng);
};
