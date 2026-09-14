#include "SingleMode.h"
#include "ModeFactory.h"

// 自注册（开闭原则）：静态对象构造时把自己登记进工厂
static ModeRegistrar reg_single("single", std::make_shared<SingleMode>());
