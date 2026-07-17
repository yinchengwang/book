# S11 Spec —— Phase9 Distributed 学习层解锁

## 1. 类型契约

`learning/code-solutions/distributed/` 中 `coordinator.{h,c}` 使用的 `RaftRouter` 类型必须有**前向声明**：

```c
typedef struct RaftRouter_s RaftRouter;
```

理由：`RaftRouter` 仅作为参数类型（API 模板），实际实现无需提供。

## 2. include path 洁化

- ❌ 不得 `#include "log.h"`（学习层无 log 实现）
- ✅ 使用 `<stdbool.h>` / `<stdint.h>` 等标准 C 头
- ✅ 使用本地头 `"raft.h"` / `"shard.h"` / `"coordinator.h"`

## 3. distributed-solutions 库契约

`learning/code-solutions/distributed/CMakeLists.txt` 必须：
- ✅ add_library(distributed-solutions STATIC ...)
- ✅ target_include_directories 指向 `${CMAKE_CURRENT_SOURCE_DIR}` 和 `learning/include`
- ✅ target_link_libraries distributed-solutions PUBLIC learning_includes
- ❌ 不得依赖工程层任何路径

## 4. 不做（范围外）

- ❌ 实现 RaftRouter 完整算法
- ❌ 引入工程层 db/core 头（如 log.h / shard.h 工程版）
- ❌ 给 distributed/ 加 ctest（仅解开编译）
