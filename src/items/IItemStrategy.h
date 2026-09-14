// ============================================================================
// IItemStrategy.h - 道具策略抽象基类（设计模式：策略模式）
//
// 【开闭原则】新增道具只需：
//   1. 在 items/ 下新建类继承 IItemStrategy 并实现 Apply()
//   2. 用 ItemRegistrar 自注册到工厂（Items.cpp 中的写法）
//   3. 在 items_config.json 中登记名称与权重
//   无需修改 Game / UI / 网络任何其他文件。
// ============================================================================
#pragma once

#include <string>

#include "../core/GameContext.h"

class IItemStrategy {
public:
    virtual ~IItemStrategy() = default;

    // 道具类型标识（与 items_config.json 中的 "type" 对应）
    virtual std::string GetType() const = 0;

    // 是否需要指定目标（手铐 / 肾上腺素为 true）
    virtual bool NeedsTarget() const { return false; }

    // 道具效果：ctx 为游戏上下文，user 为使用者，target 为目标（无目标道具为 nullptr）
    virtual void Apply(GameContext& ctx, Player* user, Player* target) = 0;
};
