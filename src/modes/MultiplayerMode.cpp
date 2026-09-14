#include "MultiplayerMode.h"
#include "ModeFactory.h"

// 自注册（开闭原则）
static ModeRegistrar reg_multiplayer("multiplayer", std::make_shared<MultiplayerMode>());
