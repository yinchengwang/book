# NOTES.md — numa_observ 工程对照

## NUMA 感知数据库设计

### 背景

多路服务器中，每个 CPU socket 有其亲缘内存，访问远程节点的延迟远高于本地节点。数据库作为内存密集型应用，NUMA 感知设计能带来 10-20% 性能提升。

### 工程对照：Buffer Pool NUMA 感知

在我们的 `engineering/src/db/storage/bufmgr.c` 中：

```c
// bufmgr.c 中的 NUMA 感知 Buffer Pool
typedef struct {
    int node_count;            // NUMA 节点数
    BufferPool *pools[];      // 每个节点独立 Buffer Pool
} NumaBufferPool;

/* 初始化 NUMA 感知 Buffer Pool */
NumaBufferPool* numa_bufpool_init(int total_pages) {
    int node_count = numa_max_node() + 1;
    NumaBufferPool *nbuf = calloc(1, sizeof(*nbuf) +
                                 node_count * sizeof(BufferPool*));
    nbuf->node_count = node_count;

    for (int i = 0; i < node_count; i++) {
        int pages_per_node = total_pages / node_count;
        // 在节点本地内存分配
        nbuf->pools[i] = numa_alloc_onnode(
            sizeof(BufferPool) + pages_per_node * sizeof(BufferDesc), i);
        nbuf->pools[i]->node_id = i;
        nbuf->pools[i]->size = pages_per_node;
    }

    return nbuf;
}

/* 选择 NUMA 节点（根据当前 CPU）*/
int get_current_node(void) {
    int cpu = sched_getcpu();
    return numa_node_of_cpu(cpu);
}

/* 页面获取（优先本地节点）*/
Page* numa_buffer_get_page(NumaBufferPool *nbuf, PageID page_id) {
    int local_node = get_current_node();
    BufferPool *pool = nbuf->pools[local_node];

    // 先查本地节点
    BufferDesc *desc = hash_lookup(pool->hash, page_id);
    if (desc) {
        pool->stats.node_hit++;
        return &desc->page;
    }

    // 再查远程节点
    for (int i = 0; i < nbuf->node_count; i++) {
        if (i == local_node) continue;
        desc = hash_lookup(nbuf->pools[i]->hash, page_id);
        if (desc) {
            nbuf->pools[i]->stats.remote_access++;
            return &desc->page;  // 跨节点访问
        }
    }

    // 都不命中，从磁盘加载到本地节点
    return load_from_disk(nbuf->pools[local_node], page_id);
}
```

### NUMA 策略在数据库中的应用

| 场景 | 推荐策略 | 原因 |
|------|---------|------|
| Buffer Pool | INTERLEAVE | 所有线程均可本地访问 |
| WAL 缓冲区 | MEMBIND node 0 | WAL 少线程、需低延迟 |
| 查询线程栈 | PREFERRED | 栈在本地节点 |
| 索引缓存 | INTERLEAVE | 所有核心均可访问 |
| 网络缓冲区 | MEMBIND 本地 | 仅本地线程使用 |

### NUMA 监控

```bash
# 1. 查看数据库进程的 NUMA 统计
numastat -p $(pgrep db_server)

# 2. 查看本地/远程内存访问比例
perf stat -e node-loads,node-load-misses,node-stores,node-store-misses \
    -p $(pgrep db_server) sleep 10

# 3. 查看各节点内存使用
numactl --hardware
cat /sys/devices/system/node/node*/meminfo
```

### 数据库 NUMA 配置

```bash
# PostgreSQL 示例
# 启动时使用 numactl 交织分配
numactl --interleave=all postgres -D /data

# MySQL 示例
# innodb_numa_interleave = 1  (MySQL 8.0+)
# 或使用
numactl --interleave=all mysqld --datadir=/data
```

### 最佳实践

1. **Buffer Pool 交织分配**：避免热点节点耗尽
2. **连接线程绑定**：线程绑定到创建时的 CPU
3. **并行查询**：workers 分配到多个节点，各自用本地内存
4. **避免跨节点迁移**：调度器设置 CPU affinity

### 性能影响

- 远程访问延迟：~1.5-2x 本地访问
- 跨节点 QPI 带宽有限：约 25-50 GB/s
- 合理 NUMA 设计可提升 10-20% 吞吐
- 无视 NUMA 可能损失 30-50% 性能（极端情况）

### 扩展思考

- PostgreSQL 的 `numa_balancing` 参数
- MySQL 的 `innodb_numa_interleave`
- MemSQL/ClickHouse 的 NUMA 感知查询执行器
- 云数据库（AWS RDS）通常 UMA 架构，不需要 NUMA 优化
