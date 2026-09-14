// ============================================================================
// Strings.h - 全部中文界面/日志/错误文本集中管理
//
// 用途：
//   1. 统一管理用户可见文案（便于维护与未来国际化）
//   2. 作为字体字形扫描语料：FontManager 扫描 All() 中的所有字符，
//      保证 raylib 加载的字体覆盖界面所需的全部 CJK 字形。
//
// 注意：日志/错误模板中的占位符统一用 {a}/{b}/{it}/{d}/{l}/{n}/{p}，
//       运行时由格式化函数替换。所有占位符均为 ASCII。
// ============================================================================
#pragma once

#include <string>

namespace S {

// ---- 主菜单 ----
inline constexpr const char* Title = "恶魔轮盘";
inline constexpr const char* Subtitle = "BUCKSHOT ROULETTE";
inline constexpr const char* BtnSingle = "单机模式";
inline constexpr const char* BtnCreate = "创建房间";
inline constexpr const char* BtnJoin = "加入房间";
inline constexpr const char* BtnQuit = "退出游戏";
inline constexpr const char* MenuHint = "点击下方按钮开始";

// ---- 创建房间 ----
inline constexpr const char* CreateTitle = "创建房间";
inline constexpr const char* LabelNickname = "昵称：";
inline constexpr const char* LabelRoomName = "房间名：";
inline constexpr const char* LabelPort = "端口：";
inline constexpr const char* LabelMaxPlayers = "最大人数：";
inline constexpr const char* BtnEnterLobby = "进入大厅";
inline constexpr const char* BtnBack = "返回";

// ---- 加入房间 ----
inline constexpr const char* JoinTitle = "加入房间";
inline constexpr const char* LabelServerIP = "服务器 IP：";
inline constexpr const char* LabelServerPort = "端口：";
inline constexpr const char* BtnConnect = "连接";
inline constexpr const char* Connecting = "连接中…";
inline constexpr const char* ConnectFail = "连接失败，请检查 IP 与端口";

// ---- 大厅 ----
inline constexpr const char* LobbyTitle = "房间大厅";
inline constexpr const char* LabelRoom = "房间：";
inline constexpr const char* TagHost = "（房主）";
inline constexpr const char* WaitHost = "等待房主开始游戏…";
inline constexpr const char* WaitPlayers = "等待玩家加入…";
inline constexpr const char* BtnStart = "开始游戏";
inline constexpr const char* BtnLeave = "离开房间";
inline constexpr const char* KickStarted = "游戏已开始";

// ---- 游戏界面 ----
inline constexpr const char* LabelRound = "第 {n} 轮";
inline constexpr const char* YourTurn = "你的回合";
inline constexpr const char* WaitingTurn = "等待 {n} 行动…";
inline constexpr const char* BtnShootSelf = "射击自己";
inline constexpr const char* BtnShootTarget = "射击对手";
inline constexpr const char* BtnShootDemon = "射击恶魔";
inline constexpr const char* BtnCancel = "取消";
inline constexpr const char* HintChooseTarget = "点击右侧玩家选择目标";
inline constexpr const char* LabelItems = "道具";
inline constexpr const char* LabelReload = "装弹：实 {l}  空 {b}";
inline constexpr const char* LabelLive = "实";
inline constexpr const char* LabelBlank = "空";
inline constexpr const char* LabelSawed = "已锯断！";
inline constexpr const char* LabelKnownLive = "已知：实弹";
inline constexpr const char* LabelKnownBlank = "已知：空弹";
inline constexpr const char* LabelChamberEmpty = "弹仓已空";
inline constexpr const char* BannerExtra = "额外回合！";
inline constexpr const char* BannerElim = "{p} 被淘汰！";
inline constexpr const char* BannerWin = "{p} 获得了胜利！";
inline constexpr const char* BannerReload = "重新装弹：实 {l}  空 {b}";
inline constexpr const char* BtnBackToMenu = "返回主菜单";
inline constexpr const char* TagOnline = "在线";
inline constexpr const char* TagDisconnected = "掉线";
inline constexpr const char* TagCuffed = "手铐";
inline constexpr const char* TagSelf = "（你）";
inline constexpr const char* LabelPlayers = "玩家";

// ---- 日志事件模板 ----
inline constexpr const char* LogShootSelfLive = "{a} 对自己开枪 → 实弹！-{d} HP";
inline constexpr const char* LogShootSelfBlank = "{a} 对自己开枪 → 空弹！额外回合";
inline constexpr const char* LogShootTargetLive = "{a} 对 {b} 开枪 → 实弹！-{d} HP";
inline constexpr const char* LogShootTargetBlank = "{a} 对 {b} 开枪 → 空弹";
inline constexpr const char* LogItemUsed = "{a} 使用了 {it}";
inline constexpr const char* LogHandcuff = "{a} 给 {b} 上了手铐";
inline constexpr const char* LogSteal = "{a} 偷走了 {b} 的 {it}";
inline constexpr const char* LogBeerLive = "{a} 使用啤酒，退出一发：实弹";
inline constexpr const char* LogBeerBlank = "{a} 使用啤酒，退出一发：空弹";
inline constexpr const char* LogMagnifier = "{a} 看清了当前弹槽";
inline constexpr const char* LogCigarette = "{a} 抽了根烟，恢复 {d} HP（吸烟有害健康）";
inline constexpr const char* LogCigaretteFull = "{a} 满血，香烟无效果";
inline constexpr const char* LogExpiredMedGood = "{a} 服用过期药品，回复 {d} HP";
inline constexpr const char* LogExpiredMedBad = "{a} 服用过期药品，中毒 -{d} HP";
inline constexpr const char* LogReverserLive = "{a} 使用反转器：实弹→空弹";
inline constexpr const char* LogReverserBlank = "{a} 使用反转器：空弹→实弹";
inline constexpr const char* LogHeal = "{a} 恢复了 {d} HP";
inline constexpr const char* LogReload = "重新装弹：实 {l}  空 {b}";
inline constexpr const char* LogElim = "{p} 被淘汰！";
inline constexpr const char* LogWin = "{p} 获得了胜利！";
inline constexpr const char* LogJoin = "{p} 加入了房间";
inline constexpr const char* LogLeave = "{p} 离开了房间";
inline constexpr const char* LogDisconnect = "{p} 掉线，AI 托管中";
inline constexpr const char* LogReconnect = "{p} 已重连";
inline constexpr const char* LogSkip = "{p} 被手铐束缚，跳过回合";
inline constexpr const char* LogBagFull = "背包已满，丢弃了 {it}";
inline constexpr const char* LogStart = "对局开始！";

// ---- 服务器错误消息（也需中文字形）----
inline constexpr const char* ErrNotYourTurn = "不是你的回合";
inline constexpr const char* ErrGameNotStarted = "游戏未开始";
inline constexpr const char* ErrRoomFull = "房间已满";
inline constexpr const char* ErrGameStarted = "游戏已开始，无法加入";
inline constexpr const char* ErrNotJoined = "未加入房间";
inline constexpr const char* ErrDisconnected = "掉线中，无法操作";
inline constexpr const char* ErrUnknown = "未知操作";

// ---- 连接状态 ----
inline constexpr const char* StatusDisconnected = "与服务器断开连接";
inline constexpr const char* BtnReconnect = "重连";
inline constexpr const char* Reconnecting = "重连中…";

// 汇集全部文本作为字形扫描语料
inline std::string All() {
    std::string s;
    s += Title;        s += Subtitle;   s += BtnSingle;   s += BtnCreate; s += BtnJoin;
    s += BtnQuit;      s += MenuHint;   s += CreateTitle;  s += LabelNickname;
    s += LabelRoomName; s += LabelPort;  s += LabelMaxPlayers; s += BtnEnterLobby;
    s += BtnBack;      s += JoinTitle;  s += LabelServerIP; s += LabelServerPort;
    s += BtnConnect;   s += Connecting; s += ConnectFail;  s += LobbyTitle; s += LabelRoom;
    s += TagHost;      s += WaitHost;   s += WaitPlayers; s += BtnStart; s += BtnLeave;
    s += KickStarted;  s += LabelRound; s += YourTurn; s += WaitingTurn; s += BtnShootSelf;
    s += BtnShootTarget; s += BtnShootDemon; s += BtnCancel; s += HintChooseTarget;
    s += LabelItems;   s += LabelReload; s += LabelLive; s += LabelBlank; s += LabelSawed;
    s += LabelKnownLive; s += LabelKnownBlank; s += LabelChamberEmpty; s += BannerExtra;
    s += BannerElim;   s += BannerWin;  s += BannerReload; s += BtnBackToMenu; s += TagOnline;
    s += TagDisconnected; s += TagCuffed; s += TagSelf; s += LabelPlayers; s += LogShootSelfLive; s += LogShootSelfBlank;
    s += LogShootTargetLive; s += LogShootTargetBlank; s += LogItemUsed; s += LogHandcuff;
    s += LogSteal;     s += LogBeerLive; s += LogBeerBlank; s += LogMagnifier; s += LogReload;
    s += LogElim;      s += LogWin;     s += LogJoin; s += LogLeave; s += LogDisconnect;
    s += LogReconnect; s += LogSkip;    s += LogBagFull; s += LogStart;
    s += LogCigarette; s += LogCigaretteFull; s += LogExpiredMedGood; s += LogExpiredMedBad;
    s += LogReverserLive; s += LogReverserBlank; s += LogHeal;
    s += ErrNotYourTurn; s += ErrGameNotStarted; s += ErrRoomFull; s += ErrGameStarted;
    s += ErrNotJoined; s += ErrDisconnected; s += ErrUnknown; s += StatusDisconnected;
    s += BtnReconnect; s += Reconnecting;
    return s;
}

// 简易占位符替换：把模板中的 {key} 替换为 val
inline std::string Replace(std::string tpl, const std::string& key, const std::string& val) {
    std::string tag = "{" + key + "}";
    size_t pos = 0;
    while ((pos = tpl.find(tag, pos)) != std::string::npos) {
        tpl.replace(pos, tag.size(), val);
        pos += val.size();
    }
    return tpl;
}

}  // namespace S
