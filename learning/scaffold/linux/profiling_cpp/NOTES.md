## 工程对照

工程轨中 `engineering/src/db/performance/performance.c` 实现了并行执行器
`parallel_executor_t`，其 `parallel_execute()` 使用 `parallel_task_t` 结构
将任务分块调度。在 CPU 密集型负载（如 SIMD 向量化 `simd_sum_double()`）
下，`perf top` 可直接看到 `simd_sum_double` 和 `parallel_executor_create`
占用最高 CPU 时间。Buffer Pool (`bufmgr.c`) 的 `buffer_get_page()` 在
Cache Miss 时会产生数十微秒延迟，表现为 `buffer_get_page@bufmgr.c:XXX`
的热点函数。用 `perf sched` 可观察进程调度延迟，定位 lock 竞争导致的
调度不公平问题。

### 与 demo 的对应关系

| Demo 函数 | 工程对标 | 性能工具 |
|-----------|----------|----------|
| `compute_bound()` | `simd_sum_double()` | `perf top -g` 采样 |
| `io_bound()` | `fwrite()` / `buffer_flush()` | `perf sched latency` |
| `now_ms()` | `CycleCounter` | `perf stat` 事件统计 |

学习轨的 `profiling_cpp` 演示了基本的 CPU 热点采样方法，工程轨中的
`performance.c` 则是真实存储引擎的性能优化实现，两者结合可完整掌握
从发现热点到消除热点的闭环。
