# Linux 数据同步（fdatasync）学习笔记

本文档记录数据同步机制在项目工程代码中的实际应用对照。

## 工程对照

### 1. WAL 日志刷盘策略

WAL（Write-Ahead Logging）是数据库持久性的核心，其刷盘策略直接决定事务提交的性能和可靠性：

```c
// engineering/src/db/storage/wal/wal.c:545-559
int wal_flush(wal_t *wal) {
    if (!wal || wal->buffer_used == 0) return 0;

    /* 计算起始偏移 */
    uint64_t start_lsn = (wal->current_lsn * 1024 - wal->buffer_used) / 1024;
    uint64_t offset = WAL_HEADER_SIZE + start_lsn * 1024;

    // 使用 pwrite 写入 WAL 缓冲区数据
    if (disk_pwrite(wal->file, offset, wal->buffer, wal->buffer_used)
        != (ssize_t)wal->buffer_used) {
        wal_set_error(wal, "Failed to flush WAL");
        return -1;
    }

    wal->buffer_used = 0;
    return 0;
}
```

**当前 WAL flush 分析：**

当前 `wal_flush` 仅将 WAL 缓冲区数据通过 `pwrite` 写入文件，但未显式调用 `fsync` 或 `fdatasync`。数据可能仍在内核页缓存中，未真正落盘。在完整实现中，应在 `pwrite` 后补充同步调用：

```c
// 改进：WAL flush 后显式同步
int wal_flush(wal_t *wal) {
    // ... pwrite 写入数据 ...

    // 使用 fdatasync 而非 fsync:
    // - WAL 日志不关心 mtime 更新
    // - 仅在文件扩展时才需要更新 inode 大小
    if (fdatasync(wal->fd) < 0) {
        wal_set_error(wal, "Failed to fdatasync WAL");
        return -1;
    }
    return 0;
}
```

### 2. fsync vs fdatasync 在 WAL 场景的选择

| 场景 | 推荐 | 原因 |
|------|------|------|
| WAL 日志刷盘 | fdatasync | 不关心 mtime，减少 inode I/O |
| 检查点（checkpoint） | fsync | 需要确保完整元数据一致性 |
| 数据文件刷盘 | fsync | 文件大小可能变化，需更新 inode |
| 事务提交 | fdatasync | 性能最优，满足持久性要求 |

### 3. 批量提交策略在 WAL Buffer 中的体现

```c
// engineering/src/db/storage/wal/wal_buf.c:130-136
bool wal_buf_needs_flush(wal_buf_t *wb, BufferDesc *buf) {
    // 检查 buffer 的 last_written 是否超过上次 flush 的 LSN
    // 通过延迟 flush 实现批量提交效果
    return buf->last_written > wb->last_flush_lsn;
}

// wal_buf.c:238-247
// 需要等待 LSN 持久化时的同步等待
if (lsn <= wb->last_flush_lsn) {
    return 0;  // 已经刷盘，无需等待
}
if (wal_flush(wb->wal) != 0) {
    return -1;
}
wb->sync_waits++;  // 统计同步等待次数
```

**批量提交的核心思想：**

```
逐条提交:  write → fdatasync → write → fdatasync → ...
          (每次 I/O 延迟叠加)

批量提交:  write → write → write → ... → fdatasync
          (积攒多条记录，一次 I/O 完成)
```

批量提交通过 `wal_buf` 将多条 WAL 记录积攒到缓冲区中，仅在实际需要持久化保证时（如事务提交、Buffer 驱逐前）才调用 `wal_flush`。这种"组提交"（Group Commit）策略是数据库实现高吞吐事务处理的关键。

### 4. O_SYNC/O_DSYNC 与显式同步的取舍

```c
// 方案 A: O_DSYNC 打开 —— 每次 write 自动同步
int fd = open("wal.log", O_WRONLY | O_DSYNC);
write(fd, record, size);  // 隐含 fdatasync，阻塞等待

// 方案 B: 显式 fdatasync —— 应用层控制同步时机
int fd = open("wal.log", O_WRONLY);
write(fd, record1, size);  // 仅写缓存
write(fd, record2, size);  // 仅写缓存
fdatasync(fd);             // 批量刷盘
```

**方案对比：**

| 特性 | O_DSYNC | 显式 fdatasync |
|------|---------|----------------|
| 灵活性 | 低（每次写都同步） | 高（任意时机同步） |
| 批量优化 | 不支持 | 支持（组提交） |
| 实现复杂度 | 简单 | 需要维护 buffer |
| 适用场景 | 简单应用 | 数据库系统 |

数据库系统无一例外选择显式 `fdatasync` 方式，因为 WAL 的本质就是批量积攒 + 批量刷盘。

### 5. disk_sync 的统一抽象

```c
// engineering/src/db/storage/page/disk.c:380-383
int disk_sync(db_file_t *file) {
    if (!file) return -1;
    return file_sync(file->fd);  // Linux 下映射到 fsync
}
```

工程代码通过 `disk_sync` 提供了跨平台的同步抽象层。当前实现使用 `fsync`，对于数据库文件（大小可能变化）这是合适的选择；而对于 WAL 专用文件，使用 `fdatasync` 可以获得更好的性能。

## 参考源码

- `engineering/src/db/storage/wal/wal.c` — WAL 日志管理（wal_flush 刷盘实现）
- `engineering/src/db/storage/wal/wal_buf.c` — WAL Buffer 协调（批量提交、同步等待统计）
- `engineering/src/db/storage/page/disk.c` — 磁盘同步抽象（disk_sync）
