# Executor 执行器

## 学习目标

- 理解 PostgreSQL Volcano 火山模型执行器的核心架构
- 掌握 TupleTableSlot、ExprContext、ExprState 的作用与协作
- 熟悉 ExecProcNode 递归调用、并行执行、JIT 编译优化

## 核心概念

- **Volcano Model**：迭代器模型，每个节点实现 `ExecProcNode` 接口
- **TupleTableSlot（TTS）**：存储一行数据的槽位
- **ExprContext**：表达式执行上下文（参数、内存、快照）
- **ExprState**：编译后的表达式状态
- **ExecProcNode**：节点执行函数指针
- **PlanState**：运行时的 Plan 状态
- **Parallel Executor**：并行执行框架（Gather / Gather Merge）

## 火山模型架构

PG 的执行器采用经典的 Volcano 火山模型，每个节点实现统一接口：

```mermaid
graph TD
    A[ExecutorRun] --> B[ExecutePlan]
    B --> C[ExecProcNode<br/>根节点]

    C --> D[ExecProcNode<br/>子节点]
    D --> E[ExecProcNode<br/>叶子节点]

    E --> F[返回 TupleTableSlot<br/>或 NULL]
```

**核心接口**：

```c
// 每个节点实现此接口
TupleTableSlot *ExecProcNode(PlanState *node);
```

## Plan 节点与 PlanState

```mermaid
graph TD
    A[Plan 节点<br/>静态描述] --> B[PlanState<br/>运行时状态]

    B --> C[type: NodeTag]
    B --> D[lefttree: PlanState*]
    B --> E[righttree: PlanState*]
    B --> F[qual: List]
    B --> G[targetlist: List]
    B --> H[ps_ExprContext]
    B --> I[ps_ResultTupleSlot]

    A --> A1[targetlist: List<br/>输出列表]
    A --> A2[qual: List<br/>过滤条件]
    A --> A3[lefttree: Plan*<br/>左子树]
    A --> A4[righttree: Plan*<br/>右子树]
```

## TupleTableSlot（TTS）

TTS 是执行器中的"一行数据容器"：

```mermaid
graph LR
    A[TupleTableSlot] --> B[tts_values: Datum*<br/>列值数组]
    A --> C[tts_isnull: bool*<br/>NULL 标志]
    A --> D[tts_nvalid: int<br/>有效列数]
    A --> E[tts_tuple: HeapTuple<br/>物理行指针]
    A --> F[tts_tupleDescriptor<br/>TupleDesc]

    G[三种实现] --> H[BufferHeapTupleSlot<br/>从 Buffer 来]
    G --> I[HeapTupleSlot<br/>内存中的 Tuple]
    G --> J[MinimalTupleSlot<br/>最小化]
```

**生命周期**：

```mermaid
sequenceDiagram
    participant Exec as 执行节点
    participant Slot as TupleTableSlot
    participant Parent as 父节点

    Exec->>Slot: ExecProcNode()
    Exec->>Exec: 扫描/计算
    Exec->>Slot: 填充 tts_values
    Slot-->>Exec: 返回 Slot
    Exec-->>Parent: Slot 指针
    Parent->>Parent: 读取 tts_values[i]
```

## ExprContext 表达式上下文

每个 PlanState 有一个 `ps_ExprContext`，存储表达式执行所需环境：

```mermaid
graph TD
    A[ExprContext] --> B[ecxt_scantuple<br/>当前扫描行]
    A --> C[ecxt_innertuple<br/>内表行（Join）]
    A --> D[ecxt_outertuple<br/>外表行（Join）]
    A --> E[ecxt_per_tuple_memory<br/>行级内存上下文]
    A --> F[ecxt_param_list<br/>参数列表]
    A --> G[ecxt_aggvalues<br/>聚合中间值]

    H[表达式计算] --> I[ExecEvalExpr<br/>ExprState]
    I --> A
```

## ExprState 表达式状态

ExprState 是编译后的表达式：

```mermaid
graph TD
    A[ExprState] --> B[expr: Expr*<br/>原始表达式]
    A --> C[evalfunc: ExprStateEvalFunc<br/>计算函数指针]
    A --> D[resulttype: Oid<br/>返回类型]
    A --> E[flags: int<br/>标志位]

    F[表达式类型] --> G[OpExpr<br/>操作符]
    F --> H[Var<br/>列引用]
    F --> I[Const<br/>常量]
    F --> J[FuncExpr<br/>函数调用]
    F --> K[BoolExpr<br/>AND/OR/NOT]
```

## 执行流程详解

### SeqScan 执行

```mermaid
sequenceDiagram
    participant Parent as 父节点
    participant SeqScan as SeqScanState
    participant Scan as Scan
    participant Buffer as Buffer Pool
    participant Heap as Heap Page

    Parent->>SeqScan: ExecProcNode()
    SeqScan->>Scan: heap_getnext()
    Scan->>Buffer: ReadBuffer()
    Buffer->>Heap: 获取页面
    Heap-->>Scan: HeapTuple
    Scan->>Scan: 检查可见性
    Scan-->>SeqScan: TupleTableSlot
    SeqScan-->>Parent: 返回一行
```

### IndexScan 执行

```mermaid
sequenceDiagram
    participant Parent as 父节点
    participant IndexScan as IndexScanState
    participant Index as BTree Index
    participant Heap as Heap Table

    Parent->>IndexScan: ExecProcNode()
    IndexScan->>Index: index_getnext()
    Index->>Index: 扫描索引页
    Index-->>IndexScan: TID
    IndexScan->>Heap: heap_fetch(TID)
    Heap-->>IndexScan: HeapTuple
    IndexScan->>IndexScan: 检查可见性
    IndexScan-->>Parent: TupleTableSlot
```

### Hash Join 执行

```mermaid
sequenceSequence
    participant Parent as 父节点
    participant HJ as HashJoinState
    participant Hash as HashState
    participant Outer as 外表扫描
    participant Inner as 内表扫描

    Parent->>HJ: ExecHashJoin()

    Note over HJ: Build Phase
    HJ->>Inner: ExecProcNode() 内表行
    Inner-->>HJ: Tuple
    HJ->>Hash: 插入 Hash Table

    Note over HJ: Probe Phase
    HJ->>Outer: ExecProcNode() 外表行
    Outer-->>HJ: Tuple
    HJ->>Hash: Hash 查找匹配
    Hash-->>HJ: 匹配行
    HJ-->>Parent: 连接结果
```

```mermaid
flowchart TD
    A[Hash Join] --> B[Build<br/>内表构建 Hash Table]
    B --> C[Probe<br/>外表探测 Hash Table]
    C --> D[输出匹配行]

    B --> E[ExecHashJoin<br/>HJ_BUILD_INNER]
    E --> F[扫描内表]
    F --> G[ExecHash<br/>插入 Hash Table]

    C --> H[ExecHashJoin<br/>HJ_NEED_NEW_OUTER]
    H --> I[扫描外表]
    I --> J[计算 Hash 值]
    J --> K[ExecHashJoin<br/>HJ_SCAN_BUCKET]
    K --> L[遍历 Bucket]
    L --> M{qual 匹配?}
    M -->|是| N[返回结果]
    M -->|否| L
```

## 并行执行

PG 9.6+ 支持并行查询：

```mermaid
graph TD
    A[Gather 节点] --> B[Worker 1]
    A --> C[Worker 2]
    A --> D[Worker N]

    B --> E[Partial SeqScan]
    C --> F[Partial SeqScan]
    D --> G[Partial SeqScan]

    E --> H[Tuple Queue]
    F --> H
    G --> H

    H --> I[Leader 收集]
    I --> J[返回结果]
```

**并行执行关键结构**：

```mermaid
graph LR
    A[ParallelContext] --> B[nworkers<br/>Worker 数量]
    A --> C[pcxt_parallel<br/>共享内存]
    A --> D[toc<br/>共享状态]

    E[ParallelWorkerMain] --> F[Worker 入口]
    F --> G[ExecParallelWorker]
    G --> H[执行 Partial Plan]
```

## JIT 编译

PG 11+ 支持 LLVM JIT 编译：

```mermaid
flowchart TD
    A[ExprState] --> B{JIT 启用?}
    B -->|是| C[llvm_compile_expr]
    B -->|否| D[解释执行]

    C --> E[LLVM IR 生成]
    E --> F[编译为机器码]
    F --> G[函数指针替换]
    G --> H[直接调用机器码]

    D --> I[ExecEvalExpr<br/>解释模式]

    J[JIT 收益] --> K[表达式计算加速]
    J --> L[元组解构优化]
    J --> M[减少函数调用]
```

**JIT 参数**：

- `jit`：默认 off（PG 12+ 默认 on）
- `jit_above_cost`：默认 100000
- `jit_inline_above_cost`：默认 500000
- `jit_optimize_above_cost`：默认 500000

## Agg 聚合执行

```mermaid
graph TD
    A[Aggregate 节点] --> B[AGG_HASHED<br/>Hash 聚合]
    A --> C[AGG_SORTED<br/>排序聚合]
    A --> D[AGG_PLAIN<br/>简单聚合]

    B --> E[Hash Table 存储<br/>分组]
    E --> F[扫描输入行]
    F --> G[Hash 查找/插入]
    G --> H[累积聚合值]

    C --> I[输入排序]
    I --> J[按组扫描]
    J --> K[累积直到组变]

    D --> L[单次扫描<br/>全局聚合]
```

## Sort 排序执行

```mermaid
flowchart TD
    A[Sort 节点] --> B[收集所有输入行]
    B --> C[存入 tuplesort]
    C --> D{内存够用?}
    D -->|是| E[内存排序<br/>quicksort]
    D -->|否| F[外部排序<br/>disk sort]
    F --> G[写临时文件]
    G --> H[多路归并]
    E --> I[返回有序行]
    H --> I
```

**关键参数**：

- `work_mem`：默认 4MB，控制内存排序上限
- `maintenance_work_mem`：VACUUM / CREATE INDEX 的排序内存

## Limit 执行

```mermaid
flowchart TD
    A[Limit 节点] --> B[计数器 count = 0]
    B --> C[调用子节点]
    C --> D{count < limit?}
    D -->|是| E[返回行, count++]
    D -->|否| F[返回 NULL<br/>结束]
```

## 执行器监控

```sql
-- 当前执行中的查询
SELECT * FROM pg_stat_activity WHERE state = 'active';

-- 执行器统计
SELECT * FROM pg_stat_statements;

-- 等待事件
SELECT query, wait_event_type, wait_event
FROM pg_stat_activity
WHERE wait_event IS NOT NULL;
```

## 要点总结

- PG 使用 Volcano 火山模型，每个节点实现 `ExecProcNode` 接口
- TupleTableSlot 存储一行数据，ExprContext 提供表达式执行环境
- 并行执行通过 Gather 节点协调多个 Worker
- JIT 编译把表达式编译成机器码，加速计算
- work_mem 控制排序、Hash 的内存上限

## 思考题

1. 火山模型每次调用返回一行，函数调用开销大。相比向量化执行（batch 返回多行），PG 为什么坚持火山模型？
2. Hash Join 的 Build 阶段需要全部内表数据，如果内表太大怎么办？
3. `work_mem` 设置过大会导致什么问题？过小会怎样？如何选择合适的值？