// ============================================================================
// Protocol.h - 网络协议（JSON 消息封装 / 序列化）
//
// 消息格式（设计文档 3.4 节）：
//   { "version": 1, "type": "...", "payload": { ... } }
// 典型消息类型：
//   "join"     客户端 -> 服务器：请求加入（携带昵称 / 断线 token）
//   "welcome"  服务器 -> 客户端：加入成功（分配 id 与 token）
//   "lobby"    服务器 -> 客户端：大厅玩家列表
//   "start"    服务器 -> 客户端：对局开始
//   "state"    服务器 -> 客户端：完整游戏状态（个性化：私有情报只发给本人）
//   "action"   客户端 -> 服务器：操作（射击 / 使用道具）
//   "event"    服务器 -> 客户端：事件广播（射击结果、淘汰、装弹等）
//   "error"    服务器 -> 客户端：操作被拒绝的原因
// ============================================================================
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../core/Game.h"
#include "../core/GameEvent.h"
#include "../core/GameView.h"

inline constexpr int PROTOCOL_VERSION = 1;

// 大厅玩家信息
struct LobbyPlayer {
    int id = -1;
    std::string name;
    bool host = false;
};

// 构造带版本号的通用消息
nlohmann::json MakeMsg(const std::string& type, const nlohmann::json& payload = nlohmann::json::object());

// GameEvent <-> JSON
nlohmann::json EventToJson(const GameEvent& event);
bool EventFromJson(GameEvent& out, const nlohmann::json& j);

// 大厅消息
nlohmann::json BuildLobbyJson(const std::vector<LobbyPlayer>& players, int hostId, const std::string& roomName);
bool ParseLobbyJson(std::vector<LobbyPlayer>& out, int& hostId, std::string& roomName, const nlohmann::json& j);

// 状态消息：按接收者个性化构建（他人只有道具数量，本人含道具明细与窥探情报）
nlohmann::json BuildStateJson(const Game& game, int viewerId, const std::string& roomName);
bool ParseStateJson(GameView& out, const nlohmann::json& payload);
