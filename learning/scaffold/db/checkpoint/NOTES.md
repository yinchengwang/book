# Checkpoint 机制详解

## 一、概述

Checkpoint（检查点）是数据库 WAL（Write-Ahead Logging）系统中用于缩短崩溃恢复时间的重要机制。通过定期将脏页刷新到磁盘并记录检查点位置，系统在崩溃后只需从最近的检查点开始恢复，而无需扫描整个 WAL 日志。

## 二、Sharp Checkpoint vs Fuzzy Checkpoint

### 2.1 Sharp Checkpoint（完全检查点）

Sharp Checkpoint 要求将**所有脏页**一次性刷盘，其执行过程如下：

1. **阻塞阶段**：停止所有新事务的写入
2. **刷盘阶段**：将 Buffer Pool 中的所有脏页写入磁盘
3. **记录阶段**：将检查点 LSN 写入 WAL 日志
4. **恢复阶段**：允许新事务继续

**优点**：
- 逻辑简单，恢复时只需从检查点开始
- 保证所有已提交事务的修改都已持久化
- 便于进行数据库备份

**缺点**：
- 阻塞时间长，对在线业务影响大
- 磁盘 I/O 压力大
- 无法做到高可用

**代码对应**（`wal.c` 中的 `WAL_STATE_CHECKPOINT` 状态）：
```c
// wal_need_checkpoint 返回 true 时触发
// 会阻塞所有写操作直到所有脏页刷盘完成
```

### 2.2 Fuzzy Checkpoint（模糊检查点）

Fuzzy Checkpoint 采用分段执行，只刷检查点**开始前**产生的脏页，分为三个阶段：

1. **BEGIN 阶段**：记录当前脏页列表，写入检查点开始记录
2. **DO 阶段**：将已存在的脏页刷盘，允许新事务继续产生脏页
3. **END 阶段**：记录检查点结束状态和最终脏页列表

**优点**：
- 不阻塞新事务，对在线业务友好
- 可在后台周期性执行
- 支持高可用部署

**缺点**：
- 恢复时需要扫描检查点之后的日志
- 实现复杂，需要处理并发场景
- 可能存在检查点期间产生的脏页需要额外处理

**代码对应**（`wal.c` 中的 `wal_write_checkpoint`）：
```c
uint64_t wal_write_checkpoint(wal_t *wal, const uint32_t *dirty_pages, size_t num_pages) {
    // 写入检查点记录，但不强制刷所有脏页
    // 脏页刷盘由后台线程异步完成
}
```

## 三、LSN 水位管理

### 3.1 LSN（Log Sequence Number）

LSN 是 WAL 中每条日志记录的唯一序号递增的值。在 `wal.c` 中：

```c
struct wal_s {
    uint64_t    current_lsn;     // 当前 LSN
    uint64_t    checkpoint_lsn;  // 上次检查点 LSN
};
```

### 3.2 水位阈值策略

系统维护两个水位线来控制检查点触发时机：

- **高水位（High Water）**：当 `current_lsn - checkpoint_lsn >= 高水位阈值` 时，触发检查点
- **低水位（Low Water）**：检查点完成后，等待差值降至低水位后再允许下一次检查点

**代码对应**（`wal.c` 中的 `wal_need_checkpoint`）：
```c
bool wal_need_checkpoint(wal_t *wal) {
    if (!wal) return false;
    // 当 LSN 超过阈值时需要检查点
    return wal->current_lsn - wal->checkpoint_lsn > 1000;
}
```

### 3.3 LSN 水位与恢复

- 检查点 LSN 之前的日志记录对应的修改都已刷盘，可以安全忽略
- 检查点 LSN 之后的日志记录需要根据恢复类型进行处理：
  - **Redo**：重做已提交事务的修改
  - **Undo**：撤销未提交事务的修改

## 四、脏页刷盘策略

### 4.1 全量刷盘（Sharp 策略）

将 Buffer Pool 中的所有脏页一次性写入磁盘，适用于：
- 停机维护前的最终检查点
- 数据库备份前的快照一致性保证
- 系统初始化时的首次加载

### 4.2 按需刷盘（Fuzzy 策略）

只刷满足以下条件的脏页：
- 该页面的 `rec_lsn <= checkpoint_lsn`（检查点开始前的修改）
- 该页面已被其他页面置换出 Buffer Pool

### 4.3 LSN 驱动刷盘

根据 LSN 水位动态调整刷盘节奏：
- 水位过高时增加刷盘频率
- 水位正常时减少不必要的 I/O
- 结合后台线程实现异步刷盘

## 五、总结

| 特性 | Sharp Checkpoint | Fuzzy Checkpoint |
|------|------------------|-------------------|
| 阻塞时间 | 长 | 短 |
| 实现复杂度 | 低 | 高 |
| 适用场景 | 停机维护、备份 | 在线运行、HA |
| 恢复复杂度 | 低 | 中 |
| LSN 水位依赖 | 不依赖 | 依赖 |

`wal.c` 中的实现通过 `wal_state_t` 状态机管理检查点过程，`wal_write_checkpoint` 记录检查点位置，`wal_need_checkpoint` 判断触发时机，共同构成了完整的检查点机制。
