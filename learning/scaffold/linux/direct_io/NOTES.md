# Direct I/O 工程对照笔记

## 数据库 Direct I/O

### MySQL/InnoDB

InnoDB 使用 O_DIRECT 时，数据文件绕过 Page Cache 直接与磁盘交互：

```c
// InnoDB 内部实现（简化）
int fd = open(file_path, O_RDWR | O_DIRECT);
```

**关键配置**：
- `innodb_flush_method = O_DIRECT`（Linux 环境）
- 需要同时配置 `innodb_buffer_pool_size` 留出足够内存给 OS 页缓存

### PostgreSQL

PostgreSQL 通过 `effective_io_concurrency` 控制异步 I/O：

```ini
effective_io_concurrency = 2   # 启用异步 I/O
```

### 工程实践

1. **对齐必须**：`posix_memalign(&ptr, 4096, size)` — 4K 对齐是主流配置
2. **双缓冲**：应用层缓存 + Direct I/O，如 RocksDB 的 BlockCache
3. **fsync 仍必需**：O_DIRECT 绕过了页缓存的写回机制，必须手动 fsync 持久化
4. **tmpfs 不支持**：/dev/shm、/tmp（多数为 tmpfs）不支持 O_DIRECT

### 性能影响

- **延迟更可预测**：无页缓存的不可控延迟
- **内存效率更高**：避免页缓存与应用缓存双份占用
- **吞吐量更高**：大顺序 I/O 场景，绕过页缓存可减少 CPU 开销
