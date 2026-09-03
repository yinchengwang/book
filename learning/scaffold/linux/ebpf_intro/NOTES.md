# NOTES.md — ebpf_intro 工程对照

## eBPF 在可观测性中的工程应用

### 背景

eBPF 是现代 Linux 可观测性的基础技术，允许在无需重启或修改内核的情况下，对系统进行动态插桩。数据库系统越来越多地利用 eBPF 进行低开销的性能诊断。

### 工程对照：数据库 I/O 路径追踪

在我们的 `engineering/src/db/storage/` 模块中，可以用 eBPF 追踪关键路径：

```c
// storage_engine.c 中的追踪点设计（配合 eBPF）
typedef enum {
    TRACE_BUF_READ,          // Buffer Pool 读
    TRACE_BUF_WRITE,         // Buffer Pool 写
    TRACE_BUF_EVICT,         // 页面淘汰
    TRACE_BUF_HIT,           // 缓存命中
    TRACE_BUF_MISS,          // 缓存未命中
    TRACE_DISK_READ,         // 磁盘读
    TRACE_DISK_WRITE,        // 磁盘写
    TRACE_LOCK_WAIT,         // 锁等待
    TRACE_CHECKPOINT,        // 检查点
} TraceEvent;

// 静态追踪点（USDT 探针，eBPF 可附加）
static void trace_storage_event(TraceEvent event, uint64_t data) {
    // 使用 USDT 探针让 eBPF 程序动态追踪
    // DTRACE_PROBE2(storage, trace_event, event, data);
    __asm__ __volatile__("" :: "g"(event), "g"(data));
}

// 在关键路径上插桩
Page* buffer_get_page(BufferPool *pool, PageID page_id) {
    trace_storage_event(TRACE_BUF_READ, page_id);

    BufferDesc *desc = hash_lookup(pool->hash_table, page_id);
    if (desc) {
        trace_storage_event(TRACE_BUF_HIT, page_id);
        return &desc->page;
    } else {
        trace_storage_event(TRACE_BUF_MISS, page_id);
        // 从磁盘加载
    }
}
```

### eBPF 追踪数据库 I/O 延迟

```python
#!/usr/bin/env python3
# BCC 脚本：追踪数据库 I/O 延迟分布
from bcc import BPF

bpf_text = """
#include <uapi/linux/ptrace.h>

// 挂接到 read() 系统调用
TRACEPOINT_PROBE(syscalls, sys_enter_read) {
    // 只追踪 db_driver 进程
    if (bpf_get_current_comm() != "db_driver")
        return 0;

    u64 ts = bpf_ktime_get_ns();
    args->read_start.update(&args->fd, &ts);
}

TRACEPOINT_PROBE(syscalls, sys_exit_read) {
    u64 *start = args->read_start.lookup(&args->fd);
    if (!start) return 0;

    u64 delta_ns = bpf_ktime_get_ns() - *start;
    // 记录延迟到直方图
    args->read_latency.increment(bpf_log2l(delta_ns));
}

// 延迟直方图存储
BPF_HASH(read_start, u64, u64);
BPF_HISTOGRAM(read_latency);
"""
```

### 数据库场景 eBPF 工具

| 工具 | 追踪目标 | 数据库场景 |
|------|---------|-----------|
| biolatency | 磁盘 I/O 延迟分布 | 存储引擎性能 |
| cachestat | 页缓存命中率 | Buffer Pool 效果 |
| tcptop | TCP 流量 | 网络连接诊断 |
| funclatency | 函数调用延迟 | 热点函数定位 |
| profile | CPU 火焰图 | 查询执行热点 |

### 生产环境 eBPF 诊断

```bash
# 1. 数据库进程 I/O 延迟分布
sudo bpftrace -e 'kprobe:submit_bio { @start[tid] = nsecs; }
  kretprobe:submit_bio /@start[tid]/ {
    $delta = nsecs - @start[tid];
    @io_latency_ns = hist($delta);
    delete(@start[tid]);
  }'

# 2. 数据库连接创建追踪
sudo bpftrace -e 't:/syscalls:sys_enter_accept4
  /comm == "db_server"/ { printf("new conn at %s\n", strftime("%H:%M:%S", nsecs)); }'

# 3. Buffer Pool 脏页刷盘频率
sudo bpftrace -e 'uprobe:/path/to/db_driver:buffer_flush_page
  { @flushes = count(); }
  interval:s:10 { printf("flushes/sec: %d\n", @flushes); clear(@flushes); }'
```

### 最佳实践

1. **低开销追踪**：eBPF 相比传统 strace 开销低 10-100 倍
2. **动态插桩**：不需要重新编译或重启数据库
3. **分层观测**：eBPF（底层）+ EXPLAIN ANALYZE（SQL 层）组合
4. **生产友好**：内核验证器确保安全，不会 crash 内核

### 性能影响

- kprobe/uprobe 追踪：<1% CPU 开销
- 频繁事件采样（如 malloc）：可能 5-10% 开销
- tracepoint：开销最小，推荐使用

### 扩展思考

现代数据库（如 MySQL 8.0、PostgreSQL 14+）的内置 eBPF 集成：
- MySQL 使用 eBPF 追踪查询执行路径
- PostgreSQL 的 pg_backend_memory_contexts 结合 eBPF 分析内存
- 分布式数据库用 eBPF 监控跨节点网络延迟
