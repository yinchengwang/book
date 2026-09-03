# SQL 执行引擎 Phase 5：生产级完善与集成测试

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 Phase 1-4 实现的执行引擎推进到生产可用状态，完成端到端集成、性能基准测试和文档完善。

**Architecture:** 
- 端到端集成：存储引擎 ↔ 执行引擎 ↔ SQL 解析器
- 性能基准：TPC-C 简化版 + 向量查询基准
- 生产完善：真正的并行 worker、触发器调用、JIT 预研

**Tech Stack:** C11/C++17, GoogleTest, CMake, 参考 PostgreSQL 16/17

---

## Global Constraints

- C11 标准
- C++17 标准
- 无运行时外部依赖
- CMake 3.20+
- 所有测试必须通过
- 性能基准必须可复现

---

## Phase 5 任务分解

### Task 5.1：存储引擎集成

**目标**：执行引擎与存储引擎对接，实现真正的数据读写。

**Files:**
- Modify: `engineering/src/db/sql/nodeSeqscan.c`
- Modify: `engineering/src/db/sql/nodeModifyTable.c`
- Create: `engineering/test/db/sql/test_storage_integration.cpp`

**接口对接：**
```c
// SeqScan 需要调用存储层
SeqScanState *ExecInitSeqScan(SeqScan *node, EState *estate, int eflags) {
    // 打开 Relation
    Relation rel = relation_open(node->scanrelid, REL_OPEN_READ);
    // 开始扫描
    ScanDesc scan = table_beginscan(rel, estate->es_snapshot);
    // 设置执行函数
    state->ExecProcNode = ExecSeqScan;
}

// ModifyTable 需要调用存储层
static TupleTableSlot *ExecModifyTable(PlanState *pstate) {
    switch (operation) {
        case CMD_INSERT:
            table_tuple_insert(rel, slot);
            break;
        case CMD_UPDATE:
            table_tuple_update(rel, otid, slot);
            break;
        case CMD_DELETE:
            table_tuple_delete(rel, tid);
            break;
    }
}
```

**验收标准：**
- `SELECT * FROM t` 返回真实数据
- `INSERT INTO t VALUES (...)` 写入持久化
- `UPDATE t SET col=val WHERE ...` 正确更新
- `DELETE FROM t WHERE ...` 正确删除
- 事务提交后数据可读

**预估工作量：** ~1,500 行，2 周

---

### Task 5.2：SQL 解析器端到端集成

**目标**：从 SQL 文本到执行结果，完整链路打通。

**Files:**
- Create: `engineering/test/db/sql/test_e2e_query.cpp`
- Create: `engineering/test/db/sql/test_e2e_dml.cpp`

**测试场景：**

```cpp
// 端到端查询测试
TEST(E2EQueryTest, SimpleSelect) {
    const char *sql = "SELECT id, name FROM users WHERE age > 18";
    QueryResult *result = execute_sql(sql);
    
    ASSERT_EQ(result->nrows, 10);
    ASSERT_EQ(result->ncols, 2);
    EXPECT_STREQ(result->cols[0].name, "id");
    EXPECT_STREQ(result->cols[1].name, "name");
}

// 端到端 DML 测试
TEST(E2EDMLTest, InsertAndSelect) {
    execute_sql("INSERT INTO users (id, name, age) VALUES (1, 'Alice', 25)");
    execute_sql("INSERT INTO users (id, name, age) VALUES (2, 'Bob', 30)");
    
    QueryResult *result = execute_sql("SELECT COUNT(*) FROM users");
    EXPECT_EQ(result->rows[0][0], 2);
}

// 端到端连接测试
TEST(E2EJoinTest, InnerJoin) {
    const char *sql = "SELECT u.name, o.total "
                      "FROM users u "
                      "JOIN orders o ON u.id = o.user_id";
    QueryResult *result = execute_sql(sql);
    ASSERT_GT(result->nrows, 0);
}
```

**验收标准：**
- 10 个典型查询通过
- 5 个 DML 操作通过
- 3 个多表连接通过
- 无内存泄漏

**预估工作量：** ~800 行，1 周

---

### Task 5.3：性能基准测试

**目标**：建立性能基线，跟踪优化效果。

**Files:**
- Create: `engineering/test/db/sql/benchmark/benchmark_config.h`
- Create: `engineering/test/db/sql/benchmark/benchmark_scan.cpp`
- Create: `engineering/test/db/sql/benchmark/benchmark_join.cpp`
- Create: `engineering/test/db/sql/benchmark/benchmark_aggregate.cpp`
- Create: `engineering/test/db/sql/benchmark/benchmark_vector.cpp`

**基准测试场景：**

```cpp
// 扫描性能基准
TEST(BenchmarkScanTest, SeqScan1MRows) {
    BenchmarkConfig cfg = {.nrows = 1000000, .ncols = 10};
    Table *t = create_test_table(cfg);
    
    auto start = std::chrono::high_resolution_clock::now();
    QueryResult *result = execute_sql("SELECT COUNT(*) FROM t");
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    RecordBenchmark("SeqScan1M", duration.count());
    
    EXPECT_LT(duration.count(), 1000);  // < 1 秒
}

// 连接性能基准
TEST(BenchmarkJoinTest, HashJoin100Kx100K) {
    // 创建两张 10 万行的表
    // 执行 HashJoin
    // 记录耗时
}

// 向量查询基准
TEST(BenchmarkVectorTest, VectorSearch100D) {
    // 创建 100 维向量表
    // 执行 KNN 查询
    // 记录 QPS
}
```

**验收标准：**
- SeqScan 100 万行 < 1 秒
- HashJoin 10 万×10 万 < 5 秒
- 向量搜索 QPS > 1000

**预估工作量：** ~1,200 行，1.5 周

---

### Task 5.4：并行执行完善

**目标**：实现真正的并行 worker 线程池。

**Files:**
- Modify: `engineering/src/db/sql/parallel.c`
- Create: `engineering/include/db/sql/worker_pool.h`
- Create: `engineering/src/db/sql/worker_pool.c`
- Modify: `engineering/test/db/sql/test_parallel.cpp`

**并行 Worker 设计：**

```c
// Worker 线程池
typedef struct WorkerPool {
    int              nworkers;
    pthread_t       *threads;
    WorkerQueue     *task_queue;
    pthread_mutex_t  lock;
    pthread_cond_t   cond;
    bool             shutdown;
} WorkerPool;

// 并行执行上下文
typedef struct ParallelWorkerContext {
    int              worker_id;
    ParallelContext *pcxt;
    PlanState       *planstate;
    TupleQueue      *output_queue;
} ParallelWorkerContext;

// 并行执行入口
void *ParallelWorkerMain(void *arg) {
    ParallelWorkerContext *ctx = (ParallelWorkerContext *)arg;
    
    // 执行子计划
    while (!ctx->pcxt->coordinator->finished) {
        TupleTableSlot *slot = ExecProcNode(ctx->planstate);
        if (TupIsNull(slot)) break;
        
        // 发送到输出队列
        TupleQueueSend(ctx->output_queue, slot);
    }
    
    return NULL;
}
```

**验收标准：**
- 4 worker 并行扫描加速比 > 3x
- 并行 HashJoin 正确性验证
- 无数据竞争

**预估工作量：** ~2,000 行，2 周

---

### Task 5.5：触发器调用机制

**目标**：实现触发器函数的真正调用。

**Files:**
- Modify: `engineering/src/db/sql/trigger.c`
- Create: `engineering/include/db/sql/trigger_functions.h`
- Create: `engineering/src/db/sql/trigger_functions.c`
- Modify: `engineering/test/db/sql/test_trigger.cpp`

**触发器函数调用设计：**

```c
// 触发器函数注册表
typedef struct TriggerFunctionTable {
    char           *func_name;
    Oid             func_oid;
    TriggerFunc     func_ptr;
} TriggerFunctionTable;

// 触发器函数类型
typedef TupleTableSlot *(*TriggerFunc)(TriggerData *trig_data);

// 执行触发器函数
TupleTableSlot *ExecTriggerFunction(TriggerDesc *desc, TriggerData *trig_data) {
    // 1. 查找函数
    TriggerFunctionTable *entry = find_trigger_function(desc->tgfuncid);
    if (!entry) {
        elog(ERROR, "trigger function %u not found", desc->tgfuncid);
        return NULL;
    }
    
    // 2. 调用函数
    return entry->func_ptr(trig_data);
}

// 内置触发器函数示例
TupleTableSlot *trigger_timestamp_insert(TriggerData *trig_data) {
    // 自动设置 created_at 列
    TupleTableSlot *slot = trig_data->tg_trigslot;
    set_slot_attr(slot, "created_at", current_timestamp());
    return slot;
}
```

**验收标准：**
- BEFORE INSERT 触发器可修改行
- AFTER INSERT 触发器可记录日志
- 触发器链正确执行

**预估工作量：** ~1,000 行，1 周

---

### Task 5.6：JIT 编译预研

**目标**：评估 LLVM JIT 对表达式求值的加速效果。

**Files:**
- Create: `engineering/docs/jit-research.md`
- Create: `engineering/include/db/sql/jit.h`（占位）
- Create: `engineering/src/db/sql/jit.c`（占位）

**JIT 预研内容：**

```c
// JIT 编译的表达式求值
typedef struct JITExprState {
    ExprState    base;
    void        *jit_func;      // JIT 编译后的函数指针
    LLVMJITContext *llvm_ctx;   // LLVM 上下文
} JITExprState;

// JIT 编译流程
JITExprState *JitCompileExpr(Expr *expr) {
    // 1. 将 Expr 树转换为 LLVM IR
    LLVMValueRef func = compile_expr_to_llvm(expr);
    
    // 2. 编译为机器码
    void *func_ptr = LLVMGetPointerToGlobal(func);
    
    // 3. 封装为 JITExprState
    JITExprState *state = palloc(sizeof(JITExprState));
    state->jit_func = func_ptr;
    return state;
}

// JIT 执行
Datum ExecJitExpr(JITExprState *state, ExprContext *econtext) {
    typedef Datum (*JitFunc)(ExprContext *);
    return ((JitFunc)state->jit_func)(econtext);
}
```

**验收标准：**
- 完成 JIT 技术调研报告
- 实现 1 个简单表达式的 JIT 编译
- 性能对比：JIT vs 解释器

**预估工作量：** ~500 行（代码）+ 文档，1 周

---

### Task 5.7：文档完善

**目标**：完整的 API 文档、架构文档和用户手册。

**Files:**
- Create: `docs/sql-executor/API.md`
- Create: `docs/sql-executor/ARCHITECTURE.md`
- Create: `docs/sql-executor/USER_GUIDE.md`
- Update: `README.md`

**文档结构：**

```
docs/sql-executor/
├── API.md              # 公共 API 参考
│   ├── MemoryContext
│   ├── ExprState
│   ├── PlanState
│   ├── EState
│   └── 各执行器节点
├── ARCHITECTURE.md     # 架构设计
│   ├── 整体架构
│   ├── Volcano 迭代器
│   ├── 表达式求值
│   └── 并行执行
└── USER_GUIDE.md       # 用户手册
    ├── 如何添加新算子
    ├── 如何编写测试
    └── 性能调优指南
```

**验收标准：**
- API 文档覆盖所有公共接口
- 架构文档包含完整架构图
- 用户手册有完整示例

**预估工作量：** ~3,000 行（文档），1 周

---

## 实施顺序

```
Week 1-2: Task 5.1 存储引擎集成
Week 3:   Task 5.2 SQL 解析器端到端集成
Week 4-5: Task 5.3 性能基准测试
Week 6-7: Task 5.4 并行执行完善
Week 8:   Task 5.5 触发器调用机制
Week 9:   Task 5.6 JIT 编译预研
Week 10:  Task 5.7 文档完善
```

---

## 总工作量估算

| 任务 | 代码量 | 工期 |
|------|--------|------|
| Task 5.1 存储引擎集成 | ~1,500 行 | 2 周 |
| Task 5.2 端到端集成 | ~800 行 | 1 周 |
| Task 5.3 性能基准 | ~1,200 行 | 1.5 周 |
| Task 5.4 并行完善 | ~2,000 行 | 2 周 |
| Task 5.5 触发器调用 | ~1,000 行 | 1 周 |
| Task 5.6 JIT 预研 | ~500 行 | 1 周 |
| Task 5.7 文档完善 | ~3,000 行 | 1 周 |
| **总计** | **~10,000 行** | **10.5 周** |

---

## 成功标准

1. **功能完整性**
   - 端到端 SQL 执行：解析 → 优化 → 执行 → 返回结果
   - 所有 DML 操作正确持久化

2. **性能达标**
   - SeqScan 100 万行 < 1 秒
   - HashJoin 10 万×10 万 < 5 秒
   - 并行加速比 > 3x

3. **质量保证**
   - 所有测试通过
   - 无内存泄漏
   - 无数据竞争

4. **文档完备**
   - API 文档完整
   - 架构文档清晰
   - 用户手册可用

---

*计划版本：v1.0*
*创建日期：2026-07-15*