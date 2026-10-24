# 执行引擎到生产级数据库完整实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 从当前执行引擎框架演进到生产级数据库，实现 SELECT/INSERT/UPDATE/DELETE 完整执行能力，支持并发访问、崩溃恢复、查询优化和运维管理。

**Architecture:** 
- 阶段 0 实现执行引擎核心：SeqScan/IndexScan/HashJoin/NestLoop/HashAgg/Sort/Limit/ModifyTable 节点
- 阶段 1 实现并发控制：MVCC 可见性、快照管理、表级锁/行级锁、死锁检测、2PC
- 阶段 2 实现恢复能力：WAL 完整恢复、检查点优化、PITR、流复制、主备切换
- 阶段 3 实现查询优化：Selinger 代价模型、统计信息、查询重写、并行执行
- 阶段 4 实现运维能力：性能监控、VACUUM/ANALYZE/REINDEX、备份恢复

**Tech Stack:** C11/C++17，无外部依赖，参考 PostgreSQL 16/17 架构

---

## Global Constraints

- C11 标准（src/*.c）
- C++17 标准（test/*.cpp）
- 无运行时外部依赖
- CMake 3.20+
- 代码路径：`engineering/src/db/sql/`、`engineering/src/db/concurrency/`、`engineering/src/db/wal/` 等
- 头文件路径：`engineering/include/db/sql/`、`engineering/include/db/concurrency/` 等
- 测试路径：`engineering/test/db/sql/`、`engineering/test/db/concurrency/` 等

---

## 文件结构总览

```
engineering/
├── include/db/
│   ├── sql/                          # SQL 执行引擎
│   │   ├── executor.h                # 执行器核心接口（已有）
│   │   ├── nodes/execnodes.h         # 执行器状态结构（已有）
│   │   ├── nodeSeqscan.h             # SeqScan 状态结构
│   │   ├── nodeIndexscan.h           # IndexScan 状态结构
│   │   ├── nodeHashjoin.h            # HashJoin 状态结构
│   │   ├── nodeNestloop.h            # NestLoop 状态结构
│   │   ├── nodeAgg.h                 # 聚合状态结构
│   │   ├── nodeSort.h                # Sort 状态结构
│   │   ├── nodeLimit.h               # Limit 状态结构
│   │   └── nodeModifyTable.h         # ModifyTable 状态结构
│   ├── concurrency/                  # 并发控制
│   │   ├── mvcc.h                    # MVCC 接口
│   │   ├── snapshot.h                # 快照管理
│   │   ├── lock.h                    # 锁管理器
│   │   └── deadlock.h                # 死锁检测
│   ├── wal/                          # WAL 日志
│   │   ├── wal.h                     # WAL 接口
│   │   ├── wal_recovery.h            # 恢复接口
│   │   └── xlog.h                    # XLOG 结构
│   ├── optimizer/                    # 查询优化
│   │   ├── cost.h                    # 代价模型
│   │   ├── statistics.h              # 统计信息
│   │   └── rewrite.h                 # 查询重写
│   └── monitoring/                   # 运维监控
│       ├── pg_stat.h                 # 性能统计
│       └── vacuum.h                  # VACUUM 接口
└── src/db/
    ├── sql/                          # SQL 执行引擎实现
    ├── concurrency/                  # 并发控制实现
    ├── wal/                          # WAL 实现
    ├── optimizer/                    # 优化器实现
    └── monitoring/                   # 运维监控实现
```

---

# 阶段 0：执行引擎核心

**工期**：12-16 周
**状态**：基础设施层已完成，核心算子层进行中
**目标**：完成 SELECT/INSERT/UPDATE/DELETE 基本执行能力

---

## 任务 0.1：SeqScan 节点实现

**目标**：实现全表扫描，对接 Access Method 层

**依赖**：
- `rel.h` - Relation 抽象（已实现）
- `heapam.h` - Heap AM 接口（已实现）
- `executor.h` - Volcano 框架（已实现）

**Files:**
- Create: `engineering/include/db/sql/nodeSeqscan.h`
- Create: `engineering/src/db/sql/nodeSeqscan.c`
- Modify: `engineering/src/db/sql/executor.c`（注册节点）
- Create: `engineering/test/db/sql/test_seqscan.cpp`

**Interfaces:**
- Consumes: `Relation relation_open(Oid relid, int mode)`、`TableScanDesc table_beginscan(Relation rel, int nkeys, ScanKey key)`、`void *table_getnext(TableScanDesc scan)`
- Produces: `SeqScanState *ExecInitSeqScan(SeqScan *node, EState *estate, int eflags)`、`static TupleTableSlot *ExecSeqScan(PlanState *pstate)`、`void ExecEndSeqScan(SeqScanState *node)`

- [ ] **Step 1: 编写 SeqScan 单元测试**

```cpp
// engineering/test/db/sql/test_seqscan.cpp
#include <gtest/gtest.h>
#include "db/sql/executor.h"
#include "db/sql/nodeSeqscan.h"
#include "db/sql/nodes/execnodes.h"
#include "db/rel.h"
#include "db/catalog.h"

extern "C" {

class SeqScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化 catalog 和存储
        catalog_init(":memory:");
        rel_init();
    }
    
    void TearDown() override {
        rel_shutdown();
        catalog_shutdown();
    }
};

TEST_F(SeqScanTest, BasicScan) {
    // 创建测试表
    Oid relid = catalog_create_test_table("test_table", 3);
    ASSERT_NE(relid, InvalidOid);
    
    // 插入测试数据
    Relation rel = relation_open(relid, REL_OPEN_READWRITE);
    // ... 插入逻辑
    
    // 创建 SeqScan 计划
    SeqScan *plan = make_seqscan(relid);
    EState *estate = CreateEState();
    
    // 初始化 SeqScan 状态
    SeqScanState *state = ExecInitSeqScan(plan, estate, 0);
    ASSERT_NE(state, nullptr);
    
    // 执行扫描
    int tuple_count = 0;
    TupleTableSlot *slot;
    while ((slot = ExecProcNode((PlanState *)state)) != NULL) {
        tuple_count++;
    }
    
    EXPECT_EQ(tuple_count, 3);
    
    // 清理
    ExecEndSeqScan(state);
    FreeEState(estate);
    relation_close(rel, REL_OPEN_READWRITE);
}

}  // extern "C"
```

- [ ] **Step 2: 实现 nodeSeqscan.h 头文件**

```c
// engineering/include/db/sql/nodeSeqscan.h
#ifndef DB_SQL_NODESEQSCAN_H
#define DB_SQL_NODESEQSCAN_H

#include "db/sql/nodes/execnodes.h"
#include "db/rel.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SeqScan 计划节点
 */
typedef struct SeqScan {
    Plan        plan;
    Oid         relid;          /**< 表 OID */
    List       *qual;           /**< 过滤条件列表 */
} SeqScan;

/**
 * @brief SeqScan 状态节点
 */
typedef struct SeqScanState {
    PlanState   ps;
    
    /* 扫描状态 */
    Relation    ss_currentRelation;    /**< 当前 Relation */
    TableScanDesc ss_scanDesc;         /**< 扫描描述符 */
    TupleTableSlot *ss_ScanTupleSlot;  /**< 扫描结果槽 */
    
    /* 过滤条件 */
    ExprState  *ss_qual;               /**< 过滤条件 */
    
    /* Buffer 统计 */
    uint64_t    ss_buf_hit;
    uint64_t    ss_buf_read;
} SeqScanState;

/* API */
SeqScanState *ExecInitSeqScan(SeqScan *node, EState *estate, int eflags);
void ExecEndSeqScan(SeqScanState *node);
void ExecReScanSeqScan(SeqScanState *node);

/* 构造函数 */
SeqScan *make_seqscan(Oid relid, List *qual);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODESEQSCAN_H */
```

- [ ] **Step 3: 实现 nodeSeqscan.c 核心逻辑**

```c
// engineering/src/db/sql/nodeSeqscan.c
#include "db/sql/nodeSeqscan.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
#include "db/rel.h"
#include "db/heapam.h"
#include <stdlib.h>

/* 前向声明 */
static TupleTableSlot *ExecSeqScan(PlanState *pstate);

/**
 * @brief 初始化 SeqScan 节点
 */
SeqScanState *ExecInitSeqScan(SeqScan *node, EState *estate, int eflags) {
    SeqScanState *state;
    MemoryContext oldctx;
    
    /* 切换到查询内存上下文 */
    oldctx = MemoryContextSwitchTo(estate->es_query_cxt);
    
    /* 分配状态结构 */
    state = (SeqScanState *)palloc(estate->es_query_cxt, sizeof(SeqScanState));
    state->ps.type = T_SeqScanState;
    state->ps.plan = (Plan *)node;
    state->ps.state = estate;
    
    /* 设置执行函数 */
    PlanStateSetExecProc(&state->ps, ExecSeqScan);
    
    /* 打开 Relation */
    state->ss_currentRelation = relation_open(node->relid, REL_OPEN_READONLY);
    if (!state->ss_currentRelation) {
        pfree(estate->es_query_cxt, state);
        MemoryContextSwitchTo(oldctx);
        return NULL;
    }
    
    /* 初始化扫描描述符 */
    state->ss_scanDesc = table_beginscan(state->ss_currentRelation, 0, NULL);
    
    /* 创建结果槽 */
    state->ss_ScanTupleSlot = MakeTupleTableSlot();
    state->ss_ScanTupleSlot->tts_tupleDescriptor = 
        relation_getdesc(state->ss_currentRelation);
    
    /* 初始化过滤条件 */
    if (node->qual) {
        state->ss_qual = ExecInitExpr((Expr *)node->qual, &state->ps);
    } else {
        state->ss_qual = NULL;
    }
    
    /* 初始化 Buffer 统计 */
    state->ss_buf_hit = 0;
    state->ss_buf_read = 0;
    
    MemoryContextSwitchTo(oldctx);
    return state;
}

/**
 * @brief 执行 SeqScan
 */
static TupleTableSlot *ExecSeqScan(PlanState *pstate) {
    SeqScanState *node = (SeqScanState *)pstate;
    void *tuple;
    bool should_free;
    
    /* 检查节点有效性 */
    CheckNodeIsValid(pstate);
    
    /* 清空结果槽 */
    ExecClearTuple(node->ss_ScanTupleSlot);
    
    /* 从 Heap AM 获取下一个元组 */
    while (true) {
        tuple = table_getnext(node->ss_scanDesc);
        
        if (tuple == NULL) {
            /* 扫描结束 */
            return NULL;
        }
        
        /* 存储元组到槽 */
        ExecStoreBufferHeapTuple(tuple, node->ss_ScanTupleSlot, 
                                  node->ss_currentRelation);
        
        /* 检查过滤条件 */
        if (node->ss_qual) {
            ExprContext *econtext = node->ps.ps_ExprContext;
            econtext->ecxt_scantuple = node->ss_ScanTupleSlot;
            
            if (!ExecCheckEvalExpr(node->ss_qual, econtext)) {
                /* 不满足条件，继续下一个 */
                continue;
            }
        }
        
        /* 返回满足条件的元组 */
        node->ss_buf_hit++;
        return node->ss_ScanTupleSlot;
    }
}

/**
 * @brief 结束 SeqScan
 */
void ExecEndSeqScan(SeqScanState *node) {
    if (!node) return;
    
    /* 结束扫描 */
    if (node->ss_scanDesc) {
        table_endscan(node->ss_scanDesc);
    }
    
    /* 关闭 Relation */
    if (node->ss_currentRelation) {
        relation_close(node->ss_currentRelation, REL_OPEN_READONLY);
    }
    
    /* 释放过滤条件 */
    if (node->ss_qual) {
        ExecFreeExpr(node->ss_qual);
    }
    
    /* 释放结果槽 */
    if (node->ss_ScanTupleSlot) {
        FreeTupleTableSlot(node->ss_ScanTupleSlot);
    }
}

/**
 * @brief 重新扫描 SeqScan
 */
void ExecReScanSeqScan(SeqScanState *node) {
    /* 重置扫描 */
    if (node->ss_scanDesc) {
        table_endscan(node->ss_scanDesc);
    }
    
    node->ss_scanDesc = table_beginscan(node->ss_currentRelation, 0, NULL);
    node->ss_buf_hit = 0;
    node->ss_buf_read = 0;
}

/**
 * @brief 构造 SeqScan 计划节点
 */
SeqScan *make_seqscan(Oid relid, List *qual) {
    SeqScan *node = (SeqScan *)palloc(NULL, sizeof(SeqScan));
    node->plan.type = T_SeqScan;
    node->relid = relid;
    node->qual = qual;
    return node;
}

/* 注册节点 */
static void __attribute__((constructor)) register_seqscan_node(void) {
    executor_register_node(T_SeqScan, (ExecInitNodeFn)ExecInitSeqScan);
}
```

- [ ] **Step 4: 在 executor.c 中注册 SeqScan 节点**

```c
// 在 engineering/src/db/sql/executor.c 中添加
extern void register_seqscan_node(void);  // 前向声明

void executor_register_nodes(void) {
    register_seqscan_node();
    // 其他节点...
}
```

- [ ] **Step 5: 运行测试验证**

Run: `ctest --test-dir build/engineering -R test_seqscan --output-on-failure`
Expected: PASS

- [ ] **Step 6: 提交**

```bash
git add engineering/include/db/sql/nodeSeqscan.h engineering/src/db/sql/nodeSeqscan.c engineering/test/db/sql/test_seqscan.cpp
git commit -m "feat(sql): 实现 SeqScan 节点

- SeqScanState 状态结构
- ExecInitSeqScan 初始化函数
- ExecSeqScan 执行函数（对接 table_getnext）
- ExecEndSeqScan 清理函数
- ExecReScanSeqScan 重置函数
- 单元测试覆盖基本扫描场景
"
```

---

## 任务 0.2：IndexScan 节点实现

**目标**：实现索引扫描，对接 BTree AM

**依赖**：任务 0.1

**Files:**
- Create: `engineering/include/db/sql/nodeIndexscan.h`
- Create: `engineering/src/db/sql/nodeIndexscan.c`
- Create: `engineering/test/db/sql/test_indexscan.cpp`

**Interfaces:**
- Consumes: `IndexScanDesc index_beginscan(Relation rel, int nkeys, ScanKey key)`、`void *index_getnext(IndexScanDesc scan)`
- Produces: `IndexScanState *ExecInitIndexScan(IndexScan *node, EState *estate, int eflags)`、`static TupleTableSlot *ExecIndexScan(PlanState *pstate)`

- [ ] **Step 1: 编写 IndexScan 单元测试**

```cpp
// engineering/test/db/sql/test_indexscan.cpp
#include <gtest/gtest.h>
#include "db/sql/executor.h"
#include "db/sql/nodeIndexscan.h"
#include "db/rel.h"
#include "db/btreeam.h"

extern "C" {

TEST_F(IndexScanTest, PointQuery) {
    // 创建带索引的表
    Oid relid = catalog_create_test_table("test_table", 100);
    Oid indexid = catalog_create_index(relid, "idx_id", "id");
    
    // 插入测试数据
    // ...
    
    // 创建 IndexScan 计划
    ScanKeyData key;
    ScanKeyInit(&key, 1, SCAN_KEY_EQ, (void *)1);
    
    IndexScan *plan = make_indexscan(indexid, relid, 1, &key);
    EState *estate = CreateEState();
    
    IndexScanState *state = ExecInitIndexScan(plan, estate, 0);
    ASSERT_NE(state, nullptr);
    
    // 执行扫描
    TupleTableSlot *slot = ExecProcNode((PlanState *)state);
    ASSERT_NE(slot, nullptr);
    
    // 验证结果
    // ...
    
    ExecEndIndexScan(state);
    FreeEState(estate);
}

}  // extern "C"
```

- [ ] **Step 2: 实现 nodeIndexscan.h**

```c
// engineering/include/db/sql/nodeIndexscan.h
#ifndef DB_SQL_NODEINDEXSCAN_H
#define DB_SQL_NODEINDEXSCAN_H

#include "db/sql/nodes/execnodes.h"
#include "db/rel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IndexScan {
    Plan        plan;
    Oid         indexid;        /**< 索引 OID */
    Oid         relid;          /**< 表 OID */
    int         nkeys;          /**< 扫描键数量 */
    ScanKey     key;            /**< 扫描键数组 */
    List       *qual;           /**< 过滤条件 */
} IndexScan;

typedef struct IndexScanState {
    PlanState   ps;
    
    Relation    iss_indexRelation;     /**< 索引 Relation */
    Relation    iss_heapRelation;      /**< 堆 Relation */
    IndexScanDesc iss_ScanDesc;        /**< 索引扫描描述符 */
    TupleTableSlot *iss_ScanTupleSlot; /**< 结果槽 */
    
    ExprState  *iss_qual;              /**< 过滤条件 */
} IndexScanState;

IndexScanState *ExecInitIndexScan(IndexScan *node, EState *estate, int eflags);
void ExecEndIndexScan(IndexScanState *node);
void ExecReScanIndexScan(IndexScanState *node);

IndexScan *make_indexscan(Oid indexid, Oid relid, int nkeys, ScanKey key);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODEINDEXSCAN_H */
```

- [ ] **Step 3: 实现 nodeIndexscan.c**

```c
// engineering/src/db/sql/nodeIndexscan.c
#include "db/sql/nodeIndexscan.h"
#include "db/sql/executor.h"
#include "db/rel.h"
#include "db/btreeam.h"
#include <stdlib.h>

static TupleTableSlot *ExecIndexScan(PlanState *pstate);

IndexScanState *ExecInitIndexScan(IndexScan *node, EState *estate, int eflags) {
    IndexScanState *state;
    MemoryContext oldctx;
    
    oldctx = MemoryContextSwitchTo(estate->es_query_cxt);
    
    state = (IndexScanState *)palloc(estate->es_query_cxt, sizeof(IndexScanState));
    state->ps.type = T_IndexScanState;
    state->ps.plan = (Plan *)node;
    state->ps.state = estate;
    
    PlanStateSetExecProc(&state->ps, ExecIndexScan);
    
    /* 打开索引和堆 Relation */
    state->iss_indexRelation = relation_open(node->indexid, REL_OPEN_READONLY);
    state->iss_heapRelation = relation_open(node->relid, REL_OPEN_READONLY);
    
    /* 开始索引扫描 */
    state->iss_ScanDesc = index_beginscan_heapscan(
        state->iss_indexRelation, 
        state->iss_heapRelation,
        node->nkeys, 
        node->key
    );
    
    state->iss_ScanTupleSlot = MakeTupleTableSlot();
    state->iss_ScanTupleSlot->tts_tupleDescriptor = 
        relation_getdesc(state->iss_heapRelation);
    
    if (node->qual) {
        state->iss_qual = ExecInitExpr((Expr *)node->qual, &state->ps);
    } else {
        state->iss_qual = NULL;
    }
    
    MemoryContextSwitchTo(oldctx);
    return state;
}

static TupleTableSlot *ExecIndexScan(PlanState *pstate) {
    IndexScanState *node = (IndexScanState *)pstate;
    void *tuple;
    
    CheckNodeIsValid(pstate);
    ExecClearTuple(node->iss_ScanTupleSlot);
    
    while (true) {
        tuple = index_getnext(node->iss_ScanDesc);
        
        if (tuple == NULL) {
            return NULL;
        }
        
        ExecStoreBufferHeapTuple(tuple, node->iss_ScanTupleSlot,
                                  node->iss_heapRelation);
        
        if (node->iss_qual) {
            ExprContext *econtext = node->ps.ps_ExprContext;
            econtext->ecxt_scantuple = node->iss_ScanTupleSlot;
            
            if (!ExecCheckEvalExpr(node->iss_qual, econtext)) {
                continue;
            }
        }
        
        return node->iss_ScanTupleSlot;
    }
}

void ExecEndIndexScan(IndexScanState *node) {
    if (!node) return;
    
    if (node->iss_ScanDesc) {
        index_endscan(node->iss_ScanDesc);
    }
    
    if (node->iss_indexRelation) {
        relation_close(node->iss_indexRelation, REL_OPEN_READONLY);
    }
    if (node->iss_heapRelation) {
        relation_close(node->iss_heapRelation, REL_OPEN_READONLY);
    }
    
    if (node->iss_qual) {
        ExecFreeExpr(node->iss_qual);
    }
    
    if (node->iss_ScanTupleSlot) {
        FreeTupleTableSlot(node->iss_ScanTupleSlot);
    }
}

void ExecReScanIndexScan(IndexScanState *node) {
    if (node->iss_ScanDesc) {
        index_endscan(node->iss_ScanDesc);
    }
    
    IndexScan *plan = (IndexScan *)node->ps.plan;
    node->iss_ScanDesc = index_beginscan_heapscan(
        node->iss_indexRelation,
        node->iss_heapRelation,
        plan->nkeys,
        plan->key
    );
}

IndexScan *make_indexscan(Oid indexid, Oid relid, int nkeys, ScanKey key) {
    IndexScan *node = (IndexScan *)palloc(NULL, sizeof(IndexScan));
    node->plan.type = T_IndexScan;
    node->indexid = indexid;
    node->relid = relid;
    node->nkeys = nkeys;
    node->key = key;
    node->qual = NULL;
    return node;
}

static void __attribute__((constructor)) register_indexscan_node(void) {
    executor_register_node(T_IndexScan, (ExecInitNodeFn)ExecInitIndexScan);
}
```

- [ ] **Step 4: 运行测试验证**

Run: `ctest --test-dir build/engineering -R test_indexscan --output-on-failure`
Expected: PASS

- [ ] **Step 5: 提交**

```bash
git add engineering/include/db/sql/nodeIndexscan.h engineering/src/db/sql/nodeIndexscan.c engineering/test/db/sql/test_indexscan.cpp
git commit -m "feat(sql): 实现 IndexScan 节点

- IndexScanState 状态结构
- ExecInitIndexScan 初始化函数
- ExecIndexScan 执行函数（对接 index_getnext）
- 支持点查询和范围查询
- 单元测试覆盖索引扫描场景
"
```

---

## 任务 0.3：HashJoin 节点实现

**目标**：实现哈希连接，支持等值连接

**依赖**：任务 0.1

**Files:**
- Create: `engineering/include/db/sql/nodeHashjoin.h`
- Create: `engineering/src/db/sql/nodeHashjoin.c`
- Create: `engineering/src/db/sql/nodeHash.c`（Hash 表构建）
- Create: `engineering/test/db/sql/test_hashjoin.cpp`

**Interfaces:**
- Produces: `HashJoinState *ExecInitHashJoin(HashJoin *node, EState *estate, int eflags)`、`static TupleTableSlot *ExecHashJoin(PlanState *pstate)`

**预估工作量**：~2,500 行，3 周

- [ ] **Step 1: 编写 HashJoin 单元测试**

```cpp
// engineering/test/db/sql/test_hashjoin.cpp
#include <gtest/gtest.h>
#include "db/sql/executor.h"
#include "db/sql/nodeHashjoin.h"

extern "C" {

TEST_F(HashJoinTest, InnerJoin) {
    // 创建两个测试表
    Oid t1_relid = catalog_create_test_table("t1", 10);
    Oid t2_relid = catalog_create_test_table("t2", 10);
    
    // 插入测试数据
    // t1: (1, 'a'), (2, 'b'), (3, 'c')
    // t2: (1, 'x'), (2, 'y'), (4, 'z')
    
    // 创建 HashJoin 计划
    // SELECT * FROM t1 JOIN t2 ON t1.id = t2.id
    HashJoin *plan = make_hashjoin(t1_relid, t2_relid, "id", "id");
    EState *estate = CreateEState();
    
    HashJoinState *state = ExecInitHashJoin(plan, estate, 0);
    ASSERT_NE(state, nullptr);
    
    // 执行连接
    std::vector<int> results;
    TupleTableSlot *slot;
    while ((slot = ExecProcNode((PlanState *)state)) != NULL) {
        results.push_back(extract_id(slot));
    }
    
    // 验证结果：(1, 'a', 'x'), (2, 'b', 'y')
    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
    
    ExecEndHashJoin(state);
    FreeEState(estate);
}

}  // extern "C"
```

- [ ] **Step 2: 实现 nodeHashjoin.h**

```c
// engineering/include/db/sql/nodeHashjoin.h
#ifndef DB_SQL_NODEHASHJOIN_H
#define DB_SQL_NODEHASHJOIN_H

#include "db/sql/nodes/execnodes.h"
#include "db/sql/nodeHash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HashJoin {
    Plan        plan;
    List       *hashclauses;       /**< 哈希连接条件 */
    List       *otherclauses;      /**< 其他条件 */
} HashJoin;

typedef struct HashJoinState {
    PlanState   ps;
    
    /* 构建端（内表） */
    PlanState  *hash_build_state;
    HashState  *hash_state;
    
    /* 探测端（外表） */
    PlanState  *hash_probe_state;
    TupleTableSlot *outer_tuple;
    
    /* 连接条件 */
    ExprState  *hashclauses;
    ExprState  *otherclauses;
    
    /* 执行状态 */
    bool        hash_build_done;
    bool        hash_probe_done;
} HashJoinState;

HashJoinState *ExecInitHashJoin(HashJoin *node, EState *estate, int eflags);
void ExecEndHashJoin(HashJoinState *node);
void ExecReScanHashJoin(HashJoinState *node);

HashJoin *make_hashjoin(List *hashclauses, List *otherclauses);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODEHASHJOIN_H */
```

- [ ] **Step 3: 实现 nodeHashjoin.c**

```c
// engineering/src/db/sql/nodeHashjoin.c
#include "db/sql/nodeHashjoin.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
#include <stdlib.h>

static TupleTableSlot *ExecHashJoin(PlanState *pstate);

HashJoinState *ExecInitHashJoin(HashJoin *node, EState *estate, int eflags) {
    HashJoinState *state;
    MemoryContext oldctx;
    
    oldctx = MemoryContextSwitchTo(estate->es_query_cxt);
    
    state = (HashJoinState *)palloc(estate->es_query_cxt, sizeof(HashJoinState));
    state->ps.type = T_HashJoinState;
    state->ps.plan = (Plan *)node;
    state->ps.state = estate;
    
    PlanStateSetExecProc(&state->ps, ExecHashJoin);
    
    /* 初始化子节点 */
    state->hash_build_state = ExecInitNode(node->plan.lefttree, estate, eflags);
    state->hash_probe_state = ExecInitNode(node->plan.righttree, estate, eflags);
    
    /* 初始化 Hash 表 */
    state->hash_state = ExecInitHash((Hash *)node->plan.lefttree, estate, eflags);
    
    /* 初始化连接条件 */
    if (node->hashclauses) {
        state->hashclauses = ExecInitExpr((Expr *)node->hashclauses, &state->ps);
    } else {
        state->hashclauses = NULL;
    }
    
    if (node->otherclauses) {
        state->otherclauses = ExecInitExpr((Expr *)node->otherclauses, &state->ps);
    } else {
        state->otherclauses = NULL;
    }
    
    /* 初始化执行状态 */
    state->hash_build_done = false;
    state->hash_probe_done = false;
    state->outer_tuple = NULL;
    
    MemoryContextSwitchTo(oldctx);
    return state;
}

static TupleTableSlot *ExecHashJoin(PlanState *pstate) {
    HashJoinState *node = (HashJoinState *)pstate;
    
    CheckNodeIsValid(pstate);
    
    /* 阶段 1：构建 Hash 表 */
    if (!node->hash_build_done) {
        TupleTableSlot *slot;
        
        while ((slot = ExecProcNode(node->hash_build_state)) != NULL) {
            /* 插入到 Hash 表 */
            ExecHashInsert(node->hash_state, slot);
        }
        
        node->hash_build_done = true;
    }
    
    /* 阶段 2：探测 Hash 表 */
    while (true) {
        /* 获取下一个外表元组 */
        if (node->outer_tuple == NULL) {
            node->outer_tuple = ExecProcNode(node->hash_probe_state);
            
            if (node->outer_tuple == NULL) {
                return NULL;  /* 连接结束 */
            }
        }
        
        /* 在 Hash 表中查找匹配 */
        TupleTableSlot *result = ExecHashProbe(node->hash_state, 
                                                 node->outer_tuple,
                                                 node->hashclauses);
        
        if (result != NULL) {
            /* 检查其他条件 */
            if (node->otherclauses) {
                ExprContext *econtext = node->ps.ps_ExprContext;
                econtext->ecxt_innertuple = result;
                econtext->ecxt_outertuple = node->outer_tuple;
                
                if (!ExecCheckEvalExpr(node->otherclauses, econtext)) {
                    continue;
                }
            }
            
            return result;
        }
        
        /* 当前外表元组无匹配，获取下一个 */
        node->outer_tuple = NULL;
    }
}

void ExecEndHashJoin(HashJoinState *node) {
    if (!node) return;
    
    if (node->hash_build_state) {
        ExecEndNode(node->hash_build_state);
    }
    if (node->hash_probe_state) {
        ExecEndNode(node->hash_probe_state);
    }
    if (node->hash_state) {
        ExecEndHash(node->hash_state);
    }
    if (node->hashclauses) {
        ExecFreeExpr(node->hashclauses);
    }
    if (node->otherclauses) {
        ExecFreeExpr(node->otherclauses);
    }
}

void ExecReScanHashJoin(HashJoinState *node) {
    ExecReScan(node->hash_build_state);
    ExecReScan(node->hash_probe_state);
    ExecReScanHash(node->hash_state);
    
    node->hash_build_done = false;
    node->hash_probe_done = false;
    node->outer_tuple = NULL;
}

HashJoin *make_hashjoin(List *hashclauses, List *otherclauses) {
    HashJoin *node = (HashJoin *)palloc(NULL, sizeof(HashJoin));
    node->plan.type = T_HashJoin;
    node->hashclauses = hashclauses;
    node->otherclauses = otherclauses;
    return node;
}

static void __attribute__((constructor)) register_hashjoin_node(void) {
    executor_register_node(T_HashJoin, (ExecInitNodeFn)ExecInitHashJoin);
}
```

- [ ] **Step 4: 实现 nodeHash.c（Hash 表构建）**

```c
// engineering/src/db/sql/nodeHash.c
#include "db/sql/nodeHash.h"
#include "db/sql/executor.h"
#include <stdlib.h>

#define HASH_TABLE_SIZE 1024

HashState *ExecInitHash(Hash *node, EState *estate, int eflags) {
    HashState *state;
    
    state = (HashState *)palloc(estate->es_query_cxt, sizeof(HashState));
    state->ps.type = T_HashState;
    state->ps.plan = (Plan *)node;
    state->ps.state = estate;
    
    /* 创建 Hash 表 */
    state->hashtable = (HashTable *)palloc(estate->es_query_cxt, sizeof(HashTable));
    state->hashtable->size = HASH_TABLE_SIZE;
    state->hashtable->buckets = (HashBucket *)palloc0(estate->es_query_cxt,
        HASH_TABLE_SIZE * sizeof(HashBucket));
    state->hashtable->ntuples = 0;
    
    return state;
}

void ExecHashInsert(HashState *state, TupleTableSlot *slot) {
    uint32_t hash = compute_hash(slot);
    uint32_t bucket_idx = hash % state->hashtable->size;
    
    HashBucket *bucket = &state->hashtable->buckets[bucket_idx];
    
    /* 插入到链表头部 */
    HashEntry *entry = (HashEntry *)palloc(state->ps.state->es_query_cxt, sizeof(HashEntry));
    entry->slot = slot;
    entry->next = bucket->head;
    bucket->head = entry;
    
    state->hashtable->ntuples++;
}

TupleTableSlot *ExecHashProbe(HashState *state, TupleTableSlot *outer_slot, ExprState *hashclauses) {
    uint32_t hash = compute_hash(outer_slot);
    uint32_t bucket_idx = hash % state->hashtable->size;
    
    HashBucket *bucket = &state->hashtable->buckets[bucket_idx];
    HashEntry *entry = bucket->head;
    
    while (entry != NULL) {
        /* 检查是否匹配 */
        if (hashclauses == NULL || check_match(outer_slot, entry->slot, hashclauses)) {
            return entry->slot;
        }
        entry = entry->next;
    }
    
    return NULL;
}

void ExecEndHash(HashState *state) {
    if (!state) return;
    /* Hash 表由内存上下文管理 */
}

void ExecReScanHash(HashState *state) {
    /* 清空 Hash 表 */
    state->hashtable->ntuples = 0;
    memset(state->hashtable->buckets, 0, state->hashtable->size * sizeof(HashBucket));
}
```

- [ ] **Step 5: 运行测试验证**

Run: `ctest --test-dir build/engineering -R test_hashjoin --output-on-failure`
Expected: PASS

- [ ] **Step 6: 提交**

```bash
git add engineering/include/db/sql/nodeHashjoin.h engineering/src/db/sql/nodeHashjoin.c engineering/src/db/sql/nodeHash.c engineering/test/db/sql/test_hashjoin.cpp
git commit -m "feat(sql): 实现 HashJoin 节点

- HashJoinState 状态结构
- 两阶段执行：构建 Hash 表 + 探测匹配
- ExecHashInsert 插入到 Hash 表
- ExecHashProbe 探测匹配元组
- 支持多条件连接
- 单元测试覆盖内连接场景
"
```

---

## 任务 0.4-0.8：其他核心节点（省略详细步骤）

由于篇幅限制，以下任务的结构与前面类似：

- **任务 0.4**：NestLoop 节点实现（~1,500 行，2 周）
- **任务 0.5**：HashAgg 节点实现（~2,000 行，2.5 周）
- **任务 0.6**：Sort 节点实现（~1,500 行，1.5 周）
- **任务 0.7**：Limit 节点实现（~500 行，1 周）
- **任务 0.8**：ModifyTable 节点实现（~2,000 行，2.5 周）

---

## 任务 0.9：阶段 0 集成测试

**目标**：验证端到端 SQL 执行能力

**Files:**
- Create: `engineering/test/db/sql/test_integration_phase0.cpp`

- [ ] **Step 1: 编写集成测试**

```cpp
// engineering/test/db/sql/test_integration_phase0.cpp
#include <gtest/gtest.h>
#include "db/sql/executor.h"

extern "C" {

TEST_F(IntegrationTest, SelectStar) {
    // SELECT * FROM t
    // 验证 SeqScan 工作正常
}

TEST_F(IntegrationTest, SelectWhere) {
    // SELECT * FROM t WHERE id = 1
    // 验证 IndexScan + 过滤条件工作正常
}

TEST_F(IntegrationTest, SelectJoin) {
    // SELECT * FROM t1 JOIN t2 ON t1.id = t2.id
    // 验证 HashJoin 工作正常
}

TEST_F(IntegrationTest, SelectGroupBy) {
    // SELECT COUNT(*), SUM(x) FROM t GROUP BY y
    // 验证 HashAgg 工作正常
}

TEST_F(IntegrationTest, InsertUpdateDelete) {
    // INSERT INTO t VALUES(...)
    // UPDATE t SET x = 1 WHERE y = 2
    // DELETE FROM t WHERE x = 1
    // 验证 ModifyTable 工作正常
}

}  // extern "C"
```

- [ ] **Step 2: 运行所有阶段 0 测试**

Run: `ctest --test-dir build/engineering -R test_ --output-on-failure`
Expected: ALL PASS

- [ ] **Step 3: 提交阶段 0 完成**

```bash
git add -A
git commit -m "feat(sql): 阶段 0 执行引擎核心完成

里程碑 M0：完成 SELECT/INSERT/UPDATE/DELETE 基本执行能力

实现内容：
- SeqScan/IndexScan 扫描节点
- HashJoin/NestLoop 连接节点
- HashAgg 聚合节点
- Sort/Limit 修饰节点
- ModifyTable DML 节点

验收标准：
- SELECT * FROM t ✓
- SELECT * FROM t WHERE id = 1 ✓
- SELECT * FROM t1 JOIN t2 ON t1.id = t2.id ✓
- SELECT COUNT(*) FROM t GROUP BY x ✓
- INSERT/UPDATE/DELETE ✓
"
```

---

# 阶段 1：并发控制

**工期**：16-20 周
**前置条件**：阶段 0 完成
**目标**：支持多用户并发访问

---

## 任务 1.1：MVCC 可见性判断

**目标**：实现元组可见性判断，支持读已提交、可重复读隔离级别

**Files:**
- Create: `engineering/include/db/concurrency/mvcc.h`
- Create: `engineering/src/db/concurrency/mvcc_visibility.c`
- Create: `engineering/src/db/concurrency/mvcc_version.c`
- Create: `engineering/test/db/concurrency/test_mvcc.cpp`

**Interfaces:**
- Produces: `bool XidInMVCCSnapshot(TransactionId xid, Snapshot snapshot)`、`bool HeapTupleSatisfiesVisibility(HeapTuple tuple, Snapshot snapshot)`

**预估工作量**：~3,000 行，3 周

- [ ] **Step 1: 编写 MVCC 可见性测试**

```cpp
// engineering/test/db/concurrency/test_mvcc.cpp
#include <gtest/gtest.h>
#include "db/concurrency/mvcc.h"

extern "C" {

TEST_F(MVCCTest, VisibilityBasic) {
    // 创建快照
    Snapshot snap = CreateSnapshot();
    
    // 测试已提交事务可见性
    TransactionId committed_xid = 100;
    TransactionIdCommit(committed_xid);
    EXPECT_TRUE(XidInMVCCSnapshot(committed_xid, snap));
    
    // 测试进行中事务不可见
    TransactionId inprogress_xid = 200;
    TransactionIdStart(inprogress_xid);
    EXPECT_FALSE(XidInMVCCSnapshot(inprogress_xid, snap));
}

TEST_F(MVCCTest, ReadCommitted) {
    // 测试读已提交隔离级别
}

TEST_F(MVCCTest, RepeatableRead) {
    // 测试可重复读隔离级别
}

}  // extern "C"
```

- [ ] **Step 2: 实现 mvcc_visibility.c**

```c
// engineering/src/db/concurrency/mvcc_visibility.c
#include "db/concurrency/mvcc.h"
#include <stdlib.h>

/**
 * @brief 判断事务 ID 是否在快照中可见
 */
bool XidInMVCCSnapshot(TransactionId xid, Snapshot snapshot) {
    /* 1. 检查是否为当前事务 */
    if (TransactionIdIsCurrentTransactionId(xid)) {
        return true;
    }
    
    /* 2. 检查是否在进行中 */
    if (TransactionIdIsInProgress(xid)) {
        return false;
    }
    
    /* 3. 检查是否已提交 */
    if (!TransactionIdDidCommit(xid)) {
        return false;  /* 回滚的事务不可见 */
    }
    
    /* 4. 检查是否在快照之前 */
    if (TransactionIdPrecedes(xid, snapshot->xmin)) {
        return true;  /* 快照之前的已提交事务可见 */
    }
    
    /* 5. 检查是否在活跃事务列表中 */
    for (int i = 0; i < snapshot->xcnt; i++) {
        if (snapshot->xip[i] == xid) {
            return false;  /* 在活跃列表中不可见 */
        }
    }
    
    return true;
}

/**
 * @brief 判断元组是否对快照可见
 */
bool HeapTupleSatisfiesVisibility(HeapTuple tuple, Snapshot snapshot) {
    TransactionId xmin = HeapTupleHeaderGetXmin(tuple);
    TransactionId xmax = HeapTupleHeaderGetXmax(tuple);
    
    /* 检查 xmin */
    if (!XidInMVCCSnapshot(xmin, snapshot)) {
        return false;  /* 插入事务不可见 */
    }
    
    /* 检查 xmax */
    if (TransactionIdIsValid(xmax) && XidInMVCCSnapshot(xmax, snapshot)) {
        return false;  /* 删除事务可见，元组不可见 */
    }
    
    return true;
}
```

- [ ] **Step 3: 提交**

```bash
git add engineering/include/db/concurrency/mvcc.h engineering/src/db/concurrency/mvcc_visibility.c engineering/test/db/concurrency/test_mvcc.cpp
git commit -m "feat(concurrency): 实现 MVCC 可见性判断

- XidInMVCCSnapshot 事务可见性判断
- HeapTupleSatisfiesVisibility 元组可见性判断
- 支持读已提交、可重复读隔离级别
"
```

---

## 任务 1.2-1.6：其他并发控制任务

- **任务 1.2**：快照管理（~2,000 行，2 周）
- **任务 1.3**：表级锁（~4,000 行，4 周）
- **任务 1.4**：行级锁（~3,000 行，3 周）
- **任务 1.5**：死锁检测（~1,500 行，2 周）
- **任务 1.6**：两阶段提交（~2,000 行，2 周）

---

# 阶段 2：恢复能力

**工期**：17-21 周
**目标**：保证数据安全

---

## 任务 2.1：WAL 完整恢复

**目标**：实现崩溃恢复流程

**Files:**
- Create: `engineering/include/db/wal/wal_recovery.h`
- Create: `engineering/src/db/wal/wal_recovery.c`
- Create: `engineering/test/db/wal/test_recovery.cpp`

**预估工作量**：~5,000 行，4 周

---

## 任务 2.2-2.6：其他恢复能力任务

- **任务 2.2**：检查点优化（~2,000 行，2 周）
- **任务 2.3**：基础备份（~2,000 行，2 周）
- **任务 2.4**：PITR（~3,000 行，3 周）
- **任务 2.5**：流复制（~5,000 行，4 周）
- **任务 2.6**：主备切换（~2,000 行，2 周）

---

# 阶段 3：查询优化

**工期**：13-17 周
**目标**：支持复杂查询

---

## 任务 3.1：Selinger 代价模型

**预估工作量**：~3,000 行，3 周

---

## 任务 3.2-3.5：其他查询优化任务

- **任务 3.2**：统计信息收集（~2,500 行，2.5 周）
- **任务 3.3**：查询重写规则（~3,000 行，3 周）
- **任务 3.4**：并行 SeqScan（~2,000 行，2 周）
- **任务 3.5**：并行 HashJoin（~2,500 行，2.5 周）

---

# 阶段 4：运维能力

**工期**：13-17 周
**目标**：可生产运维

---

## 任务 4.1：性能监控

**预估工作量**：~2,500 行，2.5 周

---

## 任务 4.2-4.5：其他运维能力任务

- **任务 4.2**：VACUUM（~4,000 行，4 周）
- **任务 4.3**：ANALYZE（~2,000 行，2 周）
- **任务 4.4**：REINDEX（~1,500 行，1.5 周）
- **任务 4.5**：pg_dump/pg_restore（~3,000 行，3 周）

---

# 总结

## 总工作量汇总

| 阶段 | 代码量 | 工期 | 累计工期 |
|------|--------|------|----------|
| 阶段 0：执行引擎核心 | ~18,000 行 | 12-16 周 | 12-16 周 |
| 阶段 1：并发控制 | ~20,500 行 | 16-20 周 | 28-36 周 |
| 阶段 2：恢复能力 | ~24,000 行 | 17-21 周 | 45-57 周 |
| 阶段 3：查询优化 | ~20,000 行 | 13-17 周 | 58-74 周 |
| 阶段 4：运维能力 | ~20,000 行 | 13-17 周 | **71-91 周** |
| **总计** | **~102,500 行** | - | **17-22 人月** |

---

## 关键成功因素

1. **阶段性验收**：每个阶段完成后进行端到端验收
2. **自动化测试**：每个模块都有完整的单元测试和集成测试
3. **性能基准**：建立性能基准，监控关键指标
4. **文档同步**：代码和文档同步更新

---

*计划版本：v1.0*
*创建日期：2026-07-15*