# ACID 特性 - 工程对照

## 本学习卡演示的 ACID 概念在工程轨的实现对照

### 1. 原子性（Atomicity）实现

**Undo Log 回滚机制**

- **工程实现位置**: `engineering/include/db/concurrency/mvcc.h`（第 61-84 行）
- **关键结构**: `mvcc_undo_record_t`（Undo 日志记录）
- **核心字段**:
  - `txn_id`: 所属事务 ID
  - `type`: 操作类型（INSERT/UPDATE/DELETE）
  - `old_data`: 旧数据（用于回滚）
  - `prev_undo`: 链式 Undo 记录指针

**回滚流程**（参考 `mvcc_undo_apply()`）:
1. 从 Undo 日志中读取旧数据
2. 恢复到原始状态
3. 沿 `prev_undo` 链继续回滚直到事务开始点

### 2. 一致性（Consistency）约束

**Buffer Pool 状态管理**

- **工程实现位置**: `engineering/include/db/buf.h`（第 32-42 行）
- **状态标志**: `BufferState`（VALID/DIRTY/PINNED）
- **一致性保证**:
  - `BM_DIRTY`: 脏页标记，确保修改持久化
  - `BM_PINNED`: Pin 机制防止并发修改冲突
  - `BM_VALID`: 数据完整性校验

**页面结构约定**（参考 `BUF_PAGE_SIZE = 8192`）:
- 固定 8KB 页面大小，与 OS 页面对齐
- 页面头包含 LSN、校验和等元数据
- 页面尾保留空间用于对齐和填充

### 3. 隔离性（Isolation）实现

**MVCC 版本链**

- **工程实现位置**: `engineering/include/db/concurrency/mvcc.h`（第 47-58 行）
- **版本链结构**: `mvcc_version_t`
- **可见性判断**:
  - `xmin`: 创建事务 ID
  - `xmax`: 删除事务 ID（0 表示未删除）
  - `ctid`: 版本链指针（指向下一个版本）

**可见性规则**（参考 `mvcc_version_visible()` 第 253 行）:
```
版本对快照可见的条件：
1. xmin 事务已提交（不在活跃列表）
2. xmax = 0 或 xmax 事务未提交
3. 自身事务创建的版本始终可见
```

**快照结构**（第 97-105 行）:
- `xmin/xmax`: 活跃事务范围边界
- `xip_list`: 活跃事务 ID 数组
- 用于实现 Repeatable Read 隔离级别

### 4. 持久性（Durability）保证

**WAL（Write-Ahead Logging）原则**

- **工程实现位置**: `engineering/src/db/concurrency/mvcc_version.c`
- **核心机制**:
  1. 修改前先写入 Redo Log
  2. Redo Log 落盘后才算提交成功
  3. 数据页面可以延迟刷盘（Buffer Pool 管理）

**页面脏标记流程**:
```
修改数据 → 设置 BM_DIRTY → 写入 WAL → 
提交时 WAL 落盘 → 后台进程刷脏页 → BM_DIRTY 清除
```

**Buffer Pool 刷盘策略**（参考 buf.h）:
- LRU 置换时优先刷脏页
- Checkpoint 强制刷盘
- 后台 Writer 定期刷盘

### 5. 事务 ID 管理

**事务 ID 类型**: `mvcc_txn_id_t`（int64_t，第 21 行）

**特殊值**:
- `MVCC_INVALID_TXN_ID = 0`: 无效事务
- `MVCC_MAX_TXN_ID = INT64_MAX`: 检测回绕

**版本链指针**: `mvcc_ctid_t`（第 30-33 行）
- `block_num`: 页面号
- `offset`: 页面内槽号

### 6. GC（垃圾回收）机制

**Vacuum 配置**（第 114-126 行）:
```c
mvcc_gc_config_t:
  - vacuum_threshold: 死亡元组数量阈值
  - vacuum_scale_factor: 相对表大小的比例
  - autovacuum_enabled: 自动 Vacuum 开关
```

**GC 流程**（参考 `mvcc_gc_vacuum()`）:
1. 扫描表，识别死亡元组（xmax 已提交且 oldest_xmin > xmax）
2. 清理 Undo 日志
3. 释放页面空间
4. 更新统计信息

---

## 学习要点总结

本学习卡演示的银行转账 ACID 保证，在工程轨有完整实现：

| 学习卡概念 | 工程实现文件 | 核心函数/结构 |
|-----------|-------------|--------------|
| Undo Log 回滚 | `mvcc.h` | `mvcc_undo_record_t`, `mvcc_undo_apply()` |
| 余额约束检查 | `buf.h` | `BufferState`, `BM_VALID` |
| 版本链可见性 | `mvcc.h` | `mvcc_version_t`, `mvcc_version_visible()` |
| WAL 持久化 | `mvcc_version.c` | 版本创建时记录 Undo |
| 快照隔离 | `mvcc.h` | `mvcc_snapshot_t`, `xip_list` |
| 死亡元组清理 | `mvcc.h` | `mvcc_gc_vacuum()` |

建议后续深入阅读：
- `engineering/src/db/concurrency/mvcc_visibility.c`（可见性判断详细实现）
- `engineering/src/db/concurrency/mvcc_snapshot.c`（快照创建与管理）
- `engineering/src/db/concurrency/mvcc_version.c`（版本链操作）