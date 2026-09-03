# Linux CFS 调度器学习笔记

本文档记录 CFS 调度器在项目工程代码中的实际应用对照。

## 工程对照

### 1. 数据库进程的调度考虑

工程代码中数据库服务器可能需要调整调度策略：

```c
// 数据库服务器使用 SCHED_FIFO 获得更低延迟
// engineering/src/db/server/ 中可能有相关设置

#ifdef USE_REALTIME_SCHED
    struct sched_param param = {
        .sched_priority = 50  // 较高优先级
    };
    sched_setscheduler(0, SCHED_FIFO, &param);
#endif
```

**调度策略对比：**

| 策略 | 特点 | 适用场景 |
|------|------|----------|
| SCHED_OTHER | CFS 公平调度 | 一般进程 |
| SCHED_FIFO | 实时，先到先服务 | 关键实时任务 |
| SCHED_RR | 实时，时间片轮转 | 实时任务 |

### 2. CPU 亲和性与 NUMA

工程代码在 NUMA 系统上优化：

```c
// engineering/src/db/storage/buffer/bufmgr.c
// 将 Buffer Pool 绑定到本地 NUMA 节点

#ifdef USE_NUMA
    /* 获取当前 CPU 所属的 NUMA 节点 */
    int node = numa_node_of_cpu(sched_getcpu());

    /* 分配本地内存 */
    void *buf = numa_alloc_onnode(
        pool->max_size * sizeof(buf_block_t),
        node
    );
#endif
```

### 3. I/O 调度与 CFS

数据库的 I/O 调度受 CFS 影响：

```
CFS 调度 I/O 请求
      │
      ▼
+-----------------+
|   I/O 调度器    |  (cfq / deadline / noop)
+-----------------+
      │
      ▼
    磁盘
```

**I/O 调度器对比：**

| 调度器 | 特点 | 适用场景 |
|--------|------|----------|
| cfq | 公平队列 | 通用、桌面 |
| deadline | 截止时间 | 数据库、VMware |
| noop | 先进先出 | SSD、虚拟化 |

### 4. 调度延迟对数据库的影响

```
调度延迟 (sched latency)
      │
      ├── 高延迟 (48ms): 影响查询响应时间
      │
      └── 低延迟 (6ms):  更好的交互响应
```

**优化建议：**

1. **高并发写入**：使用 SCHED_FIFO 避免被抢占
2. **低延迟查询**：减小 latency 参数
3. **批量导入**：使用低优先级，避免影响在线查询

### 5. CPU 亲和性实践

```c
// 绑定数据库工作线程到特定 CPU
void set_cpu_affinity(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        perror("sched_setaffinity");
    }
}

// 主线程绑定到 CPU 0-1
// worker 线程绑定到 CPU 2-N
```

**亲和性优势：**

1. 提高 Cache 命中率
2. 减少跨 NUMA 访问
3. 提高实时性

## 参考源码

- `kernel/sched/fair.c` — CFS 实现（内核源码）
- `engineering/src/db/server/` — 数据库服务器
- `engineering/src/db/storage/buffer/` — Buffer Pool
