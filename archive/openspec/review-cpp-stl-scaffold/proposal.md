## Why

`mystl::vector::pop_back` 当前直接调用元素析构函数，绕过了容器持有的分配器，无法遵守自定义分配器的销毁语义。需要用可观测的边界条件回归测试锁定行为，并以最小改动修复。

## What Changes

- 为 `vector::pop_back` 增加覆盖尾元素销毁位置与单元素边界的回归测试。
- 将 `pop_back` 的销毁操作改为在递减 `finish_` 后调用容器分配器的 `destroy`。
- 保持非空容器上的标准行为，不改变空容器调用仍属于未定义行为的约定。

## Capabilities

### New Capabilities
- `learning-mystl-vector-pop-back`: 定义 `mystl::vector::pop_back` 的尾元素销毁、分配器协作和边界条件行为。

### Modified Capabilities

无。

## Impact

- 受影响代码：`learning/scaffold/cpp/stl/include/mystl/vector.h`。
- 受影响测试：`learning/scaffold/cpp/stl/test/vector_pop_back_test.cpp`。
- 不新增运行时依赖，不改变公开 API 签名。
