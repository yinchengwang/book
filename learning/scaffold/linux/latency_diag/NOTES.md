# NOTES.md — latency_diag 工程对照

## 延迟优化方法论在数据库中的应用

### 背景

延迟（Latency）是衡量系统响应速度的核心指标。数据库查询延迟由多个阶段组成（解析、计划、执行、I/O、排序、网络），延迟分解法帮助定位性能瓶颈。

### 工程对照：数据库查询延迟诊断

在我们的 `engineering/src/db/sql/sql_exec.c` 中：

```c
// sql_exec.c 中的延迟诊断系统
typedef struct {
    double parse_us;         // 解析耗时
    double plan_us;          // 规划耗时
    double buffer_read_us;   // Buffer Pool 读耗时
    double disk_read_us;     // 磁盘读耗时
    double exec_us;          // 执行耗时
    double sort_us;          // 排序耗时
    double output_us;        // 输出耗时
    double total_us;         // 总耗时
} QueryLatencyProfile;

/* 延迟诊断追踪 */
typedef struct {
    struct timespec phase_start;
    LatencyPhase current_phase;
} LatencyTracker;

#define TRACE_PHASE_BEGIN(phase) do { \
    tracker->current_phase = phase; \
    clock_gettime(CLOCK_MONOTONIC, &tracker->phase_start); \
} while(0)

#define TRACE_PHASE_END(profile, phase) do { \
    struct timespec now; \
    clock_gettime(CLOCK_MONOTONIC, &now); \
    double elapsed = (now.tv_sec - tracker->phase_start.tv_sec) * 1e6 + \
                    (now.tv_nsec - tracker->phase_start.tv_nsec) / 1e3; \
    switch (phase) { \
        case L_PARSE: profile->parse_us += elapsed; break; \
        case L_PLAN:  profile->plan_us += elapsed; break; \
        /* ... */ \
    } \
} while(0)

/* 执行查询并收集延迟 */
ResultTable* sql_execute_with_profile(ExecutorState *state,
                                       const char *sql,
                                       QueryLatencyProfile *profile) {
    TRACE_PHASE_BEGIN(L_PARSE);
    ParseTree *tree = sql_parse(sql);
    TRACE_PHASE_END(profile, L_PARSE);

    TRACE_PHASE_BEGIN(L_PLAN);
    Plan *plan = create_plan(tree);
    Plan *opt = optimize_plan(plan);
    TRACE_PHASE_END(profile, L_PLAN);

    TRACE_PHASE_BEGIN(L_EXEC);
    while (plan_next(opt->state, &slot)) {
        // ...
    }
    TRACE_PHASE_END(profile, L_EXEC);

    profile->total_us = profile->parse_us + profile->plan_us +
                        profile->exec_us;
    return result;
}
```

### 数据库延迟优化决策树

```
延迟高
├── parse_us 占比高 → 预编译语句 (PREPARE/EXECUTE)
├── plan_us 占比高 → 查询缓存 + 简化 SQL
├── buffer_read_us 占比高 → 增大 shared_buffers
├── disk_read_us 占比高 → 索引优化 / SSD 升级
├── exec_us 占比高
│   ├── 大量扫描 → 添加 WHERE / LIMIT
│   └── 多次执行 → 批量操作 / 减少循环
├── sort_us 占比高 → 索引覆盖排序 / work_mem 增大
└── output_us 占比高 → 减少 SELECT * / 分页返回
```

### 生产环境延迟 SLO

```yaml
# 数据库服务级别目标 (SLO)
metrics:
  query_latency_p50: 10ms
  query_latency_p95: 50ms
  query_latency_p99: 200ms
  query_latency_p999: 500ms

# 延迟预算
# 1. 解析 + 计划: < 2ms (忽略不计)
# 2. Buffer Pool 查找: < 1ms
# 3. 磁盘 I/O: < 10ms (SSD)
# 4. 执行/过滤: < 20ms
# 5. 排序: < 10ms
# 6. 网络: < 1ms (内网)
```

### 延迟监控最佳实践

1. **分段监控**：分别记录每阶段耗时，而非仅总耗时
2. **百分位**：P50/P95/P99/P999 比平均值更有意义
3. **尾延迟**：P99 慢查询往往来自后台任务干扰
4. **关联分析**：延迟突增与系统指标（CPU/IO/锁）关联
5. **基线对比**：定期生成基准，检测性能退化

### 性能影响

- 延迟追踪本身开销：<0.1%（使用 CLOCK_MONOTONIC）
- 精细分段追踪（每行）开销：可能 1-5%
- 建议：采样追踪（每 100 条记录 1 条）

### 扩展思考

- Google Dapper + Jaeger 分布式追踪在数据库中的应用
- PostgreSQL 的 `auto_explain` 自动输出慢查询延迟
- MySQL 的 `performance_schema.events_statements_summary_by_digest`
- ClickHouse 的 `system.query_log` 完整延迟分解
