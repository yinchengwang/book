# S11 — 设计文档（Phase9 Distributed Unblock）

## 1. 根因分析

`learning/code-solutions/distributed/` 4 个 .c 文件都 `#include "log.h"`，但实际**没有任何 log_ 函数被调用**——属于死代码 include。这导致编译时需要 `log.h` 在 include 路径中。

`coordinator.{h,c}` 引用 `RaftRouter` 类型但从未定义。代码中所有 `RaftRouter *router` 参数都被 `(void)router;` 显式忽略——说明这是**预留 API**而非实际实现。

## 2. 修复策略

**两步即可解开编译**：
1. 删除死代码 include（4 个文件 `#include "log.h"` 都不需要）
2. 给 `coordinator.h` 添加 `RaftRouter` 前向声明

## 3. 实施

### 3.1 coordinator.h 加前向声明

```c
// 在 coordinator.h 文件头添加
typedef struct RaftRouter_s RaftRouter;
```

### 3.2 删除 4 个 .c 的死代码 include

```bash
sed -i '/#include "log.h"/d' learning/code-solutions/distributed/{coordinator,dist_txn,raft,shard}.c
```

### 3.3 解开 CMakeLists.txt

替换注释代码：

```cmake
file(GLOB_RECURSE DISTRIBUTED_SOURCES CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c" "*.cpp"
)
list(FILTER DISTRIBUTED_SOURCES EXCLUDE REGEX "CMakeLists\.txt$")
if(DISTRIBUTED_SOURCES)
    add_library(distributed-solutions STATIC ${DISTRIBUTED_SOURCES})
    target_include_directories(distributed-solutions PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(distributed-solutions PUBLIC learning_includes)
    set_target_properties(distributed-solutions PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        FOLDER "Libraries"
    )
endif()
```

## 4. 验证 V1-V3

| V | 命令 | 期望 |
|---|---|---|
| V1 | `rm -rf build-learning && cmake -B build-learning -S learning` | 0 error |
| V2 | `cmake --build build-learning` | 0 error |
| V3 | `ls build-learning/lib/libdistributed-solutions.a` | 文件存在 |

## 5. 不做

- ❌ 实现 RaftRouter 完整逻辑
- ❌ 加 ctest 测试（无现成测试）
- ❌ 跨轨链接
