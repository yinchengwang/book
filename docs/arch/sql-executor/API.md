# SQL 执行引擎 API 参考

**版本**：v5.0
**更新日期**：2026-07-15

---

## 1. 内存管理

### 1.1 MemoryContext

```c
#include "db/sql/memctx.h"

typedef struct MemoryContextData *MemoryContext;
```

**功能**：内存上下文，支持层级管理和批量释放。

#### 核心函数

| 函数 | 说明 |
|------|------|
| `AllocSetContextCreate(parent, name, min, init, max)` | 创建 AllocSet 上下文 |
| `MemoryContextDelete(ctx)` | 删除上下文及其所有子上下文 |
| `palloc(ctx, size)` | 在上下文中分配内存 |
| `palloc0(ctx, size)` | 分配并清零 |
| `pfree(ctx, ptr)` | 释放内存（no-op） |

#### 示例

```c
MemoryContext ctx = AllocSetContextCreate(NULL, "MyContext",
                                          0, 8192, 8192);
void *ptr = palloc(ctx, 1024);
MemoryContextDelete(ctx);  /* 释放 ctx 及 ptr */
```

---

## 2. 表达式求值

### 2.1 ExprState

```c
#include "db/sql/expr.h"

typedef struct ExprState {
    NodeTag        type;
    Expr          *expr;
    ExprEvalStep  *steps;
    int            nsteps;
} ExprState;
```

**功能**：编译后的表达式状态。

#### 核心函数

| 函数 | 说明 |
|------|------|
| `ExecInitExpr(expr, estate)` | 初始化表达式 |
| `ExecEvalExpr(state, econtext)` | 求值表达式 |
| `ExecEndExpr(state)` | 释放表达式状态 |

---

## 3. 执行器框架

### 3.1 EState

```c
#include "db/sql/executor.h"

typedef struct EState {
    MemoryContext   es_query_cxt;
    List           *es_range_table;
    Snapshot        es_snapshot;
} EState;
```

**功能**：执行器全局状态。

#### 核心函数

| 函数 | 说明 |
|------|------|
| `CreateExecutorState()` | 创建执行状态 |
| `FreeExecutorState(estate)` | 释放执行状态 |

---

### 3.2 PlanState

```c
typedef struct PlanState {
    NodeTag          type;
    Plan            *plan;
    EState          *state;
    ExecProcNodeMtd  ExecProcNode;
    PlanState       *lefttree;
    PlanState       *righttree;
} PlanState;
```

**功能**：计划节点运行时状态。

#### 核心函数

| 函数 | 说明 |
|------|------|
| `ExecInitNode(plan, estate, eflags)` | 初始化计划节点 |
| `ExecProcNode(pstate)` | 获取下一个元组 |
| `ExecEndNode(pstate)` | 释放节点状态 |
| `ExecReScan(pstate)` | 重置节点状态 |

---

## 4. 扫描节点

### 4.1 SeqScan

```c
#include "db/sql/nodeSeqscan.h"

typedef struct SeqScan {
    Plan    plan;
    Oid     scanrelid;
} SeqScan;

typedef struct SeqScanState {
    PlanState       ps;
    Relation        ss_currentRelation;
    HeapScanDesc    ss_currentScanDesc;
    TupleTableSlot  *ss_ScanTupleSlot;
} SeqScanState;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `make_seqscan(relid)` | 创建 SeqScan 计划 |
| `ExecInitSeqScan(plan, estate, eflags)` | 初始化 |
| `ExecSeqScan(pstate)` | 执行 |
| `ExecEndSeqScan(node)` | 清理 |

---

### 4.2 IndexScan

```c
#include "db/sql/nodeIndexscan.h"

typedef struct IndexScan {
    Plan       plan;
    Oid        scanrelid;
    Oid        indexid;
    List      *indexqual;      /* 索引条件 */
    ScanDirection direction;   /* 扫描方向 */
} IndexScan;

typedef struct IndexScanState {
    PlanState      ps;
    Relation       *iss_RelationDesc;
    IndexScanDesc *iss_ScanDesc;
    // ...
} IndexScanState;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `make_indexscan(relid, indexid, indexqual)` | 创建 IndexScan 计划 |
| `ExecInitIndexScan(plan, estate, eflags)` | 初始化 |
| `ExecIndexScan(pstate)` | 执行 |

---

## 5. 连接节点

### 5.1 HashJoin

```c
#include "db/sql/nodeHashjoin.h"

typedef struct HashJoin {
    Plan    plan;
    Oid     build_relid;
    Oid     probe_relid;
    Expr    *join_qual;
} HashJoin;

typedef struct HashJoinState {
    PlanState      js;
    HashTable      hashtable;
    HashJoinType   jointype;
} HashJoinState;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `make_hashjoin_plan(build_relid, probe_relid)` | 创建 HashJoin 计划 |
| `ExecInitHashJoin(plan, estate, eflags)` | 初始化 |
| `ExecHashJoin(pstate)` | 执行 |
| `ExecEndHashJoin(node)` | 清理 |

---

### 5.2 NestLoop

```c
#include "db/sql/nodeNestloop.h"

typedef struct NestLoop {
    Plan    plan;
    Expr   *join.join_quals;   /* 连接条件 */
} NestLoop;

typedef struct NestLoopState {
    PlanState  js;
    // ...
} NestLoopState;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `ExecInitNestLoop(plan, estate, eflags)` | 初始化 |
| `ExecNestLoop(pstate)` | 执行 |

---

## 6. 聚合节点

### 6.1 Agg

```c
#include "db/sql/nodeAgg.h"

typedef struct Agg {
    Plan        plan;
    AggStrategy aggstrategy;
    int         numCols;
    AttrNumber *grpColIdx;
} Agg;

typedef enum AggStrategy {
    AGG_PLAIN,    /* 无 GROUP BY */
    AGG_SORTED,   /* 排序聚合 */
    AGG_HASHED    /* 哈希聚合 */
} AggStrategy;

typedef struct AggState {
    PlanState          ps;
    AggStrategy        strategy;
    TupleTableSlot    *output_slot;
    // ...
} AggState;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `ExecInitAgg(plan, estate, eflags)` | 初始化 |
| `ExecAgg(pstate)` | 执行聚合 |
| `ExecEndAgg(node)` | 清理 |

---

### 6.2 Sort

```c
#include "db/sql/nodeSort.h"

typedef struct Sort {
    Plan       plan;
    int        numCols;
    AttrNumber *sortColIdx;
    Oid        *sortOperators;
} Sort;

typedef struct SortState {
    PlanState   ps;
    Tuplesortstate *tuplesort;
    // ...
} SortState;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `ExecInitSort(plan, estate, eflags)` | 初始化 |
| `ExecSort(pstate)` | 执行排序 |
| `ExecEndSort(node)` | 清理 |

---

## 7. 修改节点

### 7.1 ModifyTable

```c
#include "db/sql/nodeModifyTable.h"

typedef struct ModifyTable {
    Plan           plan;
    CmdType        operation;     /* CMD_INSERT/UPDATE/DELETE */
    Oid            resultRelid;   /* 目标表 OID */
    Plan          *planTablish;   /* 子计划 */
} ModifyTable;

typedef struct ModifyTableState {
    PlanState      ps;
    CmdType        operation;
    Oid            resultRelid;
    // ...
} ModifyTableState;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `ExecInitModifyTable(plan, estate, eflags)` | 初始化 |
| `ExecModifyTable(pstate)` | 执行修改 |
| `ExecEndModifyTable(node)` | 清理 |

---

## 8. 并行执行

### 8.1 ParallelContext

```c
#include "db/sql/parallel.h"

typedef struct ParallelContext {
    NodeTag             type;
    int                 nworkers;
    ParallelCoordinator *coordinator;
    void                *dsm_handle;
} ParallelContext;

typedef struct Gather {
    Plan    plan;
    int     nworkers;
    // ...
} Gather;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `CreateParallelContext(nworkers)` | 创建并行上下文 |
| `LaunchParallelWorkers(pcxt)` | 启动 worker |
| `WaitForParallelWorkers(pcxt)` | 等待完成 |
| `DestroyParallelContext(pcxt)` | 销毁 |

---

### 8.2 TupleQueue

```c
#include "db/sql/tqueue.h"

typedef struct TupleQueue {
    pthread_mutex_t    mutex;
    pthread_cond_t     cond;
    TupleTableSlot   **buffers;
    int                 capacity;
    int                 head;
    int                 tail;
} TupleQueue;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `TupleQueueCreate(capacity)` | 创建队列 |
| `TupleQueuePush(queue, slot)` | 入队 |
| `TupleQueuePop(queue)` | 出队 |
| `TupleQueueDestroy(queue)` | 销毁 |

---

## 9. 触发器

### 9.1 TriggerDesc

```c
#include "db/sql/trigger.h"

typedef struct TriggerDesc {
    NodeTag     type;
    char        *tgname;
    TriggerType tgtype;         /* TRIGGER_TYPE_INSERT 等 */
    Oid         tgrelid;        /* 触发器所属表 */
    Oid         tgfuncid;       /* 触发器函数 OID */
    int         tgnargs;        /* 参数个数 */
    Datum       *tgargs;        /* 触发器参数 */
    bool        tgenabled;      /* 是否启用 */
} TriggerDesc;

typedef enum TriggerType {
    TRIGGER_TYPE_INSERT      = 1 << 0,
    TRIGGER_TYPE_DELETE       = 1 << 1,
    TRIGGER_TYPE_UPDATE       = 1 << 2,
    TRIGGER_TYPE_BEFORE       = 1 << 3,
    TRIGGER_TYPE_AFTER        = 1 << 4,
    TRIGGER_TYPE_ROW          = 1 << 5,
    TRIGGER_TYPE_STATEMENT    = 1 << 6,
} TriggerType;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `CreateTriggerDesc(...)` | 创建触发器描述符 |
| `ExecBeforeTriggers(estate, rel, slot)` | 执行 BEFORE 触发器链 |
| `ExecAfterTriggers(estate, rel, slot)` | 执行 AFTER 触发器链 |
| `FreeTriggerDesc(desc)` | 释放触发器描述符 |

---

## 10. 分区路由

### 10.1 PartitionDesc

```c
#include "db/sql/partition.h"

typedef struct PartitionDesc {
    NodeTag             type;
    PartitionType       parttype;    /* PARTITION_TYPE_RANGE 等 */
    Oid                 relid;       /* 分区表 OID */
    int                 nparts;      /* 分区个数 */
    PartitionBoundInfo  boundinfo;   /* 分区边界信息 */
    Oid                *oids;        /* 分区 OID 数组 */
} PartitionDesc;

typedef enum PartitionType {
    PARTITION_TYPE_RANGE,
    PARTITION_TYPE_LIST,
    PARTITION_TYPE_HASH,
} PartitionType;
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `CreatePartitionDesc(relid, nparts, boundinfo)` | 创建分区描述符 |
| `PartitionRouting(desc, key_val)` | 路由到目标分区 |
| `GetPartitionByIndex(desc, idx)` | 按索引获取分区 |
| `FreePartitionDesc(desc)` | 释放分区描述符 |

---

## 11. SQL 驱动接口

### 11.1 高级 API

```c
#include "db/sql/sql_driver.h"

/**
 * 执行 DDL 语句（CREATE TABLE, DROP TABLE 等）
 * @param sql   SQL 语句
 * @param db    数据库句柄
 * @return      0 成功，非 0 失败
 */
int execute_ddl(const char *sql, void *db);

/**
 * 执行 DML/查询语句
 * @param sql   SQL 语句
 * @param db    数据库句柄
 * @return      查询结果结构体
 */
QueryResult *execute_sql(const char *sql, void *db);

/**
 * 释放查询结果
 * @param result  查询结果指针
 */
void FreeQueryResult(QueryResult *result);
```

### 11.2 QueryResult 结构

```c
typedef struct QueryResult {
    int      nrows;          /* 结果行数 */
    int      ncols;          /* 结果列数 */
    char   **colnames;       /* 列名数组 */
    char   **rows;           /* 结果行（扁平化存储） */
    /* 访问方式：rows[i * ncols + j] 表示第 i 行第 j 列 */
} QueryResult;
```

### 11.3 使用示例

```c
#include "db/sql/sql_driver.h"

int main() {
    void *db = kv_open(":memory:");

    /* 创建表 */
    execute_ddl("CREATE TABLE users (id INT, name TEXT)", db);

    /* 插入数据 */
    execute_sql("INSERT INTO users VALUES (1, 'Alice')", db);
    execute_sql("INSERT INTO users VALUES (2, 'Bob')", db);

    /* 查询数据 */
    QueryResult *result = execute_sql("SELECT * FROM users", db);
    for (int i = 0; i < result->nrows; i++) {
        printf("id=%s, name=%s\n",
               result->rows[i * result->ncols + 0],
               result->rows[i * result->ncols + 1]);
    }

    FreeQueryResult(result);
    kv_close(db);
    return 0;
}
```

---

## 12. 类型系统

### 12.1 核心类型定义

```c
#include "db/sql/sql_types.h"

typedef enum SqlType {
    SQL_TYPE_INVALID   = 0,
    SQL_TYPE_INTEGER   = 1,
    SQL_TYPE_BIGINT    = 2,
    SQL_TYPE_REAL      = 3,
    SQL_TYPE_DOUBLE    = 4,
    SQL_TYPE_TEXT      = 5,
    SQL_TYPE_BOOLEAN   = 6,
    SQL_TYPE_DATE      = 7,
    SQL_TYPE_TIMESTAMP = 8,
    SQL_TYPE_NULL      = 9,
} SqlType;
```

#### 类型转换函数

| 函数 | 说明 |
|------|------|
| `sql_type_from_oid(oid)` | OID 转 SQL 类型 |
| `sql_type_to_string(type)` | 类型转字符串 |
| `sql_type_size(type)` | 获取类型占用字节数 |

---

*API 版本：v5.0*
*更新日期：2026-07-15*
