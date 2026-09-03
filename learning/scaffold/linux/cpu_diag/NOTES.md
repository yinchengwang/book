# NOTES.md — CPU 性能监控工程对照

## CPU 性能监控的工程实践

### 背景

CPU 性能监控是系统性能分析的第一步。理解 CPU 利用率、上下文切换、调度延迟等指标，对于诊断性能问题至关重要。

### 工程对照：数据库查询执行器的 CPU 开销

在 `engineering/src/db/executor/` 查询执行器中，CPU 监控可用于：

1. **火山模型的开销分析**：每行调用的 `next()` 函数调用开销
2. **向量化执行的优势**：减少虚函数调用，提升 CPU 缓存命中率
3. **并行执行的任务分配**：负载均衡的 CPU 亲和性调度

```c
// executor 中的 CPU 时间统计（概念示例）
typedef struct {
    uint64_t cpu_time_ns;      // 总 CPU 时间（纳秒）
    uint64_t row_count;        // 处理行数
    uint64_t tuple_fetch;      // 元组获取次数
    uint64_t index_scan;       // 索引扫描次数
} ExecutorStats;

static __thread ExecutorStats current_stats;

// 在查询执行过程中收集统计
void executor_collect_stats(ExecutorStats *stats) {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    stats->cpu_time_ns = ts.tv_sec * 1e9 + ts.tv_nsec;
}
```

### CPU 监控的关键指标

| 指标 | 含义 | 诊断价值 |
|------|------|----------|
| user% | 用户态 CPU 时间占比 | 应用代码 CPU 消耗 |
| system% | 内核态 CPU 时间占比 | 系统调用、内存分配开销 |
| iowait% | 等待 I/O 的 CPU 空闲 | I/O 瓶颈定位 |
| idle% | 完全空闲的 CPU | 系统整体利用率 |
| ctx/s | 每秒上下文切换数 | 线程调度开销 |

### 上下文切换的开销

上下文切换的主要开销：
1. **寄存器保存/恢复**：约 100-1000 个 CPU 周期
2. **TLB 刷新**：TLB（Translation Lookaside Buffer）失效
3. **CPU 缓存失效**：L1/L2 缓存命中率下降
4. **调度器开销**：进程/线程选择算法执行

### 数据库场景

- **OLTP 场景**：高并发小查询，需要关注上下文切换
- **OLAP 场景**：大查询批量处理，CPU 利用率是主要指标
- **混合负载**：需要平衡 CPU 使用和 I/O 等待

### 工具链

| 工具 | 用途 |
|------|------|
| `top`/`htop` | 实时 CPU 监控 |
| `vmstat` | 上下文切换和中断统计 |
| `pidstat` | 单进程 CPU 使用 |
| `perf` | 热点函数分析 |
| `/proc/stat` | 原始 CPU 统计 |

### 优化策略

1. **减少上下文切换**：使用线程池、减少线程数
2. **CPU 亲和性**：`sched_setaffinity` 将线程绑定到特定核心
3. **NUMA 感知**：在 NUMA 系统上确保本地内存访问
4. **批处理**：将小任务合并减少调度开销
