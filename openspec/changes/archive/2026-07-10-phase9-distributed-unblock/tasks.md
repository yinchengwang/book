# S11 — Tasks (Phase9 Distributed Unblock)

> **目标**：解开 learning/code-solutions/distributed/ 编译阻塞，让 phase9 分布式演进的学习副本能进入学习层构建。

## 1.1 调研

- [x] 1.1.1 已查：`RaftRouter` 类型在 3 个函数签名中出现，但从未定义
- [x] 1.1.2 已查：所有 `RaftRouter *router` 参数都 `(void)router;` 忽略——API 占位
- [x] 1.1.3 已查：4 个 .c 都 `#include "log.h"` 但无任何 log_* 函数调用——死 include

## 1.2 实施

- [ ] 1.2.1 `learning/code-solutions/distributed/coordinator.h` 加 `typedef struct RaftRouter_s RaftRouter;`
- [ ] 1.2.2 删除 4 个 .c 文件 `#include "log.h"`
- [ ] 1.2.3 解开 `learning/code-solutions/distributed/CMakeLists.txt`

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: `cmake -B build-learning -S learning` 配置成功
- [ ] 1.3.2 V2: `cmake --build build-learning` 全部编译
- [ ] 1.3.3 V3: `ls build-learning/lib/libdistributed-solutions.a` 库生成

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/phase9-distributed-unblock/ learning/code-solutions/distributed/`
- [ ] 1.4.2 `git commit -m "fix(phase9): 解开 distributed/ 编译阻塞 + RaftRouter 前向声明"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-phase9-distributed-unblock/`
