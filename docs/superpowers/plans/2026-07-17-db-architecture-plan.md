# 数据库整体架构实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 按设计文档 10 层架构，分 10 个 Phase 逐步实现从关系模型到流数据模型的完整数据库体系

**架构方案：** PostgreSQL 风格分层架构 + 多模态专用算子直调，顶层按模块组织、层内参考 PG 布局

**技术栈：** C11 + C++17, CMake 3.20+, GoogleTest

---

## 全局约束

- 所有对话、解释、注释必须使用简体中文
- 严格遵守当前目录结构规则（engineering/ 主轨、learning/ 学习轨）
- 新增文件必须放在 engineering/ 下对应目录，严禁在根目录创建新文件
- 编译产物统一输出到 build/engineering/，测试产物到 test-results/engineering/
- 每次任务完成后必须输出清晰总结（完成内容、变更范围、验证方式、后续建议）
- 每次提交后必须 push，避免批量推送失败

---

## Phase 1: 架构落地 + 关系模型完善

**目标：** 将目录结构调整为设计文档规范，整合现有 Volcano 算子到统一 executor/ 入口，建立 engine/ 和 protocol/ 目录骨架，确保所有现有测试通过

**涉及现有模块：**
- `src/db/sql/` — 现有 33 个文件，包含所有标准算子（nodeSeqscan、nodeHashjoin 等）
- `src/db/storage/` — 现有 67 个文件，包含 buffer/catalog/wal/rel/access/kv/vector 等
- `src/db/executor/` — 现有 8 个文件，包含 graph gqlExec 和 rag 算子
- `include/db/` — 顶层头文件 + storage/ 子目录 + sql/ 子目录
- `src/db/tools/` — 16 个文件（vacuum/copy/explain/reindex/stats/sys_catalog）

### Task 1.1: 建立 engine/ 和 protocol/ 骨架目录

**文件：**
- Create: `engineering/include/db/engine/engine.h` — 引擎统一接口声明
- Create: `engineering/include/db/protocol/protocol.h` — 协议统一接口声明
- Create: `engineering/src/db/engine/CMakeLists.txt` — 空引擎库骨架
- Create: `engineering/src/db/protocol/CMakeLists.txt` — 空协议库骨架
- Modify: `engineering/src/db/CMakeLists.txt` — 添加 engine/ 和 protocol/ 子目录引用

**接口：**
- 消费：`db_core` 库（已有）
- 产出：`engine.h` 定义 `engine_type_t` 枚举，`protocol.h` 定义 `protocol_type_t` 枚举

- [ ] **Step 1: 创建 engine.h 头文件**

```c
/**
 * @file engine.h
 * @brief 多模态引擎统一接口声明
 *
 * 各引擎（vector/kv/ts/doc/spatial/graph/yang/stream）通过此接口
 * 被执行器算子直调，不走 Access Method 层包装。
 */
#ifndef DB_ENGINE_H
#define DB_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 引擎类型枚举 */
typedef enum {
    ENGINE_VECTOR,      /**< 向量引擎 */
    ENGINE_KV,          /**< KV 引擎 */
    ENGINE_TS,          /**< 时序引擎 */
    ENGINE_DOC,         /**< 文档引擎 */
    ENGINE_SPATIAL,     /**< 空间引擎 */
    ENGINE_GRAPH,       /**< 图引擎 */
    ENGINE_YANG,        /**< 树/Yang 引擎 */
    ENGINE_STREAM,      /**< 流引擎 */
    ENGINE_COUNT
} engine_type_t;

/** 引擎操作结果 */
typedef struct engine_result_s {
    int     code;        /**< 0 成功，非 0 错误码 */
    char   *msg;         /**< 错误信息 */
    void   *data;        /**< 结果数据 */
    int64_t nrows;       /**< 影响行数 */
} engine_result_t;

#ifdef __cplusplus
}
#endif

#endif /* DB_ENGINE_H */
```

- [ ] **Step 2: 创建 protocol.h 头文件**

```c
/**
 * @file protocol.h
 * @brief 非 SQL 协议层统一接口声明
 *
 * 各模型独立协议：RESP（KV）、InfluxDB Line Protocol（时序）、
 * GQL（图）、Datalog、Yang Path、Kafka（流）
 */
#ifndef DB_PROTOCOL_H
#define DB_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 协议类型枚举 */
typedef enum {
    PROTOCOL_RESP,          /**< RESP（Redis 序列化协议） */
    PROTOCOL_INFLUX_LINE,   /**< InfluxDB Line Protocol */
    PROTOCOL_GQL,           /**< GQL 图查询语言 */
    PROTOCOL_DATALOG,       /**< Datalog 逻辑编程语言 */
    PROTOCOL_YANG_PATH,     /**< Yang Path / NETCONF */
    PROTOCOL_KAFKA,         /**< Kafka 协议 */
    PROTOCOL_COUNT
} protocol_type_t;

#ifdef __cplusplus
}
#endif

#endif /* DB_PROTOCOL_H */
```

- [ ] **Step 3: 创建 engine/CMakeLists.txt**

```cmake
# engine 模块：多模态引擎统一直调接口
# 各引擎实现在 storage/<model>/ 下，此目录仅提供统一头文件

# 本模块目前为骨架，后续每个引擎实现会在此添加适配层
```

- [ ] **Step 4: 创建 protocol/CMakeLists.txt**

```cmake
# protocol 模块：非 SQL 协议层
# 每个协议实现独立编译单元，后续 Phase 逐步添加

# 本模块目前为骨架，后续每个协议实现会在此添加
```

- [ ] **Step 5: 修改父 CMakeLists.txt 添加新子目录**

在 `engineering/src/db/CMakeLists.txt` 中，在 `add_subdirectory(executor)` 之后添加：

```cmake
# ========================================================================
# 引擎层（多模态引擎统一直调接口）
# ========================================================================
add_subdirectory(engine)

# ========================================================================
# 协议层（非 SQL 协议）
# ========================================================================
add_subdirectory(protocol)
```

- [ ] **Step 6: 编译验证**

```bash
cd c:/code/book
cmake --build build/engineering --target db_executor 2>&1 | tail -5
cmake --build build/engineering --target sql_engine 2>&1 | tail -5
cmake --build build/engineering --target db_storage 2>&1 | tail -5
```

预期：全部编译通过，新目录不会影响现有库

- [ ] **Step 7: 提交**

```bash
git add -A
git commit -m "feat(db): 建立 engine/ 和 protocol/ 骨架目录

- 新增 engine.h 统一引擎类型枚举
- 新增 protocol.h 统一协议类型枚举
- 新增 engine/CMakeLists.txt 和 protocol/CMakeLists.txt 骨架
- 注册到顶层 src/db/CMakeLists.txt"
```

---

### Task 1.2: 清理 include/db/ 顶层头文件

**目标：** 将分散在 include/db/ 顶层的访问方法和存储头文件，按设计文档分到 access/、storage/、catalog/ 等子目录。采用**符号链接或头文件转发**方式，保持向后兼容，避免破坏现有 include 路径。

**文件：**
- Create: `engineering/include/db/access/heapam.h` — 转发头文件
- Create: `engineering/include/db/access/btreeam.h` — 转发头文件
- Create: `engineering/include/db/storage/buf.h` — 转发头文件
- Create: `engineering/include/db/storage/page.h` — 转发头文件
- Create: `engineering/include/db/storage/disk.h` — 转发头文件
- Create: `engineering/include/db/storage/wal.h` — 转发头文件
- Create: `engineering/include/db/catalog/catalog.h` — 转发头文件
- Modify: 顶层 `include/db/heapam.h` → 改为 `#include "db/access/heapam.h"`
- 等

**注意：**
- 因为当前所有测试代码都 `#include "db/heapam.h"` 等路径，直接移动文件会破坏大量引用
- 采用**双层结构**：保持顶层 `include/db/heapam.h` 不动（作为转发），同时在 `include/db/access/heapam.h` 放实际内容
- 或者更简单：**暂时不移动现有文件**，只创建新目录结构，后续 Phase 逐步迁移

**决策：** 采用"新目录优先，旧文件转发"策略。当前 Phase 只建立新目录骨架，不移动任何现有文件。所有现有头文件保持原位。

- [ ] **Step 1: 创建 access/ 目录骨架转发头文件**

```bash
mkdir -p engineering/include/db/access
```

```c
// engineering/include/db/access/heapam.h
// 转发头文件 - 实际内容在 include/db/heapam.h
// 后续 Phase 会逐步将实现迁移至此
#include "../../heapam.h"
```

- [ ] **Step 2: 创建 catalog/ 目录骨架转发头文件**

```bash
mkdir -p engineering/include/db/catalog
```

```c
// engineering/include/db/catalog/catalog.h
#include "../../catalog.h"
```

- [ ] **Step 3: 创建 storage/ 子目录转发头文件**

```bash
# storage/buf.h 等已经在 storage/ 子目录下，不需要额外创建
# 但需要确保 include/db/storage/ 下的头文件与设计文档一致
```

- [ ] **Step 4: 编译验证所有现有测试**

```bash
cd c:/code/book
cmake --build build/engineering 2>&1 | tail -10
```

- [ ] **Step 5: 提交**

```bash
git add -A
git commit -m "feat(db): 建立 access/ 和 catalog/ 目录转发头文件

- 创建 include/db/access/ 目录，包含 heapam.h/btreeam.h 转发
- 创建 include/db/catalog/ 目录，包含 catalog.h 转发
- 保持所有现有头文件位置不变，避免破坏引用
- 后续 Phase 逐步迁移实现"
```

---

### Task 1.3: 整合现有 Volcano 算子到统一 executor/ 入口

**目标：** 当前标准算子（nodeSeqscan、nodeHashjoin 等）在 `src/db/sql/` 下，多模态算子（graph、rag）在 `src/db/executor/` 下。设计文档期望算子统一在 `executor/` 下。但考虑到大量现有测试引用 `#include "db/sql/nodeSeqscan.h"`，本次不做物理移动，而是在 executor/ 下建立**转发头文件**和**统一封装**。

**文件：**
- Create: `engineering/include/db/executor/executor.h` — 统一执行器入口（聚合现有 executor.h 功能）
- Create: `engineering/include/db/executor/nodeSeqscan.h` — 转发到 `db/sql/nodeSeqscan.h`
- Create: `engineering/include/db/executor/nodeHashjoin.h` — 转发
- Create: `engineering/include/db/executor/nodeAgg.h` — 转发
- 等（涵盖所有标准算子）
- Create: `engineering/include/db/executor/vector/vector_scan.h` — 向量算子接口（骨架）
- Create: `engineering/include/db/executor/graph/graph_scan.h` — 图算子接口（骨架）
- Modify: `engineering/src/db/executor/CMakeLists.txt` — 扩展 include 路径

**接口：**
- 消费：`db/sql/sql_executor.h` 中的 `PlanState`、`TupleTableSlot` 等类型
- 产出：统一 `executor.h` 入口，各算子转发头文件

- [ ] **Step 1: 创建统一 executor.h 入口**

```c
/**
 * @file executor.h
 * @brief 统一执行器入口
 *
 * 聚合所有执行器功能，包括标准算子（Volcano 模型）和多模态算子。
 * 标准算子实现位于 src/db/sql/ 下，多模态算子位于 src/db/executor/ 下。
 */
#ifndef DB_EXECUTOR_H
#define DB_EXECUTOR_H

#include "db/sql/executor.h"        /* 执行器入口（execMain 等价） */
#include "db/sql/sql_executor.h"     /* Volcono 执行器主接口 */
#include "db/sql/expr.h"             /* 表达式求值 */
#include "db/sql/execnodes.h"        /* 执行节点类型 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 执行器版本信息
 */
typedef struct {
    int major;           /**< 主版本号 */
    int minor;           /**< 次版本号 */
    const char *engine;  /**< 引擎名称 */
} executor_version_t;

/**
 * @brief 获取执行器版本
 */
executor_version_t executor_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_H */
```

- [ ] **Step 2: 创建标准算子转发头文件**

为每个标准算子创建转发头文件，例如 `include/db/executor/nodeSeqscan.h`：

```c
/**
 * @file nodeSeqscan.h
 * @brief 顺序扫描算子（转发到 db/sql/nodeSeqscan.h）
 */
#ifndef DB_EXECUTOR_NODE_SEQSCAN_H
#define DB_EXECUTOR_NODE_SEQSCAN_H

#include "db/sql/nodeSeqscan.h"

#endif /* DB_EXECUTOR_NODE_SEQSCAN_H */
```

需要创建的转发头文件列表：
- `nodeSeqscan.h`
- `nodeIndexscan.h`
- `nodeHashjoin.h`
- `nodeNestloop.h`
- `nodeAgg.h`
- `nodeHashagg.h`
- `nodeSort.h`
- `nodeLimit.h`
- `nodeModifyTable.h`
- `nodeResult.h`
- `nodeProjectSet.h`
- `nodeGather.h`
- `nodeHash.h`

- [ ] **Step 3: 创建多模态算子骨架接口**

创建向量算子接口骨架：

```c
/**
 * @file vector_scan.h
 * @brief 向量扫描算子接口
 *
 * 向量扫描算子直接调用 vector_engine，不走 Access Method 层。
 * 实现将在 Phase 2 完成。
 */
#ifndef DB_EXECUTOR_VECTOR_SCAN_H
#define DB_EXECUTOR_VECTOR_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 向量扫描状态 */
typedef struct VectorScanState_s {
    PlanState ps;            /**< 基类 PlanState */
    float    *query_vector;  /**< 查询向量 */
    int       top_k;         /**< Top-K 参数 */
    int       distance_metric; /**< 距离度量：0=L2, 1=IP, 2=COSINE */
    void     *index;         /**< 向量索引句柄 */
    int       ef;            /**< HNSW 搜索参数 */
    int       nprobe;        /**< IVF 搜索参数 */
} VectorScanState;

/**
 * @brief 初始化向量扫描
 */
VectorScanState *exec_vector_scan_init(PlanState *parent,
    float *query_vector, int top_k, int distance_metric);

/**
 * @brief 执行向量扫描，返回下一行
 */
TupleTableSlot *exec_vector_scan_next(VectorScanState *state);

/**
 * @brief 关闭向量扫描
 */
void exec_vector_scan_close(VectorScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_VECTOR_SCAN_H */
```

- [ ] **Step 4: 修改 executor/CMakeLists.txt 扩展 include 路径**

```cmake
# 在现有 target_include_directories 中添加
target_include_directories(db_executor PUBLIC
    ...
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/vector
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/graph
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/kv
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/ts
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/doc
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/spatial
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/datalog
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/yang
    ${ENGINEERING_SOURCE_DIR}/include/db/executor/stream
)
```

- [ ] **Step 5: 编译验证**

```bash
cd c:/code/book
cmake --build build/engineering 2>&1 | tail -15
```

- [ ] **Step 6: 运行现有测试确保没有破坏**

```bash
cd c:/code/book
ctest --test-dir build/engineering --output-on-failure -R "sql_executor|rag_executor|test_executor" 2>&1 | tail -30
```

- [ ] **Step 7: 提交**

```bash
git add -A
git commit -m "feat(db): 整合 Volcano 算子到统一 executor/ 入口

- 创建统一 executor.h 入口，聚合现有执行器功能
- 创建 13 个标准算子转发头文件（nodeSeqscan 等）
- 创建向量算子骨架接口（VectorScanState）
- 扩展 executor CMakeLists 的 include 路径
- 保持现有代码位置不变，通过转发头文件提供新入口"
```

---

### Task 1.4: 重构 CMake 库结构（sql_engine → db_executor 合并）

**目标：** 当前 `sql_engine` 库包含所有标准算子，`db_executor` 库包含多模态算子。设计文档期望统一在 executor/ 下。但不做大的物理移动，而是调整 CMake 依赖关系，使 `sql_engine` 成为 `db_executor` 的依赖。

**文件：**
- Modify: `engineering/src/db/executor/CMakeLists.txt` — 链接 sql_engine
- Modify: `engineering/src/db/CMakeLists.txt` — 子目录顺序调整

**接口：**
- 消费：`sql_engine` 库（已有）
- 产出：`db_executor` 库链接 `sql_engine`

- [ ] **Step 1: 修改 executor/CMakeLists.txt 链接 sql_engine**

```cmake
target_link_libraries(db_executor PRIVATE
    db_parser
    db_optimizer
    db_storage
    db_validator
    sql_engine     # 新增：链接标准算子库
)
```

- [ ] **Step 2: 编译验证**

```bash
cd c:/code/book
cmake -B build/engineering
cmake --build build/engineering --target db_executor 2>&1 | tail -5
```

- [ ] **Step 3: 运行测试验证**

```bash
cd c:/code/book
ctest --test-dir build/engineering -R "rag_executor" --output-on-failure 2>&1 | tail -10
```

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "refactor(db): executor 库链接 sql_engine 标准算子

- db_executor 新增依赖 sql_engine 库
- 为后续多模态算子复用标准算子类型做准备
- 所有现有测试通过"
```

---

### Task 1.5: Phase 1 集成测试

**目标：** 确保 Phase 1 的所有变更不破坏现有功能，运行完整的 SQL 执行器测试套件

- [ ] **Step 1: 全量编译**

```bash
cd c:/code/book
cmake -B build/engineering
cmake --build build/engineering -j 2>&1 | tail -20
```

- [ ] **Step 2: 运行全部 SQL 相关测试**

```bash
cd c:/code/book
ctest --test-dir build/engineering -R "sql_executor|test_executor|test_node|test_seqscan|test_hashjoin|rag_executor" --output-on-failure 2>&1 | tail -40
```

- [ ] **Step 3: 运行全部存储测试**

```bash
cd c:/code/book
ctest --test-dir build/engineering -R "storage|buf|catalog|heapam|btreeam|wal|txn" --output-on-failure 2>&1 | tail -40
```

- [ ] **Step 4: 如果任何测试失败，修复并重新运行**

- [ ] **Step 5: 提交最终状态**

```bash
git add -A
git commit -m "chore(db): Phase 1 架构落地完成

- engine/ 和 protocol/ 骨架目录建立
- 统一 executor/ 入口和转发头文件
- CMake 依赖关系调整
- 全部测试通过"
```

---

## Phase 2: 向量算子

**目标：** 实现向量扫描算子（VectorScan / HnswScan / IvfScan），直接调用现有 vector_engine 索引层，通过 SQL 扩展 `ORDER BY vec <-> query LIMIT K` 触发

**依赖：** Phase 1 完成（executor/ 统一入口、VectorScanState 骨架）

**涉及现有模块：**
- `include/db/index/vector_index/` — 向量索引 API（HNSW、IVF 等）
- `include/db/storage/vector/` — 向量存储引擎
- `src/db/index/vector_index/` — 向量索引实现
- `src/db/sql/planner.c` — 需要识别向量查询模式

### Task 2.1: 向量扫描算子实现

**文件：**
- Create: `engineering/src/db/executor/vector/vector_scan.c` — 向量扫描实现
- Create: `engineering/src/db/executor/vector/CMakeLists.txt` — 向量算子编译
- Modify: `engineering/include/db/executor/vector/vector_scan.h` — 完善接口
- Modify: `engineering/src/db/executor/CMakeLists.txt` — 添加 vector 子目录

**接口：**
- 消费：`VectorScanState`（Phase 1 定义）、`vector_index_api.h`（已有）
- 产出：`exec_vector_scan_init/next/close` 完整实现

### Task 2.2: 向量查询 SQL 扩展

**文件：**
- Modify: `engineering/src/db/sql/planner.c` — 识别向量排序模式
- Modify: `engineering/src/db/sql/sql_executor.c` — 向量查询路由

### Task 2.3: 向量算子测试

**文件：**
- Create: `engineering/test/db/executor/vector_scan_test.cpp`
- Modify: `engineering/src/db/executor/CMakeLists.txt` — 添加测试目标

---

## Phase 3: KV 算子 + RESP 协议

**目标：** 实现 KV 点查和范围扫描算子，同时实现 RESP（Redis 序列化协议）解析器，使数据库兼容 Redis 客户端

**依赖：** Phase 1 完成

**涉及现有模块：**
- `include/db/kv_engine.h` — KV 引擎接口
- `src/db/storage/kv/` — KV 存储引擎实现

### Task 3.1: KV 扫描算子

**文件：**
- Create: `engineering/include/db/executor/kv/kv_scan.h`
- Create: `engineering/src/db/executor/kv/kv_scan.c`
- Create: `engineering/src/db/executor/kv/CMakeLists.txt`

### Task 3.2: RESP 协议解析器

**文件：**
- Create: `engineering/include/db/protocol/resp.h`
- Create: `engineering/src/db/protocol/resp.c`
- Create: `engineering/src/db/protocol/CMakeLists.txt`

### Task 3.3: KV 算子 + RESP 集成测试

**文件：**
- Create: `engineering/test/db/executor/kv_scan_test.cpp`
- Create: `engineering/test/db/protocol/resp_test.cpp`

---

## Phase 4: 时序算子 + InfluxDB Line Protocol

**目标：** 实现时序范围扫描、窗口聚合和降采样算子，支持 InfluxDB Line Protocol 写入

**依赖：** Phase 1 完成

**涉及现有模块：**
- `include/db/ts_engine.h` — 时序引擎接口
- `src/db/storage/ts/` — 时序存储引擎实现（当前可能禁用）

### Task 4.1: 时序扫描和窗口算子

**文件：**
- Create: `engineering/include/db/executor/ts/ts_scan.h`
- Create: `engineering/include/db/executor/ts/ts_window.h`
- Create: `engineering/src/db/executor/ts/ts_scan.c`
- Create: `engineering/src/db/executor/ts/ts_window.c`
- Create: `engineering/src/db/executor/ts/CMakeLists.txt`

### Task 4.2: InfluxDB Line Protocol 写入

**文件：**
- Create: `engineering/include/db/protocol/influx_line.h`
- Create: `engineering/src/db/protocol/influx_line.c`

### Task 4.3: 时序引擎编译修复

**文件：**
- Modify: `engineering/src/db/storage/CMakeLists.txt` — 启用 ts 子目录

---

## Phase 5: 文档算子 + JSONPath

**目标：** 实现文档扫描、BM25 全文搜索和 JSONPath 过滤算子

**依赖：** Phase 1 完成

**涉及现有模块：**
- `include/db/doc_engine.h` — 文档引擎接口
- `src/db/storage/doc/` — 文档存储引擎实现

### Task 5.1: 文档扫描算子

**文件：**
- Create: `engineering/include/db/executor/doc/doc_scan.h`
- Create: `engineering/include/db/executor/doc/bm25_scan.h`
- Create: `engineering/src/db/executor/doc/doc_scan.c`
- Create: `engineering/src/db/executor/doc/bm25_scan.c`
- Create: `engineering/src/db/executor/doc/CMakeLists.txt`

### Task 5.2: JSONPath 过滤器

**文件：**
- Create: `engineering/src/db/executor/doc/jsonpath_filter.c`
- Create: `engineering/include/db/executor/doc/jsonpath_filter.h`

---

## Phase 6: 空间算子 + PostGIS 风格函数

**目标：** 实现空间范围查询、KNN 查询和空间连接算子，支持 PostGIS 风格的 ST_* 函数

**依赖：** Phase 1 完成

**涉及现有模块：**
- `include/db/spatial_engine.h` — 空间引擎接口
- `src/db/storage/spatial/` — 空间存储引擎实现（当前可能禁用）

### Task 6.1: 空间扫描算子

**文件：**
- Create: `engineering/include/db/executor/spatial/spatial_scan.h`
- Create: `engineering/include/db/executor/spatial/spatial_knn.h`
- Create: `engineering/src/db/executor/spatial/spatial_scan.c`
- Create: `engineering/src/db/executor/spatial/spatial_knn.c`
- Create: `engineering/src/db/executor/spatial/CMakeLists.txt`

### Task 6.2: 空间引擎编译修复

**文件：**
- Modify: `engineering/src/db/storage/CMakeLists.txt` — 启用 spatial 子目录

---

## Phase 7: 图算子 + GQL

**目标：** 实现图遍历、GQL 查询语言解析和最短路径算子

**依赖：** Phase 1 完成

**涉及现有模块：**
- `include/db/graph_engine.h` — 图引擎接口
- `include/db/parser/graph/gql.h` — GQL 解析器（已有）
- `src/db/executor/graph/gqlExec.c` — GQL 执行器（已有）
- `src/db/storage/graph/` — 图存储引擎实现

### Task 7.1: 图扫描算子统一

**文件：**
- Create: `engineering/include/db/executor/graph/graph_scan.h`
- Create: `engineering/src/db/executor/graph/graph_scan.c`
- Modify: 整合现有 `gqlExec.c` 和 `traverse.c` 至此

---

## Phase 8: Datalog 推理引擎

**目标：** 实现 Datalog 语言解析器、规则编译器和半朴素求值器

**依赖：** Phase 1 完成

### Task 8.1: Datalog 解析器

**文件：**
- Create: `engineering/include/db/protocol/datalog.h`
- Create: `engineering/src/db/protocol/datalog_parser.c`
- Create: `engineering/src/db/protocol/datalog_compiler.c`

### Task 8.2: Datalog 扫描算子

**文件：**
- Create: `engineering/include/db/executor/datalog/datalog_scan.h`
- Create: `engineering/src/db/executor/datalog/datalog_scan.c`
- Create: `engineering/src/db/executor/datalog/CMakeLists.txt`

---

## Phase 9: Yang 协议适配

**目标：** 实现 NETCONF/RESTCONF 协议和 Yang Path 路径解析，操作树引擎

**依赖：** Phase 1 完成

**涉及现有模块：**
- `include/db/yang_engine.h` — Yang 引擎接口
- `src/db/storage/yang/` — Yang 存储引擎实现（当前可能禁用）

### Task 9.1: Yang 路径扫描算子

**文件：**
- Create: `engineering/include/db/executor/yang/yang_scan.h`
- Create: `engineering/src/db/executor/yang/yang_scan.c`
- Create: `engineering/src/db/executor/yang/CMakeLists.txt`

### Task 9.2: Yang 引擎编译修复

**文件：**
- Modify: `engineering/src/db/storage/CMakeLists.txt` — 启用 yang 子目录

---

## Phase 10: 流处理引擎 + Kafka 协议

**目标：** 实现流表（Stream Table）、物化视图持续聚合、窗口计算和 Kafka 协议接入

**依赖：** Phase 1 完成

### Task 10.1: 流表注册和元数据管理

**文件：**
- Create: `engineering/include/db/engine/stream_engine.h`
- Create: `engineering/src/db/engine/stream_engine.c` — Stream Registry

### Task 10.2: 流算子实现

**文件：**
- Create: `engineering/include/db/executor/stream/stream_scan.h`
- Create: `engineering/include/db/executor/stream/stream_window.h`
- Create: `engineering/include/db/executor/stream/stream_join.h`
- Create: `engineering/include/db/executor/stream/stream_agg.h`
- Create: `engineering/src/db/executor/stream/stream_scan.c`
- Create: `engineering/src/db/executor/stream/stream_window.c`
- Create: `engineering/src/db/executor/stream/stream_join.c`
- Create: `engineering/src/db/executor/stream/stream_agg.c`
- Create: `engineering/src/db/executor/stream/CMakeLists.txt`

### Task 10.3: Kafka 协议适配

**文件：**
- Create: `engineering/include/db/protocol/kafka.h`
- Create: `engineering/src/db/protocol/kafka_connector.c`

---

## 优先级与依赖关系

```
Phase 1 (架构落地)
  ├── Phase 2 (向量算子) — 依赖 Phase 1
  ├── Phase 3 (KV + RESP) — 依赖 Phase 1
  ├── Phase 4 (时序 + InfluxDB) — 依赖 Phase 1
  ├── Phase 5 (文档 + JSONPath) — 依赖 Phase 1
  ├── Phase 6 (空间 + PostGIS) — 依赖 Phase 1
  ├── Phase 7 (图 + GQL) — 依赖 Phase 1，利用现有 gqlExec
  ├── Phase 8 (Datalog) — 依赖 Phase 1
  ├── Phase 9 (Yang) — 依赖 Phase 1
  └── Phase 10 (流 + Kafka) — 依赖 Phase 1
```

各 Phase 2-10 之间无交叉依赖，可以并行实现。Phase 1 是所有 Phase 的基石。

---

## 验证方式

每个 Phase 的验证方式：
1. **编译验证**：`cmake --build build/engineering` 编译通过
2. **单元测试**：每 Task 对应的 gtest 测试通过
3. **集成测试**：Phase 相关的集成测试套件通过
4. **回归测试**：`ctest --test-dir build/engineering` 全部测试通过