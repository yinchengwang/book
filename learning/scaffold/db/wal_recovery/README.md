# wal_recovery — WAL 与崩溃恢复

## 日志类型
- WAL：Write-Ahead Log，写前日志
- Redo：重做已提交事务
- Checkpoint：一致性恢复点

## 编译运行
```bash
make && ./test
```

## 输出说明

运行后将看到：
1. 事务 BEGIN/COMMIT 日志
2. INSERT/UPDATE 操作日志（含 LSN、txn_id、key、value）
3. 检查点生成
4. 崩溃后从检查点恢复

## WAL 日志格式

每条记录 24 字节头部 + 变长数据：
```
┌────────────────────────────────┐
│ type (1B) | size (3B)         │
│ lsn (4B) | txn_id (4B)        │
│ prev_lsn (4B) | checksum (4B) │
├────────────────────────────────┤
│ key_len (4B) | key            │
│ val_len (4B) | value          │
└────────────────────────────────┘
```
