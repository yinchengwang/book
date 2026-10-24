# SQL 执行引擎架构文档

**版本**：v5.0
**更新日期**：2026-07-15

---

## 1. 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                    SQL 执行引擎完整架构                          │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │  MemoryCtx   │    │  ExprState   │    │  Volcano     │     │
│  │  (Phase 1)   │    │  (Phase 1)   │    │  (Phase 1)   │     │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘     │
│         │                   │                   │              │
│         └───────────────────┼───────────────────┘              │
│                             ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                      核心算子 (Phase 2)                   │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐│  │
│  │  │ SeqScan │ │NestLoop │ │ HashAgg │ │ ModifyTable     ││  │
│  │  │IndexScan│ │HashJoin │ │ SortAgg │ │ Append/Limit    ││  │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘│  │
│  └──────────────────────────────────────────────────────────┘  │
│                             │                                  │
│                             ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    优化层 (Phase 3)                       │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐│  │
│  │  │ Cost    │ │ Rules   │ │ EXPLAIN │ │ Statistics      ││  │
│  │  │ Model   │ │ Engine  │ │ ANALYZE │ │ Collector       ││  │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘│  │
│  └──────────────────────────────────────────────────────────┘  │
│                             │                                  │
│                             ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                  高级特性 (Phase 4-5)                     │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐│  │
│  │  │Parallel │ │Trigger  │ │Partition│ │ JIT Compilation ││  │
│  │  │ Query   │ │         │ │ Routing │ │                 ││  │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘│  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 1.1 模块依赖关系

```
┌─────────────────────────────────────────────────────────────┐
│                      调用层次                                │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐   │
│  │              SQL 驱动层 (sql_driver)                 │   │
│  │         execute_sql() / execute_ddl()                │   │
│  └────────────────────────┬────────────────────────────┘   │
│                           ▼                                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │            执行器层 (executor.c)                      │   │
│  │    ExecQuery() → ExecInitNode() → ExecProcNode()    │   │
│  └────────────────────────┬────────────────────────────┘   │
│                           ▼                                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              算子层 (nodes/)                         │   │
│  │   SeqScan / HashJoin / Agg / ModifyTable 等         │   │
│  └────────────────────────┬────────────────────────────┘   │
│                           ▼                                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              存储层 (heapam / btreeam)                │   │
│  │      heap_getnext() / index_getnext()                │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Volcano 迭代器模型

### 2.1 核心概念

Volcano 模型是数据库执行引擎的经典架构，由 Goetz Graefe 在 1994 年提出。每个算子实现统一的迭代接口，父节点通过 `ExecProcNode` 从子节点拉取数据。

### 2.2 核心接口

```c
/**
 * ExecProcNode - 获取下一个元组
 *
 * 每个 PlanState 必须实现此函数指针
 *
 * @param pstate  计划节点状态
 * @return        下一个元组的 TupleTableSlot，无更多元组返回 NULL
 */
typedef TupleTableSlot *(*ExecProcNodeMtd)(PlanState *pstate);
```

### 2.3 执行流程

```
┌─────────────────────────────────────────────────────────────┐
│                    查询执行流程                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ExecQuery()                                               │
│       │                                                     │
│       ├── ExecInitNode(Plan)  // 初始化计划树             │
│       │       │                                          │
│       │       ├── ExecInitSeqScan()                       │
│       │       │       │                                    │
│       │       │       └── ExecInitHeapScan()               │
│       │       │                                              │
│       │       └── ExecInitHashJoin()                       │
│       │               │                                    │
│       │               ├── ExecInitHash()  // 构建哈希表   │
│       │               └── ExecInitSeqScan() // 探测扫描   │
│       │                                                      │
│       └── 循环:                                              │
│               │                                             │
│               ├── ExecProcNode(Gather)                      │
│               │       │                                      │
│               │       └── ExecProcNode(HashJoin)            │
│               │               │                            │
│               │               ├── ExecProcNode(SeqScan)    │
│               │               │       │                    │
│               │               │       └── heap_getnext()   │
│               │               │                            │
│               │               └── ExecProcNode(SeqScan)    │
│               │                       │                    │
│               │                       └── heap_getnext()   │
│               │                                             │
│               └── 结果元组 → 返回给客户端                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.4 PlanState 树结构

```c
/**
 * PlanState - 计划节点运行时状态
 *
 * 每个执行算子都有对应的 PlanState，组成树形结构
 */
typedef struct PlanState {
    NodeTag          type;          /* 节点类型枚举 */
    Plan            *plan;          /* 对应的计划节点 */
    EState          *state;         /* 执行器全局状态 */
    ExecProcNodeMtd  ExecProcNode;  /* 迭代器函数指针 */

    /* 子节点指针 */
    PlanState       *lefttree;      /* 左子树 */
    PlanState       *righttree;     /* 右子树 */

    /* 表达式状态 */
    ExprState       *qual;          /* WHERE 条件 */
    ExprState       *target;        /* SELECT 表达式 */

    /* 结果槽位 */
    TupleTableSlot  *ps_ResultTupleSlot;

    /* 扫描状态（扫描节点专用） */
    TupleTableSlot  *ps_ScanTupleSlot;

    /* 诊断信息 */
    struct Instrumentation *instrument;  /* 性能统计 */
} PlanState;
```

### 2.5 TupleTableSlot 机制

```c
/**
 * TupleTableSlot - 元组容器
 *
 * 封装元组数据，提供统一访问接口
 */
typedef struct TupleTableSlot {
    NodeTag           type;
    TupleDesc         tts_tupleDescriptor;  /* 元组描述 */
    HeapTuple         tts_tuple;           /* 物理元组 */
    Datum            *tts_values;           /* 属性值数组 */
    bool              *tts_isnull;          /* NULL 标记数组 */
    int               tts_nvalid;           /* 有效属性数 */
} TupleTableSlot;

/* 常用操作 */
TupleTableSlot *MakeTupleTableSlot(void);
void ExecStoreTuple(HeapTuple tuple, TupleTableSlot *slot);
Datum GetAttributeBySlot(TupleTableSlot *slot, int attrno);
```

---

## 3. 内存管理

### 3.1 MemoryContext 层级

```
┌─────────────────────────────────────────────────────────────┐
│                   MemoryContext 层级结构                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  TopMemoryContext (进程级，永久)                            │
│      │                                                       │
│      ├── CacheMemoryContext (缓存，永久)                     │
│      │       │                                              │
│      │       └── 各种 cache 上下文                           │
│      │                                                        │
│      └── MessageContext (消息级)                            │
│              │                                               │
│              └── TransactionMemoryContext (事务级)           │
│                      │                                       │
│                      └── QueryMemoryContext (查询级)         │
│                              │                               │
│                              ├── ExprContext (表达式级)     │
│                              │       │                       │
│                              │       └── 表达式求值临时      │
│                              │                                │
│                              ├── TupleTableSlot (元组级)    │
│                              │       │                        │
│                              │       └── 元组数据            │
│                              │                                │
│                              └── OperatorContext (算子级)    │
│                                      │                        │
│                                      └── HashJoin/HashAgg    │
│                                                                     │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 生命周期管理

| 上下文 | 生命周期 | 用途 |
|--------|---------|------|
| TopMemoryContext | 进程级 | 顶层根上下文 |
| CacheMemoryContext | 进程级 | 系统缓存 |
| MessageContext | 消息级 | 单条协议消息 |
| TransactionMemoryContext | 事务级 | 单个事务 |
| QueryMemoryContext | 查询级 | 单条查询 |
| ExprContext | 元组级 | 表达式求值 |
| OperatorContext | 算子级 | 单个算子执行 |

### 3.3 AllocSet 实现

```c
/**
 * AllocSetContext - 内存分配器
 *
 * 基于 slabs 的内存分配器，支持多块大小
 */
typedef struct AllocSetContext {
    MemoryContextData   header;      /* 公共头部 */
    Size                minContextSize;  /* 最小块大小 */
    Size                initBlockSize;  /* 初始块大小 */
    Size                maxBlockSize;   /* 最大块大小 */
   链表               blocks;      /* 已分配块链表 */
    AllocFreeList      freecache[NLISTS];  /* 空闲块缓存 */
} AllocSetContext;

/* 核心操作 */
MemoryContext AllocSetContextCreate(
    MemoryContext parent,     /* 父上下文 */
    const char *name,        /* 上下文名称 */
    Size minContextSize,     /* 最小块 */
    Size initBlockSize,       /* 初始块 */
    Size maxBlockSize         /* 最大块 */
);

void MemoryContextDelete(MemoryContext context);
void *palloc(MemoryContext context, Size size);
void *palloc0(MemoryContext context, Size size);
void MemoryContextReset(MemoryContext context);
```

### 3.4 内存使用示例

```c
/* 创建子上下文 */
MemoryContext old = MemoryContextSwitchTo(ctx);
{
    /* 在子上下文中分配内存 */
    MyData *data = palloc(ctx, sizeof(MyData));

    /* 临时操作 */
    process_data(data);
} /* 作用域结束时 */
MemoryContextSwitchTo(old);

/* 批量释放子上下文 */
MemoryContextDelete(ctx);  /* 释放 ctx 及所有子上下文 */
```

---

## 4. 表达式求值

### 4.1 字节码解释器架构

```
┌─────────────────────────────────────────────────────────────┐
│                    表达式求值流程                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  SQL: WHERE salary > 5000 AND dept = 'Sales'                 │
│                                                             │
│       ┌─────────────────┐                                   │
│       │     Expr        │  ← 解析器输出                    │
│       │   (树形结构)     │                                   │
│       └────────┬────────┘                                   │
│                │                                            │
│                ▼ 编译                                        │
│       ┌─────────────────┐                                   │
│       │   ExprState     │  ← 执行器使用                    │
│       │   (字节码序列)   │                                   │
│       └────────┬────────┘                                   │
│                │                                            │
│                ▼ 执行                                        │
│       ┌─────────────────┐                                   │
│       │  ExprEvalSteps  │  ← 解释器执行                    │
│       │  [0] SCAN_VAR(0) │                                   │
│       │  [1] CONST(5000) │                                   │
│       │  [2] COMPARE(>)  │                                   │
│       │  [3] SCAN_VAR(1) │                                   │
│       │  [4] CONST(...)  │                                   │
│       │  [5] COMPARE(=)  │                                   │
│       │  [6] AND         │                                   │
│       └─────────────────┘                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 ExprEvalStep 操作码

```c
/**
 * ExprEvalOp - 表达式字节码操作码
 */
typedef enum ExprEvalOp {
    /* 变量访问 */
    EEOP_SCAN_VAR,           /* 读取扫描元组的变量 */
    EEOP_SCAN_FETCHVAR,      /* 取变量值 */
    EEOP_PARAM_EXEC,         /* 执行器参数 */
    EEOP_PARAM_CACHE,

    /* 常量 */
    EEOP_CONST,              /* 加载常量 */
    EEOP_CONST_FEREFERENCE,  /* 常量引用 */

    /* 算术运算 */
    EEOP_ADD,                /* 加法 */
    EEOP_SUB,                /* 减法 */
    EEOP_MUL,                /* 乘法 */
    EEOP_DIV,                /* 除法 */
    EEOP_MOD,                /* 取模 */

    /* 比较运算 */
    EEOP_COMPARE,            /* 比较 */
    EEOP_EQ,                 /* 等于 */
    EEOP_NE,                 /* 不等于 */
    EEOP_LT,                 /* 小于 */
    EEOP_LE,                 /* 小于等于 */
    EEOP_GT,                 /* 大于 */
    EEOP_GE,                 /* 大于等于 */

    /* 逻辑运算 */
    EEOP_AND,                /* 逻辑与 */
    EEOP_OR,                 /* 逻辑或 */
    EEOP_NOT,                /* 逻辑非 */

    /* 类型转换 */
    EEOP_CAST,               /* 类型转换 */

    /* 控制流 */
    EEOP_JUMP,               /* 无条件跳转 */
    EEOP_JUMP_IF_TRUE,       /* 条件跳转-真 */
    EEOP_JUMP_IF_FALSE,      /* 条件跳转-假 */

    /* 函数调用 */
    EEOP_FUNCEXPR,           /* 函数表达式 */
    EEOP_FUNCEXPR_STRICT,    /* 严格函数调用 */

    /* 聚合 */
    EEOP_AGGREF,             /* 聚合引用 */
    EEOP_WINDOW_FUNC,        /* 窗口函数 */

    /* 结束 */
    EEOP_DONE                /* 表达式求值完成 */
} ExprEvalOp;
```

### 4.3 表达式编译流程

```c
/**
 * ExecInitExpr - 编译表达式为字节码
 *
 * @param expr   表达式节点
 * @param estate 执行状态
 * @return       编译后的表达式状态
 */
ExprState *ExecInitExpr(Expr *expr, EState *estate)
{
    ExprState *state;
    ExprEvalStep *steps;

    /* 计算需要的步数 */
    int nsteps = estimate_expr_steps(expr);

    /* 分配状态 */
    state = palloc0(estate->es_query_cxt, sizeof(ExprState));
    state->expr = expr;
    state->nsteps = nsteps;
    state->steps = steps = palloc(estate->es_query_cxt,
                                  nsteps * sizeof(ExprEvalStep));

    /* 编译表达式树 */
    compile_expr(expr, steps, 0);

    return state;
}
```

### 4.4 表达式求值循环

```c
/**
 * ExecEvalExpr - 求值表达式
 *
 * @param state     编译后的表达式状态
 * @param econtext  表达式上下文
 * @return          求值结果 Datum
 */
Datum ExecEvalExpr(ExprState *state, ExprContext *econtext)
{
    Datum result;
    ExprEvalStep *steps = state->steps;
    ExprEvalOp *op;

    for (int i = 0; i < state->nsteps; i++) {
        op = &steps[i].op;

        switch (op) {
            case EEOP_SCAN_VAR:
                /* 读取变量 */
                result = slot->tts_values[op->var.varattno];
                break;

            case EEOP_CONST:
                /* 加载常量 */
                result = op->constval.value;
                break;

            case EEOP_ADD:
                /* 加法 */
                arg1 = POP_DATUM();
                arg2 = POP_DATUM();
                PUSH_DATUM(arg1 + arg2);
                break;

            case EEOP_COMPARE:
                /* 比较 */
                arg1 = POP_DATUM();
                arg2 = POP_DATUM();
                PUSH_DATUM(arg1 OP_COMPARE arg2);
                break;

            case EEOP_AND:
                /* 短路求值 */
                if (!POP_BOOL()) {
                    PUSH_BOOL(false);
                    /* 跳过剩余 AND 操作数 */
                    skip_to_matching_and();
                }
                break;

            case EEOP_DONE:
                return POP_DATUM();

            default:
                /* 扩展操作码 */
                result = eval_extended_op(op, econtext);
        }
    }

    return POP_DATUM();
}
```

---

## 5. 核心算子

### 5.1 扫描算子

```
┌─────────────────────────────────────────────────────────────┐
│                      扫描算子体系                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                      ┌───────────┐                         │
│                      │   Scan    │                         │
│                      │ (基类)    │                         │
│                      └─────┬─────┘                         │
│                            │                                │
│              ┌─────────────┼─────────────┐                  │
│              │             │             │                  │
│              ▼             ▼             ▼                  │
│        ┌──────────┐  ┌───────────┐  ┌──────────┐           │
│        │ SeqScan │  │IndexScan  │  │BitmapScan│           │
│        │ 全表扫描 │  │索引扫描   │  │位图扫描  │           │
│        └──────────┘  └───────────┘  └──────────┘           │
│                                                             │
│              ┌─────────────┬─────────────┐                  │
│              │             │             │                  │
│              ▼             ▼             ▼                  │
│        ┌──────────┐  ┌───────────┐  ┌──────────┐           │
│        │ TidScan  │  │SubqueryScan│  │FuncScan │           │
│        │CTID扫描  │  │子查询扫描  │  │函数扫描  │           │
│        └──────────┘  └───────────┘  └──────────┘           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 连接算子

```
┌─────────────────────────────────────────────────────────────┐
│                      连接算子实现                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    NestLoop                         │    │
│  │                                                     │    │
│  │  for each outer_row:                               │    │
│  │      for each inner_row:                           │    │
│  │          if match(outer, inner):                    │    │
│  │              output(outer, inner)                  │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    HashJoin                         │    │
│  │                                                     │    │
│  │  Phase 1 (Build):                                   │    │
│  │      for each inner_row:                           │    │
│  │          hash_table.insert(build_key, inner_row)   │    │
│  │                                                     │    │
│  │  Phase 2 (Probe):                                   │    │
│  │      for each outer_row:                           │    │
│  │          bucket = hash_table.lookup(probe_key)     │    │
│  │          for each row in bucket:                   │    │
│  │              if match(outer, row):                 │    │
│  │                  output(outer, row)                │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    MergeJoin                        │    │
│  │                                                     │    │
│  │  Phase 1: Sort both inputs                         │    │
│  │  Phase 2: Merge scan                              │    │
│  │      while not at end:                            │    │
│  │          compare keys                              │    │
│  │          advance smaller key                       │    │
│  │          if match: output                          │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.3 聚合算子

```
┌─────────────────────────────────────────────────────────────┐
│                      聚合算子实现                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              AGG_PLAIN (无 GROUP BY)               │    │
│  │                                                     │    │
│  │  while (tuple = get_next()):                       │    │
│  │      aggregate.update(tuple)                       │    │
│  │  return aggregate.result()                         │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              AGG_SORTED (排序聚合)                  │    │
│  │                                                     │    │
│  │  sorted_input = sort(input, group_cols)             │    │
│  │  while (tuple = get_next()):                       │    │
│  │      if new_group(tuple):                         │    │
│  │          output(prev_result)                       │    │
│  │          aggregate.reset()                        │    │
│  │      aggregate.update(tuple)                       │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              AGG_HASHED (哈希聚合)                  │    │
│  │                                                     │    │
│  │  hash_table = {}                                    │    │
│  │  while (tuple = get_next()):                       │    │
│  │      key = hash(tuple.group_cols)                 │    │
│  │      if key not in hash_table:                    │    │
│  │          hash_table[key] = new_agg_state()        │    │
│  │      hash_table[key].update(tuple)                 │    │
│  │  for each agg_state in hash_table:                 │    │
│  │      output(agg_state.result())                    │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. 并行执行

### 6.1 并行架构

```
┌─────────────────────────────────────────────────────────────┐
│                      并行执行架构                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    Coordinator                     │    │
│  │                  (主线程控制)                        │    │
│  └─────────────────────────┬───────────────────────────┘    │
│                            │                                 │
│                            │ LaunchWorkers                   │
│                            ▼                                 │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              ParallelContext                        │    │
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐       │    │
│  │  │ Worker 0  │ │ Worker 1  │ │ Worker 2  │       │    │
│  │  │ (线程)    │ │ (线程)    │ │ (线程)    │       │    │
│  │  │           │ │           │ │           │       │    │
│  │  │ ExecSeq   │ │ ExecSeq   │ │ ExecSeq   │       │    │
│  │  │ Scan()    │ │ Scan()    │ │ Scan()    │       │    │
│  │  │           │ │           │ │           │       │    │
│  │  │  ↓ Queue  │ │  ↓ Queue  │ │  ↓ Queue  │       │    │
│  │  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘       │    │
│  └────────┼─────────────┼─────────────┼─────────────┘    │
│           │             │             │                   │
│           └─────────────┼─────────────┘                   │
│                         ▼                                    │
│           ┌─────────────────────────┐                       │
│           │    TupleQueue (共享)    │                       │
│           │   (无锁环形缓冲区)       │                       │
│           │                         │                       │
│           │  head ────────── tail   │                       │
│           │  [slot][slot][slot]...  │                       │
│           └─────────────────────────┘                       │
│                            │                                 │
│                            ▼  Gather                         │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              主线程消费 TupleQueue                    │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 TupleQueue 实现

```c
/**
 * TupleQueue - 无锁生产者-消费者队列
 *
 * 使用原子操作实现多线程安全
 */
typedef struct TupleQueue {
    _Atomic(size_t)    head;       /* 消费者读取位置 */
    _Atomic(size_t)    tail;       /* 生产者写入位置 */
    size_t             capacity;   /* 队列容量 */
    TupleTableSlot    *slots[];    /* 槽位数组 */
} TupleQueue;

/* 入队操作（生产者线程） */
bool TupleQueueEnqueue(TupleQueue *queue, TupleTableSlot *slot)
{
    size_t tail = atomic_load(&queue->tail);
    size_t next_tail = (tail + 1) % queue->capacity;

    if (next_tail == atomic_load(&queue->head)) {
        /* 队列满，需要等待 */
        return false;
    }

    queue->slots[tail] = slot;
    atomic_store(&queue->tail, next_tail);
    return true;
}

/* 出队操作（消费者线程） */
TupleTableSlot *TupleQueueDequeue(TupleQueue *queue)
{
    size_t head = atomic_load(&queue->head);

    if (head == atomic_load(&queue->tail)) {
        /* 队列空 */
        return NULL;
    }

    TupleTableSlot *slot = queue->slots[head];
    atomic_store(&queue->head, (head + 1) % queue->capacity);
    return slot;
}
```

### 6.3 并行 HashJoin

```
┌─────────────────────────────────────────────────────────────┐
│                    并行 HashJoin                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                    全局哈希分区                       │    │
│  │                                                     │    │
│  │   输入元组 → Hash(key) → Partition N                │    │
│  │                                                     │    │
│  │   Partition 0  ┌──────────────────────────┐       │    │
│  │   Partition 1  │ Worker 0: HashJoin       │       │    │
│  │   Partition 2  │ Worker 1: HashJoin       │       │    │
│  │   ...          │ Worker N: HashJoin       │       │    │
│  │   Partition N  └──────────────────────────┘       │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  Phase 1: 构建阶段                                          │
│      - 所有 worker 并行读取 build 表                        │
│      - 按哈希分区写入共享哈希表                              │
│      - 每个分区独立构建                                     │
│                                                             │
│  Phase 2: 探测阶段                                          │
│      - 每个 worker 处理部分 probe 数据                      │
│      - 只访问本地分区的哈希桶                                │
│      - 无锁并发探测                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. 触发器机制

### 7.1 触发器执行时机

```
┌─────────────────────────────────────────────────────────────┐
│                    触发器执行流程                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  INSERT 语句:                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  1. BEFORE INSERT 触发器                             │    │
│  │     └─ 可以修改元组数据、拒绝插入                     │    │
│  │  2. 检查约束                                         │    │
│  │  3. 插入元组到表                                     │    │
│  │  4. AFTER INSERT 触发器                              │    │
│  │     └─ 仅可读、可记录日志                            │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  UPDATE 语句:                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  1. BEFORE UPDATE 触发器                             │    │
│  │     └─ 可以修改新元组数据                            │    │
│  │  2. 获取旧元组、构造新元组                           │    │
│  │  3. 检查约束                                         │    │
│  │  4. 更新元组                                         │    │
│  │  5. AFTER UPDATE 触发器                              │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  DELETE 语句:                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  1. BEFORE DELETE 触发器                            │    │
│  │     └─ 可阻止删除                                   │    │
│  │  2. 获取待删除元组（用于触发器）                     │    │
│  │  3. 删除元组                                        │    │
│  │  4. AFTER DELETE 触发器                             │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 触发器数据结构

```c
/**
 * TriggerDesc - 触发器描述符
 *
 * 存储表的触发器元数据
 */
typedef struct TriggerDesc {
    NodeTag         type;

    /* 触发器基本信息 */
    char           *tgname;         /* 触发器名称 */
    Oid             tgrelid;        /* 所属表 OID */
    Oid             tgfuncid;       /* 触发器函数 OID */
    TriggerType     tgtype;         /* 触发器类型 */
    int             tgnargs;        /* 参数个数 */
    Datum          *tgargs;          /* 触发器参数 */

    /* 触发器链 */
    List           *triggers_before;   /* BEFORE 触发器列表 */
    List           *triggers_after;     /* AFTER 触发器列表 */

    /* 行级 vs 语句级 */
    bool            tg_row;          /* 是否为行级触发器 */

    /* 启用状态 */
    bool            tgenabled;       /* 是否启用 */
} TriggerDesc;

/**
 * TriggerData - 触发器执行时传入的数据
 */
typedef struct TriggerData {
    NodeTag         type;
    TriggerEvent    tg_event;        /* 触发事件 */
    Relation        tg_relation;    /* 触发表 */
    HeapTuple        tg_trigtuple;   /* 触发元组（行级） */
    HeapTuple        tg_newtuple;    /* 新元组（INSERT/UPDATE） */
    TriggerDesc     *tg_trigger;     /* 触发器描述 */
} TriggerData;

/**
 * TriggerFunc - 触发器函数签名
 *
 * @param trig_data  触发器执行上下文
 * @return           返回 NULL 或修改后的元组
 */
typedef HeapTuple (*TriggerFunc)(TriggerData *trig_data);
```

---

## 8. 分区路由

### 8.1 分区类型

```
┌─────────────────────────────────────────────────────────────┐
│                    分区策略                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  RANGE 分区 (范围分区)                              │    │
│  │                                                     │    │
│  │  PARTITION BY RANGE (sale_date)                    │    │
│  │    PARTITION p1 VALUES LESS THAN ('2024-01-01')  │    │
│  │    PARTITION p2 VALUES LESS THAN ('2024-07-01')  │    │
│  │    PARTITION p3 VALUES LESS THAN (MAXVALUE)       │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  LIST 分区 (列表分区)                               │    │
│  │                                                     │    │
│  │  PARTITION BY LIST (region)                        │    │
│  │    PARTITION p_north VALUES IN ('北京', '天津')    │    │
│  │    PARTITION p_south VALUES IN ('广州', '深圳')   │    │
│  │    PARTITION p_other VALUES IN (DEFAULT)           │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  HASH 分区 (哈希分区)                                │    │
│  │                                                     │    │
│  │  PARTITION BY HASH (user_id)                       │    │
│  │    PARTITIONS 4                                    │    │
│  │                                                     │    │
│  │  partition = hash(user_id) % num_partitions       │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 8.2 分区路由流程

```
┌─────────────────────────────────────────────────────────────┐
│                    分区路由流程                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  INSERT INTO partitioned_table VALUES (...)                 │
│                      │                                      │
│                      ▼                                      │
│  ┌─────────────────────────────────────────────────────┐    │
│  │        CreatePartitionRoutingCtx()                  │    │
│  │        创建分区路由上下文                            │    │
│  └────────────────────────┬────────────────────────────┘    │
│                           ▼                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │        PartitionRouting()                           │    │
│  │                                                     │    │
│  │  switch (parttype) {                               │    │
│  │      case RANGE:                                    │    │
│  │          for each bound in bounds:                 │    │
│  │              if key < bound: return partition_idx   │    │
│  │          return last_partition;                    │    │
│  │      case LIST:                                     │    │
│  │          for each list in lists:                   │    │
│  │              if key IN list: return partition_idx  │    │
│  │          return default_partition;                  │    │
│  │      case HASH:                                     │    │
│  │          return hash(key) % num_partitions;       │    │
│  │  }                                                  │    │
│  └────────────────────────┬────────────────────────────┘    │
│                           ▼                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │        ExecInitModifyTable(target_partition)       │    │
│  │        初始化目标分区的修改操作                      │    │
│  └────────────────────────┬────────────────────────────┘    │
│                           ▼                                  │
│                      执行 INSERT                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 8.3 PartitionDesc 结构

```c
/**
 * PartitionDesc - 分区描述符
 */
typedef struct PartitionDesc {
    NodeTag             type;

    /* 分区类型 */
    PartitionType       parttype;

    /* 所属表 */
    Oid                 relid;

    /* 分区信息 */
    int                 nparts;           /* 分区数量 */
    Oid                *oids;              /* 分区 OID 数组 */

    /* 边界信息 */
    PartitionBoundInfo  boundinfo;          /* 分区边界 */

    /* 范围分区边界 */
    Datum              *range_min;         /* 范围下界 */
    Datum              *range_max;         /* 范围上界 */

    /* 列表分区值 */
    Datum              *list_values;       /* 列表值数组 */
    int                *list_idx;          /* 值到分区的映射 */

    /* 默认分区 */
    Oid                 default_part_oid; /* 默认分区 OID */
} PartitionDesc;

/**
 * PartitionBoundInfo - 分区边界信息
 */
typedef struct PartitionBoundInfoData {
    int                 nparts;            /* 分区数 */
    int                *indexes;           /* 分区索引 */
    Bitmapset          *null_index;        /* NULL 值分区 */

    /* RANGE 分区边界 */
    Datum               lower_bound[];     /* 下界数组 */
    Datum               upper_bound[];     /* 上界数组 */
} PartitionBoundInfo;
```

---

## 9. 优化与诊断

### 9.1 EXPLAIN 输出格式

```sql
-- 简单输出
EXPLAIN SELECT * FROM users WHERE id > 100;
                    QUERY PLAN
--------------------------------------------------
Seq Scan on users  (cost=0.00..35.50 rows=810 width=36)
  Filter: (id > 100)

-- 详细输出（含缓冲区统计）
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM users;
                   QUERY PLAN
--------------------------------------------------------------
Seq Scan on users  (cost=0.00..35.50 rows=810 width=36)
  (actual time=0.015..0.156 rows=810 loops=1)
  Buffers: shared hit=35 read=0
```

### 9.2 性能统计信息

```c
/**
 * Instrumentation - 算子性能统计
 */
typedef struct Instrumentation {
    /* 执行统计 */
    long long   num_executions;    /* 执行次数 */
    double       total_time;        /* 总耗时（微秒） */
    double       min_time;          /* 最小耗时 */
    double       max_time;          /* 最大耗时 */

    /* 元组统计 */
    long long   rows_fetched;       /* 输出的元组数 */

    /* 缓冲区统计 */
    long long   buf_hit;            /* 缓冲区命中 */
    long long   buf_read;           /* 缓冲区读取 */
} Instrumentation;
```

### 9.3 成本模型

```
┌─────────────────────────────────────────────────────────────┐
│                    成本估算公式                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  SeqScan 成本:                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  cost_seqscan =                                     │    │
│  │      seq_page_cost * npages +                       │    │
│  │      cpu_tuple_cost * ntups                         │    │
│  │                                                     │    │
│  │  其中:                                              │    │
│  │    seq_page_cost = 1.0      (可配置)                │    │
│  │    cpu_tuple_cost = 0.01    (可配置)                │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
│  HashJoin 成本:                                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  cost_hashjoin =                                   │    │
│  │      cost_hash(build) +                             │    │
│  │      cost_hashjoin_probe                            │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

*架构版本：v5.0*
*更新日期：2026-07-15*
