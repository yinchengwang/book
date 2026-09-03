# NOTES.md — perf 工程对照

## perf 在数据库性能分析中的工程应用

### 背景

Linux `perf` 是内核自带的性能分析工具，通过硬件性能计数器和软件事件采样，帮助定位 CPU 热点、缓存失效、指令效率等问题。数据库作为 CPU 和内存密集型应用，是 perf 的典型使用场景。

### 工程对照：数据库查询执行分析

在我们的 `engineering/src/db/sql/sql_exec.c` 执行器中，perf 可以帮助分析查询执行效率：

```c
// sql_exec.c 中的热点分析（配合 perf 使用）
typedef struct {
    double parse_time_us;      // 解析耗时
    double plan_time_us;       // 规划耗时
    double exec_time_us;       // 执行耗时
    size_t rows_scanned;       // 扫描行数
    size_t rows_returned;      // 返回行数
} QueryExecutionStats;

// 性能敏感的查询执行循环
void exec_scan_loop(PlanState *state) {
    Timer start;
    timer_start(&start);

    while (heap_scan_next(state->scan)) {
        // 这段循环是 perf 热点分析的典型目标
        ExprEvalContext ctx;
        expr_eval(&ctx, state->target_list);

        if (exec_qual(state->qual, &ctx)) {
            output_tuple(state->slot);
            state->stats.rows_scanned++;
        }
    }

    state->stats.exec_time_us = timer_elapsed_us(&start);
}
```

### perf 与数据库索引

| perf 事件 | 含义 | 数据库场景 |
|-----------|------|------------|
| cache-misses | 缓存未命中 | 索引遍历效率 |
| branch-misses | 分支预测失败 | 条件分支优化 |
| cycles | CPU 周期 | 整体 CPU 效率 |
| instructions | 指令数 | 代码密度 |

### 数据库性能分析实战

```bash
# 1. 分析查询执行热点
perf record -g -o query.perf ./db_driver "SELECT * FROM big_table WHERE ..."
perf report --stdio -i query.perf

# 2. 分析 Buffer Pool 访问模式
perf stat -e cache-misses,L1-dcache-load-misses ./db_driver

# 3. 分析锁竞争（需要 -g 获取调用栈）
perf record -g ./db_server
perf report -g --stdio
```

### 火焰图集成

perf + FlameGraph 是生产环境性能分析的黄金组合：

```bash
# 生成火焰图
perf record -F 99 -g -o perf.data ./program
perf script -i perf.data | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

### 最佳实践

1. **采样频率**：`perf record -F 99` 适合 CPU 密集型，`-F 1000` 适合短时任务
2. **调用栈**：始终加 `-g`，后续分析需要调用栈信息
3. **过滤**：使用 `--call-graph dwarf` 避免栈展开开销
4. **去噪**：先运行空跑基准，再对比有负载时的差异

### 性能影响

- perf record 本身有 <5% 的 CPU 开销
- 高频采样（>1000Hz）可能影响测量精度
- 栈展开（-g）在深层调用栈时开销显著

### 扩展思考

现代数据库（PostgreSQL、MySQL）都提供 `EXPLAIN ANALYZE` 配合 perf 使用：
- EXPLAIN ANALYZE 提供逻辑执行计划
- perf 提供物理层面的 CPU/缓存真实开销
- 两者结合才能全面定位性能瓶颈
