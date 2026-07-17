# SQL 执行引擎生产级完整实现设计

**版本**：v1.0  
**日期**：2026-07-15  
**状态**：草稿  
**参考**：PostgreSQL 16/17 执行引擎架构

---

## 1. 概述

### 1.1 目标

实现一个**生产级 SQL 执行引擎**，对标 PostgreSQL 完整实现，覆盖 SELECT/INSERT/UPDATE/DELETE 所有操作，支持复杂查询（多表连接、聚合、子查询、窗口函数等）。

### 1.2 当前状态

| 维度 | 当前状态 | 参考基线 (PG) | 完成度 |
|------|---------|--------------|--------|
| 代码量 | ~1,900 行 | ~193,000 行 | **1%** |
| 核心算子 | 16 个全部为 NULL 桩 | 40+ 种节点 | **0%** |
| 表达式求值 | 9 个函数全部空桩 | ~90 个 EEOP_* | **0%** |
| 内存管理 | 无 | 4 种 allocator | **0%** |
| 并行执行 | 无 | DSM + shm_mq | **0%** |

### 1.3 工作量估算

| 阶段 | 内容 | 代码量 |
|------|------|--------|
| Phase 1 | 基础设施层 | ~8,000 行 |
| Phase 2 | 核心算子层 | ~15,000 行 |
| Phase 3 | 优化层 | ~10,000 行 |
| Phase 4 | 高级特性 | ~17,000 行 |
| **总计** | | **~50,000 行** |

---

## 2. 架构总览

```
┌─────────────────────────────────────────────────────────────────┐
│                    SQL 执行引擎完整架构                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐       │
│  │  MemoryCtx   │    │  ExprState   │    │  Volcano     │       │
│  │  (Phase 1)   │    │  (Phase 1)   │    │  (Phase 1)   │       │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘       │
│         │                   │                   │                │
│         └───────────────────┼───────────────────┘                │
│                             ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                      核心算子 (Phase 2)                   │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │   │
│  │  │ SeqScan │ │NestLoop │ │ HashAgg │ │ ModifyTable     │ │   │
│  │  │IndexScan│ │HashJoin │ │ SortAgg │ │ Append/Limit    │ │   │
│  │  │BitmapScn│ │MergeJoin│ │WindowAgg│ │ Material/Sort   │ │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                             │                                    │
│                             ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    优化层 (Phase 3)                       │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │   │
│  │  │ Cost    │ │Rules    │ │EXPLAIN  │ │ Statistics      │ │   │
│  │  │ Model   │ │Engine   │ │ANALYZE  │ │ Collector       │ │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                             │                                    │
│                             ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  高级特性 (Phase 4)                       │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │   │
│  │  │Parallel │ │Trigger  │ │Partition│ │ JIT Compilation │ │   │
│  │  │ Query   │ │         │ │ Routing │ │                 │ │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Phase 1：基础设施层

### 3.1 内存上下文子系统

**目标**：提供类似 PG 的 MemoryContext 机制，支持生命周期管理和批量释放。

**文件**：
- `engineering/include/db/sql/memctx.h`
- `engineering/src/db/sql/memctx.c`

**设计**：

```c
// 核心类型
typedef struct MemoryContextData *MemoryContext;

typedef struct MemoryContextMethods {
    void *(*alloc)(MemoryContext ctx, Size size);
    void  (*free)(MemoryContext ctx, void *ptr);
    void  (*reset)(MemoryContext ctx);
    void  (*delete)(MemoryContext ctx);
} MemoryContextMethods;

typedef struct MemoryContextData {
    NodeTag        type;
    MemoryContext  parent;
    MemoryContext  firstchild;
    MemoryContext  prevchild;
    MemoryContext  nextchild;
    MemoryContextMethods *methods;
    const char    *name;
    Size          mem_allocated;
    bool          isReset;
} MemoryContextData;

// AllocSet 实现（默认 allocator）
MemoryContext AllocSetContextCreate(
    MemoryContext parent,
    const char *name,
    Size minContextSize,    // 初始块大小
    Size initBlockSize,     // 初始块增长
    Size maxBlockSize       // 最大块增长
);

// 核心 API
void *palloc(MemoryContext ctx, Size size);
void *palloc0(MemoryContext ctx, Size size);
void pfree(MemoryContext ctx, void *ptr);
void reset_memory(MemoryContext ctx);
void delete_memory(MemoryContext ctx);

// 内存上下文层级
//   - es_query_cxt: 查询级（整个查询生命周期）
//   - es_per_tuple_exprctx: 每个元组（每次求值后 reset）
//   - per-node context: 每个算子私有
```

**关键实现点**：
1. AllocSet 按块分配，避免每次小分配都调用 malloc
2. 支持父子层级，reset 时只释放子块
3. 提供 `MemoryContextReset()` 和 `MemoryContextDelete()` 两种清理方式

### 3.2 表达式求值器

**目标**：实现类似 PG ExprState + ExprEvalStep 的字节码解释器。

**文件**：
- `engineering/include/db/sql/expr.h`
- `engineering/src/db/sql/expr.c`
- `engineering/src/db/sql/expr_interp.c`

**ExprEvalOp 操作码枚举（约 90 个）**：

```c
typedef enum ExprEvalOp {
    // ========== 控制流 (0-10) ==========
    EEOP_DONE = 0,                  // 表达式求值完成
    EEOP_JUMP,                      // 无条件跳转
    EEOP_JUMP_IF_NULL,              // NULL 时跳转
    EEOP_JUMP_IF_NOT_NULL,          // 非 NULL 时跳转
    EEOP_JUMP_IF_NOT_TRUE,          // 非 true 时跳转
    EEOP_BOOL_AND_STEP_FIRST,       // 布尔 AND 首步
    EEOP_BOOL_AND_STEP,             // 布尔 AND 后续步
    EEOP_BOOL_OR_STEP_FIRST,        // 布尔 OR 首步
    EEOP_OR_STEP,                   // 布尔 OR 后续步
    EEOP_BOOL_NOT_STEP,             // 布尔 NOT

    // ========== Var 访问 (11-25) ==========
    EEOP_INNER_VAR,                 // 内表变量
    EEOP_OUTER_VAR,                 // 外表变量
    EEOP_SCAN_VAR,                  // 扫描表变量
    EEOP_ASSIGN_INNER_VAR,          // 赋值给内表变量
    EEOP_ASSIGN_OUTER_VAR,          // 赋值给外表变量
    EEOP_ASSIGN_SCAN_VAR,           // 赋值给扫描变量

    // ========== 参数 (26-30) ==========
    EEOP_PARAM_EXEC,                // 执行期参数
    EEOP_PARAM_EXTERN,              // 外部参数
    EEOP_PARAM_CALLBACK,            // 参数回调

    // ========== 常量 (31-35) ==========
    EEOP_CONST,                     // 常量值
    EEOP_CONST_NULL,                // NULL 常量

    // ========== 运算 (36-70) ==========
    EEOP_FUNCEXPR_STANDARD,         // 标准函数调用
    EEOP_FUNCEXPR_STRICT,           // 严格函数（NULL 输入直接返回 NULL）
    EEOP_AGGREF,                    // 聚合引用
    EEOP_WINDOW_FUNC,               // 窗口函数
    EEOP_AGG_STRICT_INPUT,          // 聚合严格输入
    EEOP_AGG_PLAIN_PERGROUP,        // 聚合每组
    EEOP_WINDOW_PLACEHOLDER,        // 窗口占位符

    // ========== NULL 测试 (71-75) ==========
    EEOP_NULLTEST_ISNULL,           // IS NULL
    EEOP_NULLTEST_ISNOTNULL,        // IS NOT NULL
    EEOP_NULLTEST_ROWISNULL,        // ROW IS NULL
    EEOP_NULLTEST_ROWISNOTNULL,     // ROW IS NOT NULL

    // ========== 布尔测试 (76-80) ==========
    EEOP_BOOLTEST_IS_TRUE,          // IS TRUE
    EEOP_BOOLTEST_IS_FALSE,         // IS FALSE
    EEOP_BOOLTEST_IS_NOT_UNKNOWN,   // IS NOT UNKNOWN
    EEOP_BOOLTEST_IS_UNKNOWN,       // IS UNKNOWN

    // ========== 复杂表达式 (81-90) ==========
    EEOP_CASE_TESTVAL,              // CASE 测试值
    EEOP_ARRAYEXPR,                 // 数组表达式
    EEOP_ARRAY_subscript,           // 数组下标
    EEOP_ROW,                       // 行表达式
    EEOP_ROWCOMPARE,                // 行比较
    EEOP_DISTINCT,                  // DISTINCT
    EEOP_NOT_DISTINCT,              // NOT DISTINCT
    EEOP_NULLIF,                    // NULLIF
    EEOP_COALESCE,                  // COALESCE
    EEOP_MINMAX,                    // GREATEST/LEAST

    EEOP_MAX                        // 总计约 50 个基础操作码
} ExprEvalOp;
```

**ExprState 结构**：

```c
typedef struct ExprState {
    NodeTag        type;
    Expr          *expr;            // 原始表达式树
    ExprEvalStep  *steps;           // 编译后的操作码序列
    int            nsteps;          // 操作码数量
    ExprContext   *eval_ctx;        // 求值上下文
    Datum         *resvalue;        // 结果值
    bool          *resnull;         // 结果是否为 NULL
} ExprState;

typedef struct ExprEvalStep {
    ExprEvalOp    op;
    union {
        struct {
            int  var_varno;         // INNER/OUTER/SCAN
            int  var_attno;         // 属性号
            Datum *result;
            bool *isnull;
        } var;
        struct {
            Datum  constval;
            Oid    consttype;
            bool   constisnull;
        } const_;
        struct {
            int  jumpdest;          // 跳转目标 step 索引
        } jump;
        struct {
            FmgrInfo func;          // 函数信息
            int  nargs;             // 参数个数
            ExprState **args;       // 参数表达式
        } func;
    } d;
} ExprEvalStep;
```

**表达式编译流程**：

```
Expr 树 → ExecInitExpr() → ExprState.steps[] → ExecEvalExpr() → Datum

示例：WHERE age > 18 AND name = 'Tom'

Step 0: EEOP_SCAN_VAR (age)           → resvalue = slot->values[age_idx]
Step 1: EEOP_CONST (18)               → resvalue = 18
Step 2: EEOP_FUNCEXPR_STANDARD (>)    → resvalue = (age > 18)
Step 3: EEOP_JUMP_IF_NOT_TRUE → 7     → if (!resvalue) skip to Step 7
Step 4: EEOP_SCAN_VAR (name)          → resvalue = slot->values[name_idx]
Step 5: EEOP_CONST ('Tom')            → resvalue = 'Tom'
Step 6: EEOP_FUNCEXPR_STANDARD (=)    → resvalue = (name = 'Tom')
Step 7: EEOP_DONE                     → 返回最终结果
```

### 3.3 Volcano 迭代器框架

**目标**：修复当前的断路问题，联通 PlanState → exec_proc。

**文件**：
- `engineering/include/db/sql/executor.h`
- `engineering/include/db/sql/nodes.h`
- `engineering/src/db/sql/executor.c`
- `engineering/src/db/sql/node.c`

**PlanState 结构**：

```c
typedef struct PlanState {
    NodeTag             type;
    Plan               *plan;              // 对应的物理计划
    EState             *state;             // 共享执行器状态
    ExecProcNodeMtd     ExecProcNode;      // 虚函数：获取下一个元组
    ExecProcNodeMtd     ExecProcNodeReal;  // 真实函数（用于包装）

    // 子树
    PlanState          *lefttree;
    PlanState          *righttree;
    List               *initPlan;          // InitPlan（先执行一次）
    List               *subPlan;

    // 过滤条件
    ExprState          *qual;              // WHERE 条件
    ExprState          *recheck;           // 并行时重新检查

    // 投影
    ProjectionInfo     *ps_ProjInfo;

    // 结果
    TupleTableSlot     *ps_ResultTupleSlot;
    TupleDesc           ps_ResultTupleDesc;

    // 表达式上下文
    ExprContext        *ps_ExprContext;

    // 诊断
    struct Instrumentation *instrument;
    bool                needs_to_scan_queue;

    Bitmapset          *chgParam;           // 参数变化标志
} PlanState;

// 虚函数类型
typedef TupleTableSlot *(*ExecProcNodeMtd)(PlanState *pstate);
```

**EState 结构**：

```c
typedef struct EState {
    // 内存
    MemoryContext       es_query_cxt;        // 查询级内存上下文

    // 元组表
    List               *es_tupleTable;       // 元组槽列表
    TupleTableSlot     *es_trig_tuple_slot;  // 触发器元组槽

    // MVCC 快照
    Snapshot            es_snapshot;
    Snapshot            es_crosscheck_snapshot;
    CommandId           es_output_cid;

    // Range 表
    List               *es_range_table;      // FROM 子句
    List               *es_relations;        // 打开的 Relation
    List               *es_rowmarks;         // 行级锁

    // 计数器
    uint64              es_processed;        // 处理的元组数
    uint64              es_total_processed;

    // 表达式上下文
    List               *es_exprcontexts;     // 表达式上下文列表
    ExprContext        *es_per_tuple_exprctx;// 每个元组的临时上下文

    // 扫描方向
    int                 es_direction;        // FORWARD/BACKWARD

    // DML 目标
    List               *es_result_relations; // INSERT/UPDATE/DELETE 目标
    ResultRelInfo      *es_current_result_rel;

    // 并行
    bool                es_use_parallel_mode;
    int                 es_parallel_workers_needed;

    // JIT
    JitContext         *es_jit;
} EState;
```

**执行器四阶段接口**：

```c
// 启动阶段
void ExecutorStart(QueryDesc *queryDesc, int eflags);
// 运行阶段
void ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count);
// 结束阶段
void ExecutorFinish(QueryDesc *queryDesc);
void ExecutorEnd(QueryDesc *queryDesc);

// 核心循环
static inline TupleTableSlot *
ExecProcNode(PlanState *node) {
    CheckNodeIsValid(node);
    return node->ExecProcNode(node);
}
```

**关键修复点**：
1. `planner_create_plan_state` 中设置 `state->ExecProcNode = exec_<nodetype>;`
2. 每个算子实现自己的 `exec_<nodetype>` 函数
3. 首次调用时初始化（`ExecProcNodeFirst`）

---

## 4. Phase 2：核心算子层

### 4.1 扫描算子

#### SeqScan（顺序扫描）

**文件**：`nodeSeqscan.h`, `nodeSeqscan.c`

```c
typedef struct SeqScanState {
    PlanState      ps;
    Relation       ss_currentRelation;   // 扫描的表
    TupleTableSlot *ss_ScanTupleSlot;   // 扫描结果槽
    TableScanDesc  ss_tableScan;        // 堆扫描描述符
    ExprState      *ss_ScanKeys;        // 扫描键
    ExprState      *ss_CheckQual;       // 过滤条件
} SeqScanState;

// 核心逻辑
static TupleTableSlot *exec_seqscan(PlanState *pstate) {
    SeqScanState *node = (SeqScanState *)pstate;

    for (;;) {
        // 1. 获取下一个元组
        if (!table_scan_getnext(node->ss_tableScan, 
                                node->ss_currentRelation)) {
            return NULL;  // 扫描结束
        }

        // 2. 填充元组槽
        ExecStoreBufferHeapTuple(
            node->ss_tableScan->rs_cbuf,
            node->ss_ScanTupleSlot,
            &node->ss_currentRelation->rd_node
        );

        // 3. 检查过滤条件
        if (node->ss_CheckQual != NULL) {
            ExprContext *ectxt = node->ps.ps_ExprContext;
            ectxt->ecxt_scantuple = node->ss_ScanTupleSlot;

            bool passes = ExecCheckEvalExpr(node->ss_CheckQual, ectxt);
            if (!passes) continue;  // 不满足条件，继续
        }

        return node->ss_ScanTupleSlot;
    }
}
```

#### IndexScan（索引扫描）

**文件**：`nodeIndexscan.h`, `nodeIndexscan.c`

```c
typedef struct IndexScanState {
    ScanState       ps;
    Relation        indexRelation;       // 索引 Relation
    IndexScanDesc   indexScan;           // 索引扫描描述符
    List           *indexqual;           // 索引条件
    List           *indexqualorig;       // 原始条件
    ScanDirection   direction;           // 扫描方向
} IndexScanState;
```

### 4.2 连接算子

#### HashJoin（哈希连接）

**文件**：`nodeHashjoin.h`, `nodeHashjoin.c`, `nodeHash.c`

```c
typedef struct HashJoinState {
    PlanState       ps;
    HashJoinMode    hj_Type;             // INNER/LEFT/RIGHT/FULL/SEMI/ANTI
    Hash           *hj_HashTable;        // Hash 表
    List           *hj_JoinOperators;    // 连接条件
    List           *hj_HashOperators;    // Hash 操作符
    ExprState      *hj_JoinQual;         // Join 条件
    ExprState      *hj_HashQual;         // Hash 条件

    // 流式构建
    PlanState      *hj_OuterHashKey;
    TupleTableSlot *hj_HashTupleSlot;

    // 探测
    TupleHashEntry *hj_CurrentHashEntry;
    bool            hj_NewHashBatch;
} HashJoinState;

typedef struct HashState {
    PlanState       ps;
    HashJoinState  *hashtable;           // 关联的 HashJoinState
    ParallelHashJoinState *parallel;     // 并行状态
    int             nbuckets;            // Bucket 数
    int             nbatch;              // 批次数
    int             curbatch;            // 当前批次
    TupleHashTable  hash_tbl;            // 分组 Hash 表
} HashState;
```

**执行流程**：
1. **构建阶段**：读取外表，填充 Hash 表
2. **探测阶段**：读取内表，查找匹配
3. **溢出处理**：多批次处理大表

#### NestLoop（嵌套循环连接）

**文件**：`nodeNestloop.h`, `nodeNestloop.c`

```c
typedef struct NestLoopState {
    PlanState      ps;
    JoinType       nl_JoinType;         // INNER/LEFT/FULL
    ExprState     *nl_JoinQual;         // Join 条件
    ExprState     *nl_NestLoopParam;    // 参数化嵌套循环
    bool           nl_MatchedOuter;     // LEFT JOIN 标记
    TupleTableSlot *nl_OfferTupleSlot;
} NestLoopState;
```

### 4.3 聚合算子

#### HashAgg（哈希聚合）

**文件**：`nodeAgg.h`, `nodeAgg.c`

```c
typedef struct AggState {
    PlanState       ps;
    AggStrategy     aggstrategy;        // PLAIN/HASHED/SORTED
    int             numaggs;            // 聚合函数数量
    AggrefExprState **aggs;             // 聚合函数状态
    TupleHashTable  hash_tbl;           // 分组 Hash 表
    TupleTableSlot  *hashslots;         // 输出槽
    long            numGroups;          // 估计分组数
    int             numPartitions;      // Hash 分区数
    TupleHashIterator hashiter;         // 迭代器
    bool            agg_done;           // 完成标志
} AggState;
```

#### WindowAgg（窗口函数）

**文件**：`nodeWindowAgg.h`, `nodeWindowAgg.c`

```c
typedef struct WindowAggState {
    PlanState       ps;
    WindowObject    winobj;             // 窗口对象
    int             numWindowAggs;      // 窗口函数数量
    TupleTableSlot *shared_slot;        // 共享槽
    bool            all_done;           // 完成标志
} WindowAggState;
```

### 4.4 修饰算子

#### Sort

**文件**：`nodeSort.h`, `nodeSort.c`

```c
typedef struct SortState {
    PlanState       ps;
    TupleTableSlot *sortslot;
    Tuplesortstate *tuplesort;          // 排序状态
    bool            sort_Done;          // 排序完成
    int             sortRowCount;       // 排序行数
} SortState;
```

#### Limit

**文件**：`nodeLimit.h`, `nodeLimit.c`

```c
typedef struct LimitState {
    PlanState       ps;
    int64           offset;             // OFFSET
    int64           count;              // LIMIT
    int64           offset_count;       // 已跳过
    int64           count_count;        // 已返回
    ExprState      *limitOffset;        // OFFSET 表达式
    ExprState      *limitCount;         // LIMIT 表达式
    bool            have_row_count;
    TupleTableSlot *slot;
} LimitState;
```

### 4.5 控制算子

#### Result（结果）

**文件**：`nodeResult.h`, `nodeResult.c`

```c
typedef struct ResultState {
    PlanState       ps;
    ExprState      *resconstantqual;    // 常量条件
    TupleTableSlot *resultslot;
} ResultState;
```

#### ModifyTable（INSERT/UPDATE/DELETE）

**文件**：`nodeModifyTable.h`, `nodeModifyTable.c`

```c
typedef struct ModifyTableState {
    PlanState       ps;
    CmdType         operation;          // CMD_INSERT/UPDATE/DELETE
    List           *resultRelInfos;     // 目标表信息
    List           *mt_plans;           // 子计划
    TupleTableSlot *mt_temp_slot;
    ResultRelInfo  *mt_current_result_rel;
    int             mt_whichplan;
    List           *mt_ar_inserted_vvs;
    bool            mt_done;
} ModifyTableState;
```

---

## 5. Phase 3：优化层

### 5.1 代价模型

**文件**：`cost.h`, `cost.c`

**Selinger 风格的代价估算**：

```c
typedef struct CostParams {
    double seq_page_cost;       // 顺序读页面代价
    double random_page_cost;    // 随机读页面代价
    double cpu_tuple_cost;      // 处理每行 CPU 代价
    double cpu_index_tuple_cost;// 索引元组处理代价
    double cpu_operator_cost;   // 操作符执行代价
    double parallel_divider;    // 并行度除数
    double work_mem;            // 工作内存
    double effective_cache_size;// 有效缓存
} CostParams;

// 代价估算函数
void cost_seqscan(Path *path, RelOptInfo *rel, ParamPathInfo *param_info);
void cost_index(Path *path, RelOptInfo *rel, IndexOptInfo *index,
                List *indexqual, double loop_count);
void cost_nestloop(Path *path, Path *outer_path, Path *inner_path,
                   JoinPathExtraData *extra);
void cost_hashjoin(Path *path, JoinPathExtraData *extra);
void cost_mergejoin(Path *path, JoinPathExtraData *extra);
void cost_sort(Path *path, RelOptInfo *rel, double loop_count);
void cost_agg(Path *path, AggClauseCosts *aggcosts, double loop_count);
```

### 5.2 优化规则

**文件**：`optimizer.h`, `optimizer.c`

**规则注册表**：

```c
typedef enum OptimizerRule {
    RULE_PREDICATE_PUSHDOWN,     // ✓ 已有，需完善
    RULE_COLUMN_PRUNING,         // 列裁剪
    RULE_CONSTANT_FOLDING,       // 常量折叠
    RULE_SUBQUERY_FLATTENING,    // 子查询扁平化
    RULE_JOIN_REORDERING,        // 连接重排序
    RULE_DISTINCT_PUSHDOWN,      // DISTINCT 下推
    RULE_AGG_PUSHDOWN,           // 聚合下推
    RULE_LIMIT_PUSHDOWN,         // LIMIT 下推
    RULE_INDEX_COND_REWRITE,     // 索引条件重写
    RULE_OR_CLAUSE_SPLIT,        // OR 条件拆分
} OptimizerRule;
```

### 5.3 EXPLAIN/ANALYZE

**文件**：`explain.h`, `explain.c`

```c
typedef struct ExplainState {
    StringInfo     str;
    bool           analyze;          // 执行 ANALYZE
    bool           verbose;
    bool           costs;
    bool           buffers;
    bool           timing;
    bool           summary;
    bool           format_json;
    bool           format_yaml;
} ExplainState;

typedef struct Instrumentation {
    double         startup_time;
    double         total_time;
    uint64         tuples_out;
    uint64         buf_hit;
    uint64         buf_read;
} Instrumentation;

void ExplainQuery(Query *query, ExplainState *es);
void ExplainNode(PlanState *planstate, Plan *plan,
                 const char *relationship, const char *extra,
                 ExplainState *es);
```

---

## 6. Phase 4：高级特性

### 6.1 并行执行

**文件**：`parallel.h`, `parallel.c`, `nodeGather.h`, `nodeGather.c`

**Parallel Query 架构**：

```c
typedef struct ParallelContext {
    const char    *coord_shmem;
    const char    *worker_shmem;
    int            num_workers;
    dsa_area      *area;
    shm_mq_handle **tqueue;
} ParallelContext;

typedef struct ParallelExecutorInfo {
    ParallelContext *pcxt;
    PlanState       *planstate;
    BackgroundWorkerHandle *worker_handles[MAX_WORKER];
    int              num_workers_launched;
    BufferUsage      buffer_usage;
} ParallelExecutorInfo;

// 并行节点
typedef struct GatherState {
    PlanState           ps;
    ParallelExecutorInfo *pei;
    TupleQueueReader  **reader;
    bool                gather_done;
    int                 num_workers;
} GatherState;
```

### 6.2 触发器

**文件**：`trigger.h`, `trigger.c`

```c
typedef struct TriggerDesc {
    int         num_before_row;
    int         num_after_row;
    int         num_before_stmt;
    int         num_after_stmt;
    Trigger    *triggers;
} TriggerDesc;

void ExecBRTriggers(EState *estate, ResultRelInfo *rinfo,
                    TriggerEvent event, TupleTableSlot *slot);
void ExecARTriggers(EState *estate, ResultRelInfo *rinfo,
                    TriggerEvent event, TupleTableSlot *slot);
void ExecIRTriggers(EState *estate, ResultRelInfo *rinfo,
                    TriggerEvent event, TupleTableSlot *slot);
```

### 6.3 分区表

**文件**：`partitions.h`, `partitions.c`

```c
typedef struct PartitionDescData {
    int          nparts;
    PartitionKeyData *key;
    Oid         *oids;
    PartitionBoundData *bounds;
    List        *partitions;
} PartitionDescData;

ResultRelInfo *ExecFindPartition(ResultRelInfo *resultRelInfo,
                                  PartitionKeyData *key,
                                  TupleTableSlot *slot,
                                  EState *estate);
```

---

## 7. 文件组织

```
engineering/
├── include/db/sql/
│   ├── executor.h              # 执行器核心接口
│   ├── memctx.h                # 内存上下文
│   ├── expr.h                  # 表达式求值
│   ├── cost.h                  # 代价模型
│   ├── optimizer.h             # 优化器
│   ├── explain.h               # EXPLAIN
│   ├── parallel.h              # 并行执行
│   ├── trigger.h               # 触发器
│   ├── partitions.h            # 分区表
│   └── nodes/
│       ├── execnodes.h         # 执行器状态结构
│       └── nodetags.h          # 节点类型枚举
└── src/db/sql/
    ├── executor.c              # 执行器主入口
    ├── memctx.c                # 内存上下文实现
    ├── expr.c                  # 表达式编译
    ├── expr_interp.c           # 表达式解释器
    ├── cost.c                  # 代价估算
    ├── optimizer.c             # 优化器
    ├── explain.c               # EXPLAIN 实现
    ├── parallel.c              # 并行执行
    ├── trigger.c               # 触发器实现
    ├── partitions.c            # 分区表
    ├── node.c                  # 节点通用函数
    ├── nodeSeqscan.c           # 顺序扫描
    ├── nodeIndexscan.c         # 索引扫描
    ├── nodeHash.c              # Hash 表构建
    ├── nodeHashjoin.c          # Hash Join
    ├── nodeNestloop.c          # 嵌套循环
    ├── nodeAgg.c               # 聚合
    ├── nodeWindowAgg.c         # 窗口函数
    ├── nodeSort.c              # 排序
    ├── nodeLimit.c             # Limit
    ├── nodeResult.c            # Result
    ├── nodeModifyTable.c       # DML
    ├── nodeAppend.c            # Append
    ├── nodeMaterial.c          # Material
    ├── nodeGather.c            # Gather (并行汇流)
    └── CMakeLists.txt
```

---

## 8. 实施顺序

### Phase 1：基础设施层（约 8,000 行）
1. **内存上下文** (`memctx.c`)
   - MemoryContextData 结构
   - AllocSet 实现
   - palloc/pfree/reset API

2. **表达式求值器** (`expr.c`, `expr_interp.c`)
   - ExprEvalOp 枚举定义
   - ExprState 结构
   - ExecInitExpr 编译
   - ExecEvalExpr 解释器

3. **Volcano 框架** (`executor.c`, `node.c`)
   - PlanState/EState 结构
   - ExecutorStart/Run/Finish/End
   - ExecProcNode 虚函数机制

### Phase 2：核心算子层（约 15,000 行）
1. **扫描算子**：SeqScan → IndexScan
2. **连接算子**：NestLoop → HashJoin → MergeJoin
3. **聚合算子**：HashAgg → SortAgg → WindowAgg
4. **修饰算子**：Sort → Limit → Material
5. **控制算子**：Result → Append → ModifyTable

### Phase 3：优化层（约 10,000 行）
1. **代价模型**：8 个 cost_* 函数
2. **优化规则**：8 条规则实现
3. **EXPLAIN**：格式输出 + ANALYZE

### Phase 4：高级特性（约 17,000 行）
1. **并行执行**：Gather + 共享 Hash
2. **触发器**：BEFORE/AFTER/INSTEAD OF
3. **分区表**：路由 + 分区裁剪

---

## 9. 测试策略

### 单元测试
- 每个算子独立测试
- 表达式求值覆盖所有 EEOP_*
- 内存上下文边界测试

### 集成测试
- TPC-H Q1（聚合排序）
- TPC-H Q3/Q5（多表连接）
- TPC-H Q6（条件过滤）
- 完整 SELECT 端到端

### 基准测试
- 单表扫描性能
- Hash Join vs NestLoop 对比
- 聚合性能

---

## 10. 参考资料

- PostgreSQL 16/17 `src/backend/executor/` (72 个 .c, ~77,000 行)
- PostgreSQL `src/include/nodes/execnodes.h` (2,928 行)
- PostgreSQL `src/include/executor/execExpr.h` (~90 个 EEOP_*)
- PostgreSQL `src/backend/utils/mmgr/` (11 个 .c, ~12,654 行)

---

**文档版本**：v1.0  
**下次审查**：Phase 1 实现完成后
