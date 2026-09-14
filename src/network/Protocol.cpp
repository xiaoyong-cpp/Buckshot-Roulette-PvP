#include "Protocol.h"

nlohmann::json MakeMsg(const std::string& type, const nlohmann::json& payload) {
    nlohmann::json j;
    j["version"] = PROTOCOL_VERSION;  // 协议版本化，便于未来升级
    j["type"] = type;
    j["payload"] = payload;
    return j;
}

nlohmann::json EventToJson(const GameEvent& event) {
    nlohmann::json j;
    j["kind"] = static_cast<int>(event.kind);
    j["playerId"] = event.playerId;
    j["targetId"] = event.targetId;
    j["flag"] = event.flag;
    j["value"] = event.value;
    j["value2"] = event.value2;
    j["text"] = event.text;
    return j;
}

bool EventFromJson(GameEvent& out, const nlohmann::json& j) {
    if (!j.is_object() || !j.contains("kind")) return false;
    out = GameEvent{};
    out.kind = static_cast<EventKind>(j["kind"].get<int>());
    out.playerId = j.value("playerId", -1);
    out.targetId = j.value("targetId", -1);
    out.flag = j.value("flag", false);
    out.value = j.value("value", 0);
    out.value2 = j.value("value2", 0);
    out.text = j.value("text", "");
    return true;
}

nlohmann::json BuildLobbyJson(const std::vector<LobbyPlayer>& players, int hostId, const std::string& roomName) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& p : players) arr.push_back({{"id", p.id}, {"name", p.name}, {"host", p.host}});
    return nlohmann::json{{"players", arr}, {"hostId", hostId}, {"roomName", roomName}};
}

bool ParseLobbyJson(std::vector<LobbyPlayer>& out, int& hostId, std::string& roomName, const nlohmann::json& j) {
    if (!j.is_object() || !j.contains("players")) return false;
    out.clear();
    hostId = j.value("hostId", 0);
    roomName = j.value("roomName", "");
    for (const auto& p : j["players"])
        out.push_back({p.value("id", -1), p.value("name", "?"), p.value("host", false)});
    return true;
}

nlohmann::json BuildStateJson(const Game& game, int viewerId, const std::string& roomName) {
    nlohmann::json j;
    j["state"] = static_cast<int>(game.GetState());
    j["round"] = game.Round();
    j["turn"] = game.CurrentTurnPlayerId();
    j["winner"] = game.WinnerId();
    j["pvp"] = game.IsPvp();
    j["selfId"] = viewerId;
    j["roomName"] = roomName;
    if (game.Mode()) j["modeId"] = game.Mode()->GetId();

    const Chamber& ch = game.GetChamber();
    j["shell"] = {
        {"count", ch.Count()},
        {"index", ch.Index()},
        {"live", ch.LiveRemaining()},
        {"blank", ch.BlankRemaining()},
        {"sawed", ch.sawed},
    };

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& p : game.Players()) {
        nlohmann::json jp;
        jp["id"] = p.id;
        jp["name"] = p.name;
        jp["hp"] = p.hp;
        jp["maxHp"] = p.maxHp;
        jp["alive"] = p.alive;
        jp["connected"] = p.connected;
        jp["skip"] = p.skipNextTurn;
        jp["turn"] = (p.id == game.CurrentTurnPlayerId());
        jp["count"] = p.ItemCount();
        if (p.id == viewerId) {
            // 私有信息：只发给本人
            nlohmann::json items = nlohmann::json::array();
            for (const auto& it : p.items) items.push_back(it);
            jp["items"] = items;
            jp["known"] = p.knownShellValid;
            jp["knownLive"] = p.knownShellLive;
        }
        arr.push_back(jp);
    }
    j["players"] = arr;
    return j;
}

bool ParseStateJson(GameView& out, const nlohmann::json& payload) {
    if (!payload.is_object()) return false;
    out = GameView{};
    out.valid = true;
    out.state = static_cast<GameState>(payload.value("state", 0));
    out.round = payload.value("round", 0);
    out.turnId = payload.value("turn", -1);
    out.winnerId = payload.value("winner", -1);
    out.pvp = payload.value("pvp", false);
    out.selfId = payload.value("selfId", -1);
    out.roomName = payload.value("roomName", "");
    out.modeId = payload.value("modeId", "");

    if (payload.contains("shell") && payload["shell"].is_object()) {
        const auto& sh = payload["shell"];
        out.shellCount = sh.value("count", 0);
        out.shellIndex = sh.value("index", 0);
        out.liveLeft = sh.value("live", 0);
        out.blankLeft = sh.value("blank", 0);
        out.sawed = sh.value("sawed", false);
    }

    if (payload.contains("players") && payload["players"].is_array()) {
        for (const auto& jp : payload["players"]) {
            ViewPlayer vp;
            vp.id = jp.value("id", -1);
            vp.name = jp.value("name", "?");
            vp.hp = jp.value("hp", 0);
            vp.maxHp = jp.value("maxHp", 4);
            vp.alive = jp.value("alive", true);
            vp.connected = jp.value("connected", true);
            vp.skipNext = jp.value("skip", false);
            vp.isTurn = jp.value("turn", false);
            vp.itemCount = jp.value("count", 0);
            vp.isSelf = (vp.id == out.selfId);
            if (jp.contains("items") && jp["items"].is_array()) {
                for (const auto& it : jp["items"]) vp.items.push_back(it.get<std::string>());
                if (vp.isSelf) {
                    out.known = jp.value("known", false);
                    out.knownLive = jp.value("knownLive", false);
                }
            }
            out.players.push_back(vp);
        }
    }

    // 获胜者名字
    for (const auto& vp : out.players) {
        if (vp.id == out.winnerId) out.winnerName = vp.name;
    }
    return true;
}
