# NOTES.md — db_flamegraph 工程对照

## 火焰图在数据库性能分析中的应用

### 背景

火焰图（FlameGraph）由 Brendan Gregg 于 2011 年发明，是分析 CPU 消费的可视化工具。数据库查询涉及多阶段（解析、优化、执行、排序），火焰图能直观识别每个阶段的 CPU 占比。

### 工程对照：查询执行路径分析

在我们的 `engineering/src/db/sql/sql_exec.c` 中，火焰图可以可视化不同阶段的 CPU 开销：

```c
// sql_exec.c 中的查询执行阶段（火焰图各层栈）
typedef enum {
    PHASE_PARSE,        // 词法/语法分析
    PHASE_ANALYZE,      // 语义分析
    PHASE_REWRITE,      // 查询重写
    PHASE_PLAN,         // 计划生成
    PHASE_OPTIMIZE,     // 代价优化
    PHASE_EXECUTE,      // 执行扫描
    PHASE_SORT,         // 结果排序
    PHASE_OUTPUT        // 结果输出
} QueryPhase;

/* 查询执行入口（火焰图的根）*/
ResultTable* sql_execute(ExecutorState *state, const char *sql) {
    perf_trace_begin(PHASE_PARSE);
    ParseTree *tree = sql_parse(sql);  // <-- 火焰图栈: parse → yyparse → ...

    perf_trace_end(PHASE_PARSE);
    perf_trace_begin(PHASE_PLAN);

    Plan *plan = create_plan(tree);    // <-- 火焰图栈: plan → cost_estimate → ...
    Plan *opt = optimize_plan(plan);   // <-- 火焰图栈: optimize → join_order_select → ...

    perf_trace_end(PHASE_PLAN);
    perf_trace_begin(PHASE_EXECUTE);

    TupleSlot *slot;
    while (plan_next(opt->state, &slot)) {  // <-- 最大火焰图栈
        // 条件过滤
        if (exec_qual(opt->qual, slot)) {
            output_row(slot);
        }
    }

    perf_trace_end(PHASE_EXECUTE);
}
```

### 火焰图分析数据库热点

```
                    root（100%）
        ┌───────────────────────────────┐
    parse(2%)  plan(8%)     execute(90%)
                        ┌─────────────────────┐
                   index_scan(45%)    heap_scan(20%)  sort(25%)
                                                        ↑
                                                    O(n²) 热点！
```

### 数据库火焰图实战

```bash
# 1. 采集数据库负载
perf record -F 99 -g -p $(pgrep db_driver) sleep 30

# 2. 生成火焰图
perf script | FlameGraph/stackcollapse-perf.pl | FlameGraph/flamegraph.pl \
    --colors=hot --title="DB Driver CPU Flamegraph" > db_flame.svg

# 3. 分析结果
# - index_scan 占 45%：考虑索引优化
# - sort 占 25%：排序是 O(n²)？检查排序算法
# - parse 占 2%：不必优先优化
```

### 火焰图类型与数据库场景

| 火焰图类型 | 采集命令 | 分析目标 |
|-----------|---------|---------|
| CPU 火焰图 | `perf record -F 99 -g` | 热点函数 |
| 内存火焰图 | `perf record -e page-faults -g` | 内存分配模式 |
| 冷火焰图 | `perf record -e sched:sched_switch -g` | 等待/阻塞分析 |
| 差分火焰图 | `difffolded.pl before.folded after.folded` | 优化前后对比 |
| 事务火焰图 | 自定义 tracepoint | 事务内操作占比 |

### 生产环境火焰图采集方案

```bash
# 方案 1: 按需采集（用户触发）
kill -USR2 <db_pid>  # 信号触发 perf
sleep 10
kill -USR2 <db_pid>  # 停止采集
perf script | flamegraph.pl > ondemand_flame.svg

# 方案 2: 定时采集（crontab）
# 每小时采集 10 秒
0 * * * * perf record -F 99 -g -p <pid> -o /tmp/flame_$(date +\%H).data sleep 10
```

### 最佳实践

1. **采样频率**：99Hz 对 CPU 影响 <1%，适合生产环境
2. **采样时长**：10-60 秒，太短不准确，太长开销大
3. **栈展开**：使用 dwarf 模式（`--call-graph dwarf`）不需内核符号表
4. **差分对比**：优化前后各采集一份，用 `difffolded.pl` 对比

### 性能影响

- `perf record -F 99`：<1% CPU 开销
- `perf record -g`：2-5% CPU 开销（栈展开）
- 生产环境采样 10-30 秒，总影响可忽略

### 扩展思考

- MySQL 的 performance_schema 提供数据库层火焰图
- PostgreSQL 的 pg_stat_statements 结合火焰图分析慢查询
- 定时火焰图对比引擎集成到 CI/CD（如持续火焰图 profiling）
