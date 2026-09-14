# 恶魔轮盘 Buckshot Roulette

单机 + 联网双模式的《恶魔轮盘》实现，C++23 / raylib 5.5 / nlohmann-json / Winsock，遵循开闭原则与零命令行 UI（启动即窗口，全程鼠标操作）。

## 特性

- **单机模式（PVE）**：玩家 vs 恶魔 AI，AI 会智能使用放大镜/手锯/啤酒。
- **联网模式（PVP）**：2~8 人主机-客户端架构，局域网 TCP，断线 60 秒内重连（期间 AI 托管）。
- **5 种道具**：放大镜、手锯、啤酒、手铐（PVP）、肾上腺素（PVP），策略模式实现，新增道具零侵入。
- **弹仓状态机**：8 格转轮，实/空随机洗牌，击发/退弹/手锯伤害翻倍。
- **N 人支持**：按加入顺序循环回合，淘汰即跳过，最后存活者获胜。
- **零命令行**：主菜单 → 创建/加入房间 → 大厅 → 对局，全部图形交互。
- **开箱即用**：首次运行自动生成 `config.json` / `items_config.json`，无需手动配置。

## 编译与运行

### 依赖（均已安装在 MinGW 包含目录，无需额外 -I）

- MinGW-w64 GCC 14.2+（C++20）
- raylib 5.5
- nlohmann/json
- utfcpp
- Winsock2（Windows 原生）

### 编译

双击 `build.bat`，或在项目根目录执行 PowerShell：

```powershell
g++ -std=c++20 -O2 -mwindows @build.rsp -o BuckshotRoulette.exe `
    -lraylib -lws2_32 -lgdi32 -lopengl32 -lwinmm -static
```

`build.bat` 会自动收集 `src/` 下所有 `.cpp` 写入 `build.rsp` 再编译链接。成功后生成 `BuckshotRoulette.exe`，双击即可运行（无控制台窗口）。

### 运行

双击 `BuckshotRoulette.exe`。**分发时需连同 `assets/` 目录一起携带**（内含 `simhei.ttf` 中文字体与 `cjk_common.txt` 常用字表），否则将回退到系统字体。首次运行会在 exe 同目录生成：

- `config.json` —— 全局参数（窗口、生命值、道具数、端口等）
- `items_config.json` —— 道具注册表（名称、权重、是否 PVP 专属）
- `logs/game_YYYYMMDD_HHMMSS.log` —— 运行日志

## 玩法

### 主菜单

- **单机模式**：立即开始与恶魔 AI 的对局。
- **创建房间**：输入昵称、房间名、端口、最大人数（2~8），启动服务器并进入大厅。
- **加入房间**：输入昵称、主机 IP、端口，连接后进入大厅。

### 对局规则

- 初始生命值 4 点（可配置）。每局随机装填实弹/空弹至 8 格弹仓。
- **射击自己**：空弹无事且获得额外回合；实弹自伤 1 点。
- **射击对手**：实弹扣目标 1 点（手锯翻倍为 2）；空弹无事，回合结束。
- 弹仓打空后自动重新装弹。
- 生命归零即淘汰，最后存活者获胜。

### 道具（每轮开始随机获得 0~2 个，背包上限 5）

| 道具 | 效果 | 模式 |
|------|------|------|
| 放大镜 | 查看下一发实/空（仅自己可见，不消耗回合） | 通用 |
| 手锯 | 下一发实弹伤害翻倍为 2（空弹则浪费） | 通用 |
| 啤酒 | 退出当前弹并丢弃（不消耗回合） | 通用 |
| 手铐 | 指定目标下回合被跳过 | PVP |
| 肾上腺素 | 偷取目标一个随机道具 | PVP |

道具使用时机：自己回合内、射击之前，可连续使用任意数量。

## 项目结构

```
src/
├── core/          # 游戏核心（Game, Player, Chamber, GameEvent, GameContext, AIController, GameState, GameView, Strings）
├── items/         # 道具策略（IItemStrategy, 5 种道具, ItemFactory）
├── modes/         # 模式接口（IMode, SingleMode, MultiplayerMode, ModeFactory）
├── network/       # 网络封装（Protocol, TcpServer, TcpClient, HostSession, ClientSession, NetworkCore）
├── ui/            # raylib 界面（SceneManager, MenuScene, CreateRoomScene, JoinScene, LobbyScene, GameScene, Widgets, FontManager, Scene）
└── main.cpp       # 入口
assets/
├── simhei.ttf         # 中文字体（黑体，可自由分发，随程序携带）
└── cjk_common.txt     # GB2312 一级常用字表（3760 字，覆盖玩家昵称等动态文本）
```

## 设计模式

- **策略模式（道具）**：`IItemStrategy::Apply(GameContext&, Player*, Player*)`，每种道具一个子类，`ItemFactory` 按类型字符串创建实例。
- **工厂模式 + 自注册**：`ItemRegistrar` 在静态初始化时把类型注册进 `ItemFactory`，`items_config.json` 决定启用与权重。**新增道具只需在 `Items.cpp` 追加一个策略类 + 一行注册**，无需改动其它文件（开闭原则）。
- **观察者模式**：`Game` 通过 `IGameObserver` 把 `GameEvent` 分发给 UI（刷新界面）与网络中继（转发客户端）；`IEventSink` 让道具反向回传事件。
- **状态模式**：`GameState`（Waiting/RoundEnd/Reloading/Aiming/ItemSelect/Shooting/GameOver）驱动对局流程。
- **单例模式**：`ConfigLoader`、`Logger`、`NetworkCore`、`ItemFactory`、`ModeFactory`、`FontManager`。

## 网络架构

- 主机-客户端模型：主机运行权威 `Game`，客户端仅发送 `action`、接收 `state`/`event`。
- TCP + 4 字节长度前缀帧格式，后台线程 select 收发，主线程通过消息队列通信（raylib 主循环线程安全）。
- JSON 消息：`join` / `state` / `action` / `event` / `start`。
- 状态同步 10 Hz（`config.json` 的 `network.syncHz`），事件即时广播。
- 断线 60 秒内可重连，期间该玩家由 AI 托管（`reconnectTimeout` 配置）。

## 可扩展性

### 新增道具

1. 在 `src/items/Items.h` 声明新策略类，继承 `IItemStrategy`。
2. 在 `src/items/Items.cpp` 实现 `Apply`，并追加一行 `static ItemRegistrar`。
3. 运行时自动注册；如需调整名称/权重，编辑 `items_config.json`。

无需改动 `Game`、UI 或网络层。

### 新增模式

1. 实现 `IMode` 接口（`GetId`/`GetName`/`GenerateReload`/`IsPvp`）。
2. 用 `ModeRegistrar` 自注册。
3. 在 `config.json` 的 `modes` 数组登记 id。

主菜单会自动加载新模式按钮。

## 测试

`headless_test.cpp` 是无头逻辑测试（不依赖 raylib/Winsock），验证核心规则：

```powershell
g++ -std=c++20 -O2 @test_build.rsp -o headless_test.exe
.\headless_test.exe
```

覆盖项（9 项全部通过）：

- 单机/PVP 2人/PVP 4人自动对局完整性（多随机种子，验证必然终局、HP 不变量、唯一存活获胜者）
- 弹仓装填与击发计数
- 手锯实弹伤害翻倍
- 单机模式拒绝手铐
- 放大镜揭示当前弹
- PVP 手铐使目标跳过下回合
- PVP 肾上腺素偷取道具

## 配置参考

`config.json`：

```json
{
  "window": { "width": 1280, "height": 720 },
  "game": {
    "defaultHp": 4,
    "maxItems": 5,
    "itemsPerRoundMin": 0,
    "itemsPerRoundMax": 2,
    "maxPlayers": 4,
    "maxPlayersCap": 8,
    "reconnectTimeout": 60
  },
  "network": { "port": 7777, "syncHz": 10 },
  "font": { "size": 32 },
  "modes": ["single", "multiplayer"]
}
```

`items_config.json` 中每条记录：`type`（注册标识）、`name`（显示名）、`weight`（抽取权重）、`pvpOnly`（是否仅 PVP）。

## 运行验证

- 编译：0 错误 0 警告，生成 `BuckshotRoulette.exe`（约 5MB，静态链接）。
- 启动：日志显示 Logger → Config → Winsock → items_config → 模式加载 → 常用字表（cjk_common.txt）→ 中文字体（assets/simhei.ttf，3864 字形）→ 主循环 → 干净退出（退出码 0）。
- 无头测试：9/9 通过。

## 技术备注

- **raylib 与 windows.h 符号冲突**：`Rectangle`/`CloseWindow`/`ShowCursor` 同名冲突。在 `network/NetworkCore.h`、`TcpServer.h`、`TcpClient.h` 包含 `winsock2.h` 前定义 `NOGDI`/`NOUSER`/`NOMINMAX`/`WIN32_LEAN_AND_MEAN` 屏蔽。
- **raylib 5.5 API**：`DrawRectangleRoundedLines` 不再接受线宽参数，带线宽版本改用 `DrawRectangleRoundedLinesEx`。
- **中文字体**：`FontManager` 优先加载项目自带 `assets/simhei.ttf`（黑体，可自由分发），字形语料 = 界面文案 + `assets/cjk_common.txt`（GB2312 一级字 3760 个）+ ASCII，共 3864 字形，覆盖玩家昵称等动态文本。不再依赖系统的 `msyh.ttc`（版权 + 可移植性）。字体缺失时回退系统 `simhei.ttf`/`simsun.ttc`/`Deng.ttf`。
- **线程安全**：网络收发在独立线程，主线程通过互斥锁保护的队列通信，不直接调用 raylib。
