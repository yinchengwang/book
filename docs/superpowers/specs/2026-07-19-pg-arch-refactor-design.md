# 数据库按 PG 架构全面重建 — 设计文档

> 修复 SQL 引擎入口断裂、多模态算子孤立、CLI/SDK 分层缺失

## 1. 概述

### 1.1 目标

按 PostgreSQL 风格重新组织 `engineering/src/db/` 下的 SQL 引擎，端到端贯通 **解析 → 语义分析 → 计划 → 优化 → 执行 → 存储** 链路，并补齐 **客户端/SDK/协议** 三层。

### 1.2 问题陈述

通过代码事实核实，数据库当前存在 4 个核心架构问题：

1. **SQL 管线入口断裂**：parser/sql 套解析出的 sql_node_t* 没有生产路径接收；sql/ 套的 sql_driver.c 是死代码；唯一跑通的 SQL 路径是 sql_ddl.c，但它是手写解析 DDL，绕开了所有 AST 抽象
2. **多模态算子孤立**：executor/{vector,kv,spatial,graph}/*_scan.c 是手写的 init/next/close 模板（6 个文件 ~600 行重复），未接入 sql_executor 注册表，只被 gtest 测试调用
3. **graph traverse 三套并存**：graph/graph_algo.c（核心算法）+ storage/graph/graph_traverse.c（存储适配）+ executor/graph/traverse.c（混合体）。其中 traverse.c 的 graph_bfs/dfs/dijkstra/pagerank 是死代码
4. **CLI/SDK 分层缺失**：vdb_cli 直接链接 vector_api 内部库；src/db/cli/cli.c 是 PG 风格的 CLI 设计但 CMake 注释了；sdk/ 下只有 python SDK，没有 C SDK

### 1.3 设计原则

1. **PG 架构对标**：分层严格对应 PG（parser → analyze → planner → optimizer → executor → access method → storage）
2. **现有代码优先**：保留所有活跃模块（catalog/rel/parser/sql 套），仅删除死代码
3. **增量贯通**：每个阶段编译+测试通过后再进下一阶段
4. **SDK 分层**：vdb_cli/Python SDK/apps 统一通过 db_sdk 调用，禁止直链内部库

## 2. 目标架构

### 2.1 分层架构图

```
┌────────────────── 客户端层 ──────────────────┐
│  vdb_cli  psql  daily-digest 后端             │
│  (C/CLI)    (PG 客户端)  (Python)            │
└────────────┬─────────────────────────────────┘
             ↓
┌────────────────── SDK 层 ────────────────────┐
│  sdk/c/include/db_sdk.h   (新增 C SDK)        │
│  sdk/python/minivecdb/   (现有 Python SDK)    │
└────────────┬─────────────────────────────────┘
             ↓
┌────────────────── 协议层 ────────────────────┐
│  src/db/sql/pgwire.c      (恢复 + Win 兼容)   │
│  src/db/core/db_server.c  (保留并简化)        │
└────────────┬─────────────────────────────────┘
             ↓
┌────────────────── SQL 引擎主路径 ─────────────┐
│  src/db/parser/sql/      (解析，保留)         │
│      ↓ sql_node_t*                           │
│  parse_analyze            (语义分析)          │
│      ↓ Query tree                            │
│  src/db/sql/planner.c    (逻辑计划)          │
│      ↓ Plan                                  │
│  src/db/sql/optimizer.c  (代价+重写)         │
│      ↓ Optimized Plan                        │
│  src/db/sql/executor.c   (注册表 + 调度)     │
│      ↓                                       │
│  ┌─────────── 13 个核心算子 ────────────┐     │
│  │  nodeSeqScan/IndexScan/HashJoin     │     │
│  │  nodeHash/Agg/HashAgg/Sort/Limit    │     │
│  │  nodeModifyTable/ProjectSet/Result  │     │
│  │  nodeGather/Nestloop                │     │
│  └─────────────────────────────────────┘     │
│      ↓                                       │
│  ┌─────────── 多模态算子（新增接入）───────┐    │
│  │  executor/vector/*_scan.c            │    │
│  │  executor/kv/*_scan.c                │    │
│  │  executor/spatial/*_scan.c           │    │
│  │  executor/graph/traverse/traverser   │    │
│  └─────────────────────────────────────┘     │
└────────────┬─────────────────────────────────┘
             ↓
┌────────────────── 存储抽象层 ─────────────────┐
│  catalog (pg_class/pg_attribute)              │
│  rel (RelationData 句柄 + 扫描器)             │
│  access method (heap/btree)                  │
│  storage_ops_t 接口                          │
└────────────┬─────────────────────────────────┘
             ↓
┌────────────────── 存储引擎层 ─────────────────┐
│  storage/heap   storage/btree                │
│  storage/kv     storage/vector               │
│  storage/graph  storage/spatial              │
│  storage/yang   storage/ts (按需)            │
└──────────────────────────────────────────────┘
```

### 2.2 与现状的对比

| 层 | 现状 | 目标 |
|---|---|---|
| 客户端 | vdb_cli 直链 vector_api | vdb_cli 走 db_sdk |
| SDK | 无 C SDK | 新增 sdk/c/ |
| 协议 | sql/pgwire.c 注释，core/db_server.c 简化版 | 恢复 sql/pgwire.c（Win 兼容），db_server 委托 pgwire |
| SQL 入口 | cli.c 死了 / sql_driver.c 死了 | 恢复 cli.c 并贯通 parser→planner→executor |
| 解析 | parser/sql/ 套（活跃） + sql/sql_parser.c（死） | 保留 parser/sql/，删除 sql/sql_parser.c |
| 计划/优化 | sql/{planner, optimizer}.c 已实现但无入口 | 接入 cli.c 入口 |
| 执行器 | 13 个 nodeXxx 注册表驱动；executor/{vector,kv,...} 孤立 | 把孤立算子接入注册表 |
| 多模态算子 | 6 个 *_scan.c 手写模板重复 | 提取 EXEC_SCAN_DEFINE_TEMPLATE 宏 |
| 存储抽象 | catalog↔rel 分层合理；table.c 独立栈 | table.c 改为 catalog wrapper |
| graph 遍历 | 3 套并存（其中 traverse.c 部分死） | 收敛到 graph_algo.c + storage/graph_traverse.c |

## 3. 模块决策

### 3.1 parser vs sql 收敛决策

| 模块 | 决策 | 理由 |
|---|---|---|
| `src/db/parser/sql/*`（3305 行） | ✅ 保留 | 活跃，12+ 调用方，sql_node_t* 是契约 |
| `src/db/sql/sql_parser.c`（935 行） | ❌ 删除 | 死代码，与 parser/sql/sql_parser.c 同名同规模但无调用方 |
| `src/db/sql/sql_driver.c` | ❌ 删除 | 死代码，sql/ 套入口 |
| `src/db/sql/planner.c` | ✅ 保留 | Volcano 计划器，必须保留 |
| `src/db/sql/optimizer.c` | ✅ 保留 | 优化器 |
| `src/db/sql/executor.c` + `executor_register_node` | ✅ 保留 | 注册表驱动，13 个核心算子 |
| `src/db/sql/nodeXxx.c` (13 个) | ✅ 保留 | 核心算子 |
| `src/db/sql/sql_ddl.c` | ✅ 保留 | 唯一活跃的 SQL 路径，DDL 入口 |
| `src/db/sql/memctx.c` | ✅ 保留 | 内存上下文，planner/executor 依赖 |
| `src/db/sql/expr.c` + `expr_interp.c` | ✅ 保留 | 表达式求值 |
| `src/db/sql/cost.c` | ✅ 保留 | 代价估算 |
| `src/db/sql/jit.c` | ✅ 保留 | JIT 编译 |
| `src/db/sql/trigger.c` + `trigger_functions.c` | ✅ 保留 | 触发器 |
| `src/db/sql/partition.c` | ✅ 保留 | 分区路由 |
| `src/db/sql/tuple_queue.c` + `worker_pool.c` + `parallel.c` | ✅ 保留 | 并行执行 |

### 3.2 catalog/rel/table 收敛

| 模块 | 决策 |
|---|---|
| `src/db/storage/catalog/catalog.c`（810 行） | ✅ 保留，作为系统目录权威源 |
| `src/db/storage/rel/rel.c` | ✅ 保留，Relation 包装层（已正确调用 catalog） |
| `src/db/storage/rel/table.c` | 🔄 **重写为 catalog 薄包装** |
| `src/db/storage/mm/mm_storage.c` | ✅ 保留，多模态抽象 |
| `include/db/{catalog,rel,table}.h` | 🔄 合并 table.h 接口到 catalog.h（或保留独立头但实现收敛） |

**table.c 重构方案**：
- 删除 `table.c` 中自己维护的 JSON 元数据解析（约 100 行）
- `table_open(kv_t*, name)` 改为：
  1. 调 `catalog_lookup_table(name)` 拿到 OID
  2. 调 `catalog_get_table(OID)` 拿 table_info_t
  3. 包装成 table_t（保留向后兼容）
- `table_create` 改为调 `catalog_create_table`
- 验证 `validator/sql_semantic.c` 和 `validator/semantic_analyzer.c` 通过

### 3.3 executor 多模态算子接入（注册表插件化设计原则）

**核心原则**：所有算子（含关系算子 + 多模态算子）通过 `executor_register_node()` **统一注册**。新增数据模型只需实现 `(Plan, EState) → PlanState` 风格的 init 函数 + 一行注册即可，不修改执行器核心。

**现状验证**：`include/db/sql/sql_executor.h::ExecutorType` 枚举已预留 `EXEC_VECTOR_SCAN`、`EXEC_HNSW_SCAN`、`EXEC_IVF_SCAN`、`EXEC_BM25_SCAN`、`EXEC_TIMESERIES_SCAN`、`EXEC_GRAPH_SCAN`、`EXEC_DOCUMENT_SCAN`、`EXEC_SPATIAL_SCAN` 等 8 个多模态算子槽位——**注册表接口已为多模态扩展设计好了**。

**重构方式**：提取 `EXEC_SCAN_DEFINE_TEMPLATE(NAME, ENGINE_TYPE, RESULTS_TYPE, SEARCH_FN)` 宏，把每个文件从 ~124 行模板压缩到 ~20 行差异部分。

**新增注册表项**（在 `executor_register_nodes()` 末尾）：
```c
executor_register_node(T_VectorScan, (ExecInitNodeFn)ExecInitVectorScan);
executor_register_node(T_HnswScan, (ExecInitNodeFn)ExecInitHnswScan);
executor_register_node(T_IvfScan, (ExecInitNodeFn)ExecInitIvfScan);
executor_register_node(T_KvScan, (ExecInitNodeFn)ExecInitKvScan);
executor_register_node(T_SpatialScan, (ExecInitNodeFn)ExecInitSpatialScan);
executor_register_node(T_SpatialKnn, (ExecInitNodeFn)ExecInitSpatialKnn);
executor_register_node(T_GraphScan, (ExecInitNodeFn)ExecInitGraphScan);
```

**新增 ExecutorType**（在 `include/db/sql/sql_executor.h`）：
```c
EXEC_VECTOR_SCAN,    /**< 向量暴力搜索 */
EXEC_HNSW_SCAN,      /**< HNSW 索引搜索 */
EXEC_IVF_SCAN,       /**< IVF-PQ 索引搜索 */
EXEC_KV_SCAN,        /**< KV 范围扫描 */
EXEC_SPATIAL_SCAN,   /**< 空间范围查询 */
EXEC_SPATIAL_KNN,    /**< 空间 KNN */
EXEC_GRAPH_SCAN,     /**< 图遍历 */
```

**澄清记录（基于代码事实）**：

- `parser/sql/sql_parse_one` 产出 `sql_node_t*`
- `planner_logical_plan` 已经支持 `sql_node_t*` 作为输入（自动类型判断：如果是 SQL_NODE_* 走简化路径，如果是 T_SelectStmt 走 Bison 路径）
- **`parse_analyze.c` 是 Bison 套的遗产，无任何调用方**，是死代码——不需要"修复兼容"，**直接删除**
- 端到端贯通的真实路径：`sql_parse_one → planner_plan → PhysPlan → PlanState → ExecutorRun → 元组`

### 3.3.1 优化器双路径（planner.c vs optimizer.c）→ 重写为单一优化器

**事实澄清（基于代码）**：

| 优化规则 | planner.c::apply_rule_recursive | optimizer.c::opt_xxx |
|---|---|---|
| 谓词下推 | ✅ 真实现 | ⚠ TODO 占位 |
| 常量折叠 | ⚠ 简化（仅检测） | ⚠ TODO 占位 |
| 列裁剪 | ⚠ 简化占位 | ⚠ TODO 占位 |
| 子查询扁平化 | ⚠ 简化占位 | ⚠ TODO 占位 |
| 连接重排序 | ⚠ 简化占位 | ⚠ TODO 占位 |
| 投影下推 | ❌ 未实现 | ⚠ TODO 占位 |
| 聚合下推 | ❌ 未实现 | ⚠ TODO 占位 |
| 排序消除 | ❌ 未实现 | ⚠ TODO 占位 |

**结论**：两边都是半成品。重写为单一 PG 风格优化器，一次遍历应用所有规则。

**接口设计（基于澄清）**：
- 输入：`LogicalPlan *`（与 `planner_logical_plan` 兼容）
- 输出：优化后的 `LogicalPlan *`
- 调用入口：在 `planner_plan` 中，`planner_logical_plan` 之后、`planner_physical_plan` 之前调用
- 实现参考：PG `planner.c` 的 `subquery_planner` + `optimize_plan` 模式

### 3.3.2 执行层双路径（exec_proc vs ExecProcNode）→ 收敛到 B（Volcano）

**事实澄清（基于代码）**：

| 路径 | 入口 | 执行函数 | 状态 |
|---|---|---|---|
| **A (旧)** | `planner_create_plan_state` 设置 `state->exec_proc = get_exec_func(plan->type)` | `sql_executor.c` 中 12 个 `exec_xxx` 函数（exec_seq_scan, exec_hashjoin, exec_sort, exec_limit, exec_result, exec_hash_agg, exec_sort_agg, exec_insert, exec_update, exec_delete, exec_vector_scan, exec_hnsw_scan） | ✅ 完整跑通（**直接调存储层，未走 catalog**） |
| **B (新)** | `ExecInitNode` → 注册表 → `ExecInitXxx` → `state->ExecProcNode(state)` | `nodeXxx.c` 中 `ExecSeqScan` 等（同名但实现不同，**走 catalog**） | ⚠ 只有 Result 完整跑通，其余 Init 返回 NULL |

**目标**：删除 A 路径所有 `exec_xxx` 函数（sql_executor.c）以及 `get_exec_func` switch case，修改 `planner_create_plan_state` 走 `ExecutorStart + ExecInitNode + 注册表` 路径。补全 B 路径 13 个算子的 `ExecInitXxx` 实际逻辑，让 `ExecInitNode` 返回非 NULL。

### 3.3.3 执行引擎到存储引擎的对接

**事实澄清（基于代码）**：

**A. 多模态存储引擎注册现状**：
- ✅ `MODEL_KV` 已注册（`kv_engine_get_ops`）
- ✅ `MODEL_VECTOR` 已注册（`vector_engine_get_ops`）
- ❌ `MODEL_GRAPH/SPATIAL/DOCUMENT/TIMESERIES/TREE` 5 个未注册

**B. 一个引擎内多个索引并存**：
- `vector_engine_db_t` 已经预留多个索引字段（`hnsw_index`、`ivf_pq_index`、`quantizer`、`page_pool`、`wal`）
- 但调度逻辑不完整：
  - `vector_engine_search` 默认走暴力搜索（page_pool + L2 距离）
  - `vector_engine_search_hnsw` / `vector_engine_search_ivf_pq` 是独立入口，调用方需显式选
  - `vector_index_selector` 选完索引后还是 TODO，没真正构建 IVF-PQ 等

**目标决策**：
1. **5 个未注册引擎补全**：`graph_engine` / `spatial_engine` / `doc_engine` / `ts_engine` / `yang_engine` 全部实现 `storage_ops_t` 并注册到 `storage_register_engine()`
2. **编译时隔离（FEATURE_SQL / FEATURE_KV 宏）**：每个引擎独有的代码用编译宏隔离
   - `FEATURE_SQL`：启用 SQL 路径（catalog + rel + heap AM）
   - `FEATURE_KV`：启用 KV 引擎路径
   - `FEATURE_VECTOR` / `FEATURE_GRAPH` / `FEATURE_SPATIAL`：启用各多模态引擎
   - 多模态算子（`executor/vector/*`）启用条件：`#ifdef FEATURE_VECTOR`
   - 默认开启 FEATURE_SQL + FEATURE_KV + FEATURE_VECTOR，其他按需开启
3. **多索引选择策略**：在 `vector_engine_db_t` 中加 `active_index_type` 字段（`VECTOR_INDEX_HNSW` / `VECTOR_INDEX_IVF_PQ` / `VECTOR_INDEX_DISKANN`），`vector_engine_search` 根据该字段分派到不同索引

### 3.3.4 PG 风格代价估算与索引选择

**事实澄清（基于代码）**：

| PG 组件 | 你的项目 | 状态 |
|---|---|---|
| `pg_class.relpages/reltuples` | ✅ `table_info_t.npages/ntupes` | ✅ 已预留字段 |
| `stats_collector` 后台进程 | ✅ `StatsCollector_s` + `task_stats.c` | ✅ 完整实现 |
| `pg_stat_*` 视图 | ✅ `stat_views.c` 提供 `stat_user_tables` | ✅ 完整实现 |
| `ANALYZE` 命令 | ⚠ `analyze_execute` 是 TODO 占位 | ⚠ 简化版 |
| **planner 调用 stats** | ❌ planner.c 完全不读 stats | ❌ **核心缺口** |
| **catalog 维护 stats** | ❌ catalog.c 不更新统计 | ❌ **核心缺口** |
| **planner 用真实统计** | ❌ planner 用 `rows * width / 8192.0` 固定公式 | ❌ **核心缺口** |

**目标决策**：
1. **实现 PG 完整代价估算**：
   - 补全 `analyze_execute` 实现（全表采样 → 更新 `pg_statistic`）
   - planner 的 `cost_seqscan` / `cost_index` 从 `table_info_t.npages/ntupes` 拿真实数据
   - planner 调 `StatsCollector` 拿 idx_scan/seq_scan 计数
2. **索引选择策略**：代价驱动 + USE INDEX 提示
   - 优化器在多个索引中选代价最低
   - SQL 支持 `SELECT * FROM t USE INDEX (idx_name)` 强制指定索引

### 3.3.5 PG 风格 AM 注册 + 索引创建/重建

**事实澄清（基于代码）**：

| 组件 | 你的项目 | PG 对应 |
|---|---|---|
| 索引 AM 注册表 | ❌ 不存在 | `IndexAmRoutine` 函数指针表 + `amregistry.h` |
| `storage_ops_t.index_create` | ✅ 存在但引擎各自实现 | 统一入口 |
| 空表建索引 | ❌ `btbuild` 拒绝 `ntuples=0`，`vector_engine_build_index` 拒绝 `num_vectors=0` | ✅ 支持（建空索引，INSERT 增量维护） |
| 表里有数据建索引 | ✅ `btbuild` 循环 btinsert | ✅ 全表扫描 → bulk build |
| `CREATE INDEX CONCURRENTLY` | ⚠ `TOKEN_CONCURRENTLY` 已定义，`ReindexOptions.concurrently` 已定义，但实现是 TODO | ✅ 不阻塞写 |
| `REINDEX` | ⚠ `reindex_index/table/database/system` API 已定义，但内部 TODO | ✅ 原子切换 |
| 多索引并存 | ⚠ vector_engine_db_t 预留多索引字段，但调度未串 | ✅ 一个表多个索引 |

**目标决策**：
1. **AM 注册机制**：引入 `IndexAmRoutine` 函数指针表（参考 PG 头文件 `access/amapi.h`）
   ```c
   typedef struct IndexAmRoutine_s {
       const char *am_name;                    // "btree" / "hnsw" / "ivf_pq" / ...
       int (*ambuild)(Relation heap, IndexInfo *index, ...);  // 空表/批量构建
       int (*aminsert)(Relation index, Datum *values, ...);    // 单行插入
       int (*amdelete)(Relation index, Datum *values, ...);    // 单行删除
       IndexScanDesc (*ambeginscan)(...);       // 扫描接口
       void (*amendscan)(IndexScanDesc scan);
       bool (*amrescan)(...);
   } IndexAmRoutine;
   ```
2. **三个创建场景支持**：
   - **空表建索引**：`ambuild` 接受 `ntuples=0`，建空索引骨架
   - **有数据建索引**：`ambuild` 接受 `ntuples>0`，全表扫描 bulk build
   - **在线重建（CONCURRENTLY）**：通过 snapshot + 写时增量维护实现（参考 PG `REINDEX CONCURRENTLY` 算法）
3. **btbuild 修复**：`btbuild` 接受 `ntuples>=0`，空表时只创建元数据
4. **vector_engine_build_index 修复**：空集合时建空 HNSW 结构，后续 INSERT 时增量维护

### 3.4 graph traverse 单一化

| 模块 | 决策 |
|---|---|
| `src/db/graph/graph_algo.c`（528 行） | ✅ 保留，核心算法（Dijkstra/PageRank/迭代器） |
| `src/db/storage/graph/graph_traverse.c` | ✅ 保留，存储适配 + SQL 函数包装 |
| `src/db/executor/graph/traverse.c` | 🔄 删除 graph_bfs/dfs/dijkstra/pagerank（死代码，~400 行） |
| `src/db/executor/graph/traverse.c` 的 `graph_traverser_*` | ✅ 保留，接入 executor 注册表 |
| `src/db/storage/graph/graph_engine.c` | ✅ 保留，graph 存储引擎入口 |

### 3.5 bit hack 宏化

提取到 `include/db/executor/tuple_table_slot.h`：
```c
#define TTS_VALUES_SET_INT(slot, idx, val) \
    do { (slot)->tts_values[(idx)] = (void *)(uintptr_t)(int64_t)(val); } while(0)

#define TTS_VALUES_SET_FLOAT(slot, idx, val) \
    do { \
        float _v = (val); \
        (slot)->tts_values[(idx)] = (void *)(uintptr_t)(*(uint64_t *)&_v); \
    } while(0)

#define TTS_VALUES_SET_DOUBLE(slot, idx, val) \
    do { \
        double _v = (val); \
        (slot)->tts_values[(idx)] = (void *)(uintptr_t)(*(uint64_t *)&_v); \
    } while(0)
```

**清理 11 处直接位 hack**（vector_scan.c、hnsw_scan.c、ivf_scan.c、spatial_scan.c、spatial_knn.c 各 1-4 处）。

### 3.6 pgwire 协议恢复

**当前状态**：
- `src/db/sql/pgwire.c`（1292 行）实现了完整 pgwire 协议
- CMakeLists.txt 注释掉了（"使用 Unix 特有头文件，在 Windows 上不编译"）

**目标**：
1. 修改 pgwire.c 加 Windows 兼容：
   - `<sys/socket.h>` → `<winsock2.h>`
   - `read/write/close` → `recv/closesocket`
   - 加 `#ifdef _WIN32` 分支
2. 从 CMakeLists.txt 取消注释，加入 `sql_engine` 库
3. 重写 `src/db/core/db_server.c` 委托给 `pgwire.c`，不再维护两套

### 3.7 SDK 层新增

**新增目录**：`sdk/c/`（与 sdk/python/ 并列）

**API 风格对标 libpq**：
```c
// sdk/c/include/db_sdk.h
typedef struct db_conn_s db_conn_t;
typedef struct db_result_s db_result_t;

db_conn_t *db_connect(const char *conninfo);   // "host=localhost port=5432 dbname=test"
void db_close(db_conn_t *conn);

db_result_t *db_exec(db_conn_t *conn, const char *sql);
int db_result_status(const db_result_t *res);
const char *db_result_error(const db_result_t *res);
int db_result_rows(const db_result_t *res);
int db_result_cols(const db_result_t *res);
const char *db_result_column_name(const db_result_t *res, int col);
const char *db_result_value(const db_result_t *res, int row, int col, int *out_len);

void db_result_free(db_result_t *res);
```

**实现**：`sdk/c/db_sdk.c` 通过调用 `parser/sql/sql_parse_one` + `sql/planner` + `sql/executor` 走完整 SQL 路径（不走网络，仅本地嵌入式调用）。

### 3.8 CLI 重构

| 模块 | 决策 |
|---|---|
| `src/db/cli/cli.c` | 🔄 重写：让 cli 走完整 SQL 管线（parser→planner→executor→catalog→rel），不再直链 vector_api |
| `apps/vdb_cli/main.c + vdb_cli.c` | 🔄 重写：从直链 vector_api 改为链 db_sdk |

## 4. 实施阶段

### Phase 1：清理死代码（1 周）

**目标**：删除明确无用的代码，每个删除后立即验证编译

任务清单：
1. 删除 `src/db/sql/sql_parser.c`（935 行）— 同时从 CMakeLists.txt 移除
2. 删除 `src/db/sql/sql_driver.c` — 同时从 CMakeLists.txt 移除
3. 删除 `src/db/executor/graph/traverse.c` 中的 `graph_bfs`、`graph_dfs`、`graph_dijkstra`、`graph_pagerank`、`graph_shortest_path` 函数（~400 行）
4. 验证：cmake 重新配置 + ninja -j4 全量编译 + 测试通过

### Phase 2：executor 模板宏 + bit hack 宏（1 周）

**目标**：提取公用宏，6 个 scan 文件模板化

任务清单：
1. 在 `include/db/executor/tuple_table_slot.h` 添加 `TTS_VALUES_SET_*` 宏
2. 替换 vector_scan.c、hnsw_scan.c、ivf_scan.c、spatial_scan.c、spatial_knn.c 中的 11 处位 hack
3. 在 `include/db/executor/scan_template.h` 添加 `EXEC_SCAN_DEFINE_TEMPLATE` 宏
4. 用宏重写 6 个 scan 文件，预计从 ~750 行压缩到 ~200 行
5. 验证所有相关 gtest 通过

### Phase 3：catalog/table 收敛（1-2 周）

**目标**：table.c 改为 catalog 薄包装

任务清单：
1. 修改 `src/db/storage/rel/table.c`，删除 JSON 元数据解析（约 100 行）
2. table_open → catalog_lookup_table + catalog_get_table
3. table_create → catalog_create_table
4. 修改 `include/db/table.h` 接口（保持向后兼容）
5. 验证 `validator/sql_semantic.c`、`validator/semantic_analyzer.c` 通过
6. 验证 backup_test.cpp（依赖 table API）通过

### Phase 4：SQL 管线贯通（2-3 周）

**目标**：parser → planner → executor → catalog → rel 完整链路跑通

任务清单：
1. 恢复 `src/db/sql/pgwire.c`（添加 Windows 兼容）
2. 取消 CMakeLists.txt 中 pgwire.c 的注释
3. 重写 `src/db/cli/cli.c`：让 cli 走完整 SQL 管线
4. 在 `sql/sql_executor.c` 中注册多模态算子（VectorScan/HnswScan/IvfScan/KvScan/SpatialScan/SpatialKnn/GraphScan）
5. 加端到端测试：
   - CREATE TABLE + INSERT + SELECT 全流程
   - 多表 JOIN
   - 向量索引 SQL 触发搜索
   - 图遍历 SQL 触发
6. 验证所有现有测试通过

### Phase 5：SDK 层 + vdb_cli 改写（1-2 周）

**目标**：SDK 分层，vdb_cli 走 SDK

任务清单：
1. 新增 `sdk/c/include/db_sdk.h`
2. 新增 `sdk/c/db_sdk.c`，实现 libpq 风格 API
3. 修改 `apps/vdb_cli/vdb_cli.c`，从直链 vector_api 改为链 db_sdk
4. 修改 `sdk/python/minivecdb/`，让 Python SDK 通过 ctypes 调用 db_sdk
5. 验证编译通过 + vdb_cli 跑通

## 5. 验证标准

### 5.1 编译验证

- `cmake -S /c/code/book/engineering -B /c/code/book/build/engineering` 成功
- `ninja -j4` 全量编译成功（137 个可执行文件）
- 无编译错误（允许警告）

### 5.2 测试验证

- 所有现有测试通过
- 新增端到端 SQL 测试覆盖：
  ```sql
  CREATE TABLE users (id INT, name TEXT);
  INSERT INTO users VALUES (1, 'alice'), (2, 'bob');
  SELECT * FROM users WHERE id = 1;
  -- JOIN
  CREATE TABLE orders (user_id INT, amount INT);
  INSERT INTO orders VALUES (1, 100), (2, 200);
  SELECT u.name, o.amount FROM users u JOIN orders o ON u.id = o.user_id;
  -- 向量
  SELECT * FROM vec_table ORDER BY distance <-> '[1.0, 2.0, 3.0]' LIMIT 10;
  ```

### 5.3 SDK 验证

- `vdb_cli` 通过 db_sdk 跑通完整 SQL
- Python SDK 通过 ctypes 调用 db_sdk 跑通

### 5.4 代码量验证

- Phase 1 完成后：`ninja` 编译产物 ~137 个可执行文件
- Phase 2 完成后：executor/{vector,kv,spatial}/*.c 总行数从 ~570 减少到 ~200
- Phase 3 完成后：table.c 从 ~600 行减少到 ~150 行
- Phase 4 完成后：新增端到端测试 ≥ 5 个
- Phase 5 完成后：vdb_cli 不再链接 vector_api

## 6. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| pgwire.c Windows 兼容改动引入新 bug | server 启动失败 | 保留 db_server.c 作为回退；先在 Linux 编译验证，Windows 用 ifdef 分支 |
| executor 多模态算子接入打破现有测试 | gtest 失败 | Phase 2 先做模板宏，不接入注册表；Phase 4 才接入 |
| table.c 重构影响 validator 调用 | 编译失败 | 保持 table.h 接口不变，只改实现 |
| vdb_cli 改用 SDK 性能下降 | 启动慢 | SDK 设计为本地嵌入式调用，零网络开销 |
| 多模态算子接入 sql_engine 后类型冲突 | 编译失败 | 在 sql_executor.h 中提前声明 ExecutorType enum |

## 7. 不在范围内

- 不重写 parser（保留 parser/sql/ 手写简化版）
- 不实现完整 Bison 解析器（gram.y/scan.l 文件保留但暂不构建）
- 不改 mm_storage 多模态抽象
- 不改 storage/{vector,graph,...} 各引擎内部实现
- 不改 WAL/Buffer Pool/Catalog 存储细节

## 8. 后续可优化（V2）

1. 把 `parser/sql/` 套与 Bison 套（gram.y/scan.l）合并，提供完整 SQL 语法
2. 实现完整的 `psql` 客户端（通过 pgwire 协议连接 server）
3. 把 `db_sdk` 暴露给其他语言（Rust bindings、Go bindings）
4. 增加分布式事务支持
5. 增加向量/图索引的 SQL 语法扩展

## 9. 总优化方案（按 PG 完备性维度）

**与 PG 完备性差距（基于代码事实量化）**：

| PG 完备性维度 | 当前实装度 | 主要差距 |
|---|---|---|
| SQL 语法完备性 | ~10% | 缺 CTE、窗口、复杂 JOIN、子查询、UNION、CASE |
| 解析→计划→执行→存储链路 | ~40% | 13 个 ExecInitXxx 只有 Result 完整跑通 |
| 优化器实装深度 | ~5% | 5 条规则全是简化版或 TODO |
| 代价估算与统计 | ~5% | planner 用固定公式，不读真实统计 |
| MVCC + 事务 | MVCC 30%, 事务 0% | txn/ 目录 CMake 注释 |
| 索引 AM 框架 | ~5% | 无 IndexAmRoutine 注册表 |
| 多模态接入 | 2/7 | graph/spatial/doc/ts/yang 未注册 storage_ops_t |
| pgwire + CLI + SDK | ~40% | pgwire.c 注释，cli.c 死了 |

**多模态优势**（差异化）：~80%（vector/graph/timeseries/document/spatial/yang 自研）

**总优化方案（按 Phase 排序）**：

| Phase | 内容 | 周期 | 依赖 |
|---|---|---|---|
| Phase 0 | 删除死代码（parse_analyze.c 等） | 1 周 | — |
| Phase 1 | 解析→计划→执行→存储端到端贯通 | 6-8 周 | Phase 0 |
| Phase 2 | 优化器代价驱动 + 真实统计 + ANALYZE | 6-8 周 | Phase 1 |
| Phase 3 | 5 个引擎补全 + storage_ops_t 注册 | 4-6 周 | Phase 1 |
| Phase 4 | IndexAmRoutine 框架 + CONCURRENTLY | 5-7 周 | Phase 1 |
| Phase 5 | 恢复 txn/ 目录 + 多隔离级别 | 6-8 周 | Phase 1 |
| Phase 6 | pgwire + SDK + CLI | 6-8 周 | Phase 1 |

**工程量估算**：

| 路径 | 周期 |
|---|---|
| MVP（Phase 0+1） | 7-9 周（端到端贯通） |
| 生产可用（Phase 0-4） | 22-30 周（多模态 + 索引框架） |
| PG 兼容（Phase 0-6 全部） | 35-46 周 |

**Phase 1 详细任务（解析→计划→执行→存储贯通）**：

| 任务 | 工程量 |
|---|---|
| 删死代码（parse_analyze.c、sql/sql_parser.c、sql/sql_driver.c、graph traverse.c 死函数） | 1 周 |
| node_to_logical 真正解析 SELECT 的 columns/where/table | 1 周 |
| node_to_logical 真正解析 INSERT 的 columns/values | 3 天 |
| 补全 ExecInsert 真正 catalog_insert 调用 | 1 周 |
| 补全 ExecModifyTable 真正 update/delete | 1 周 |
| 补全 ExecIndexScan 真正用 btree 扫描 | 1 周 |
| 补全 ExecHashJoin 真正哈希连接 | 1-2 周 |
| 补全 ExecNestLoop 真正嵌套循环 | 1 周 |
| 补全 ExecAgg 真正聚合 | 1-2 周 |
| 补全 ExecSort 真正排序 | 1 周 |
| 补全 ExecLimit 真正 LIMIT | 3 天 |
| 端到端测试（CREATE TABLE + INSERT + SELECT 全流程） | 1 周 |

**关键约束**：

1. **不重写**：保留所有现有模块（catalog/buffer/wal/mvcc/heap/btree）
2. **不破坏现有测试**：每个 Phase 完成后所有测试仍通过
3. **可增量**：任何 Phase 失败可回退，不影响已完成 Phase

## 10. 重新评估：每个模块"是否值得保留"

**基于代码事实的关键发现**：

| 模块 | 实装度 | 决策 |
|---|---|---|
| `sql_executor.c` (674 行) | 15 个 `exec_xxx` 函数**100% 是桩实现**（全部 `return NULL`） | ❌ **整体删除** |
| `parse_analyze.c` (575 行) | 零调用方 | ❌ **删除** |
| `parse_expr.c` (751 行) | 死代码 | ❌ **删除** |
| `sql/sql_parser.c` (2318 行) | CMake 注释，零调用 | ❌ **删除** |
| `sql/sql_driver.c` (300 行) | 死代码 | ❌ **删除** |
| `optimizer.c` (360 行) | 8 处 TODO，函数体几乎全是"递归→TODO→return" | ❌ **删除**（接口合并到 planner.c） |
| `executor/graph/traverse.c` 4 个函数（~400 行） | `graph_bfs/dfs/dijkstra/pagerank` 零调用 | ❌ **删除** |
| `nodeHash.c` (152 行) | 仅 Init/End，Exec 是桩 | 🔄 **合并到 nodeHashjoin.c** |
| `nodeIndexscan.c::exec_index_scan` | 命名混乱（小写） | 🔄 **改名为 ExecIndexScan** |
| `parser/sql/{sql_parser,sql_lexer,makefuncs,parse_node}.c` (1979 行) | 活跃，12+ 调用方 | ✅ **保留** |
| `sql/sql_ddl.c` (165 行) | 活跃，被 db_sql_ddl 库用 | ✅ **保留** |
| `planner.c` (1419 行) | 22 处 TODO，**架构保留** | 🔄 **保留架构重写 TODO** |
| `executor.c` (920 行) | 13 个 Init 已注册，Result 完整跑通 | ✅ **保留并扩展** |
| 12 个 nodeXxx.c（除 Hash） | 大部分 ExecXxx 真实现 | ✅ **保留** |
| 存储引擎（catalog/rel/buffer/wal/mvcc/btree/heap/vector/graph/...） | 真实现 | ✅ **保留** |
| 多模态（kv/vector 已注册 storage_ops_t；graph/spatial/doc/ts/yang 未注册） | 真实现 | 🔄 **补全 5 个未注册** |

**预计清理**：删除 ~5000-6000 行死代码/桩实现，保留 ~25000 行有用代码

## 11. 三档优化方案（基于"是否值得保留"重新整理）

### 第一档：清理 + 端到端贯通（1-2 周）

**目标**：删除死代码/桩，补全贯通

| 任务 | 工程量 |
|---|---|
| 删除 sql_executor.c (674 行) | 1 小时 |
| 删除 parse_analyze.c + parse_expr.c (1326 行) | 1 小时 |
| 删除 sql/sql_parser.c + sql/sql_driver.c (2618 行) | 1 小时 |
| 删除 optimizer.c (360 行) | 1 小时 |
| 删除 executor/graph/traverse.c 4 个死函数 (~400 行) | 1 小时 |
| 合并 nodeHash.c 到 nodeHashjoin.c (152 行) | 2 小时 |
| 重命名 nodeIndexscan.c::exec_index_scan → ExecIndexScan | 1 小时 |
| 验证编译通过 + 所有测试通过 | 1-2 天 |
| 端到端测试（CREATE TABLE + INSERT + SELECT 全流程） | 1 周 |

### 第二档：补全贯通（4-6 周）

| 任务 | 工程量 |
|---|---|
| 13 个 ExecXxx 的 catalog 集成（验证哪些已实现，哪些需要补） | 2-3 周 |
| 测试覆盖（确保无回归） | 1 周 |

### 第三档：PG 维度补全（按需，Phase 2-6 之前的设计）

按之前的 Phase 2-6 计划，但**只在第一档和第二档完成后再决定**。

**原则**：保留有用的，重写有架构价值但实现简陋的，删除无价值的（死代码/全桩实现）。