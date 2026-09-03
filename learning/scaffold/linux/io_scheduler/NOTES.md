# I/O 调度器工程对照笔记

## I/O 调度策略选择

### 数据库场景

**MySQL/InnoDB**：数据文件用 WAL + DoubleWrite Buffer，`innodb_use_native_aio = 1` 可启用 Linux 原生 AIO 绕过调度器。

**PostgreSQL**：`effective_io_concurrency = 2` 限制并发 I/O 请求，防止调度器队列过长导致延迟增加。

### 存储设备推荐

| 设备 | 推荐调度器 | 理由 |
|------|----------|------|
| HDD | BFQ / mq-deadline | 需要寻道优化 |
| SATA SSD | mq-deadline / none | 无寻道，合并即可 |
| NVMe | none | 硬件自行调度 |
| RAID | mq-deadline | 合并跨磁盘 I/O |

### 工程实践

1. **WAL 日志盘用 deadline**：优先保证持久化写入的截止时间
2. **数据盘用 none**：SSD 场景下内核调度器多余
3. **混合负载**：Kyber 自动适应读/写比例
