# HugePages 工程对照笔记

## 内存密集型应用优化

### PostgreSQL

PostgreSQL 的 shared_buffers 是共享内存池，大页可显著减少 TLB 开销：

```ini
# postgresql.conf
huge_pages = try        # 尝试使用大页，失败则回退
shared_buffers = 8GB   # 建议是 2MB 的整数倍
```

**配置步骤**：
```bash
# 1. 计算所需页数（8GB / 2MB = 4096）
# 2. 配置系统
sysctl -w vm.nr_hugepages=4096

# 3. 验证
grep -i huge /proc/meminfo
```

### MySQL/InnoDB

InnoDB 通过 `large_pages` 参数启用：

```ini
[mysqld]
large_pages = 1
innodb_buffer_pool_size = 16G
```

### Faiss 向量索引

Faiss 处理百万级向量时，内存映射文件 + HugePages 是常见优化：

```cpp
// Faiss 内存映射 + 大页
faiss::IndexIVFFlat *index = ...;
index->train(num_vectors, vectors);
// mmap 大文件配合 HugePages
```

### 工程实践

1. **透明大页（THP）**：RHEL/CentOS 7+ 自动启用，但数据库场景建议关闭（khugepaged 造成延迟抖动）
2. **NUMA 亲和**：大页应与 local_memory 绑定：`numactl --membind=0 ./postgres`
3. **预留足够**：系统内存 = PostgreSQL shared_buffers + OS 预留 + HugePages
4. **监控**：关注 `/proc/meminfo` 中 `HugePages_Rsvd` 防止 OOM
