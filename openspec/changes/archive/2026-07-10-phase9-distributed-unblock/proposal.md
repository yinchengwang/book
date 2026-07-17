# S11 — Phase9 Distributed Unblock (解开 learning/code-solutions/distributed)

## What Changes

学习层 `learning/code-solutions/distributed/` 包含 phase9 分布式演进（coordinator/dist_txn/raft/shard）的副本，**自 S5 起一直被注释**：

```cmake
# distributed/CMakeLists.txt (current)
message(STATUS "distributed-solutions: DISABLED (phase9 待续做)")
```

**问题**：

1. **类型缺失**：`coordinator.h` 和 `coordinator.c` 引用 `RaftRouter *router` 类型，但 `RaftRouter` 从未在任何头文件中定义
2. **死代码 include**：4 个 .c 文件都 `#include "log.h"`，但实际没有任何 `log_*` 函数被调用
3. **CMakeLists.txt 全部被注释**：无法编译为 `distributed-solutions` 静态库

调查确认：所有 `RaftRouter *router` 参数都被 `(void)router;` 显式忽略——**没有实际使用**，仅作为 API 模板。这意味着只要为 `RaftRouter` 提供**前向声明**（forward declaration），代码就能编译通过。

**变更内容**：

1. **添加 `RaftRouter` 前向声明**：`learning/code-solutions/distributed/coordinator.h` 在文件头加 `typedef struct RaftRouter_s RaftRouter;`
2. **移除 4 个 .c 文件的死代码 include**：删除 `#include "log.h"`
3. **解开 CMakeLists.txt 注释**：启用 `distributed-solutions` 静态库
4. **验证学习层 cmake build-learning 成功**

## Why

**β 价值（学习日志）**：
- learning 轨道唯一被标注"DISABLED"的就是 distributed/
- 解开 = 学习层完整闭环（ds-c/orig/ + algo-c/ + code-solutions/{c,interview,distributed}）

**前置依赖**：
- S5 已把 distributed/ 整目录迁入学习层
- S10 specs 整理已把 quiz-file-split 等相似债处理掉

## Scope

**包含**：
- coordinator.h 加 `typedef struct RaftRouter_s RaftRouter;`
- 4 个 .c 文件删除 `#include "log.h"`
- 启用 CMakeLists.txt 编译 distributed-solutions 库
- V1-V3 验证：cmake 配置 + 编译 + 链接

**不包含**：
- ❌ 实际实现 RaftRouter（仅前向声明，让参数签名能编译）
- ❌ 跨轨链接到工程层 db/core/log.h（S11 严格遵守双轨）
- ❌ 集成 ctest（distributed/ 没测试用例）
- ❌ 提供 log.h 实现（应该让 coordinator.c 不需要 log.h，去掉死代码）

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| 编译时其他类型缺失 | 低 | grep 所有 coordinator.h 类型，已确认仅 RaftRouter |
| 其他死代码 include 仍有 | 中 | 编译会暴露，但 cmake build 失败即解决 |
| phase9 业务逻辑不完整（仅 API 模板） | 中 | S11 范围"仅解开编译"，逻辑完善属 S11+ 后续 |

## 不做（明确范围外）

- ❌ 实现 RaftRouter 完整逻辑（仅前向声明）
- ❌ 给 distributed/ 加 ctest 测试（无现成测试，仅 API 模板）
- ❌ 重构 phase9 业务逻辑（coordinator/dist_txn/raft/shard）
- ❌ 修改工程层 db/core/ 任何代码
