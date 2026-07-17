# fsync 工程对照笔记

## `engineering/src/db/storage/wal.c` WAL 刷盘

### WAL 写入路径

PostgreSQL 的 WAL（Write-Ahead Log）在 `wal.c` 中通过 `XLogWrite()` 函数实现日志持久化：

```c
// 简化版 WAL 写入逻辑
void XLogWrite(XLogwrtRqst WriteRqst) {
    // 1. 组装 WAL 页面（wal_buf.c）
    // 2. write() 到 WAL 文件
    // 3. 如果是同步提交 → fsync()/fdatasync()
    // 4. 更新共享 WAL 位置（XLogCtl->LogwrtResult）
}
```

### fsync 调用链

```
XLogWrite()
  └── issue_xlog_fsync()
      └── pg_fsync(fd)           // 平台封装
          └── fdatasync(fd)      // Linux: 数据同步
```

### 组提交优化

在 `wal.c` 中，`XLogInsert()` 返回后不立即 fsync — 由 `WalWriter` 进程批量处理：

```c
// WAL 写入器主循环
while (WAL_INSERTED++) {
    // 检查共享缓存中是否有新的 WAL 记录
    XLogPageRead(&page, startptr);
    XLogWrite(request);
    // 周期性 fsync
    WaitLatch(MyLatch, WL_LATCH_SET, wal_writer_delay);
}
```

### 同步级别

| `synchronous_commit` | WAL 行为 | 延迟 |
|----------------------|---------|------|
| `remote_apply` | 等待 standby 应用 WAL | 最高 |
| `on` (默认) | 本地 fsync 后即返回 | 中 |
| `remote_write` | 本地 fsync，standby 写入即可 | 低 |
| `local` | 仅本地 fsync | 中 |
| `off` | 不等待 fsync | 最低 |

### 工程要点

1. **fdatasync 优于 fsync**：WAL 大小通常在写入后改变，元数据同步一次即可
2. **组提交**：wal_writer_delay = 200ms 设置合理的 fsync 频率
3. **不要过度 fsync**：每个 write 后都 fsync 会让性能退化 100 倍以上
