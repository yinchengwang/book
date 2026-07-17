# 快照隔离（Snapshot Isolation）- 工程对照

## 本学习卡演示的快照隔离在工程轨的实现对照

### 1. 快照隔离（SI）vs Read Committed（RC）

| 特性 | SI | RC |
|------|----|----|
| 快照时机 | 事务开始时 | 每条语句执行时 |
| 读取一致性 | 事务内一致 | 语句间可能变化 |
| 并发性 | 较低 | 较高 |
| Write-Skew | 可能 | 可能 |

### 2. 工程实现位置

**快照创建**: `engineering/src/db/concurrency/mvcc_snapshot.c`

```c
mvcc_snapshot_t *mvcc_snapshot_create(
    mvcc_txn_id_t xmin,           // 最小活动事务 ID
    mvcc_txn_id_t xmax,           // 最大已分配事务 ID
    const mvcc_txn_id_t *active_txns,
    int active_count);
```

### 3. First-Writer-Wins 实现

**冲突检测**: `engineering/src/db/concurrency/mvcc_visibility.c`

```c
bool mvcc_check_write_conflict(const mvcc_snapshot_t *snapshot,
                               mvcc_txn_id_t row_xmax,
                               mvcc_txn_id_t cur_txn_id);
```

**冲突处理流程**：
1. 事务 T 读取行 R，xmax = 0
2. 事务 T 尝试更新 R，检查 row_xmax
3. 如果 row_xmax 在 T 的快照中已提交 → 冲突
4. 冲突时 T 回滚（First-Writer-Wins）

### 4. Write-Skew 异常

**场景**：
```
约束: A + B >= 0
初始: A = 100, B = 100

T1: BEGIN; SELECT A;  -- 读到 A=100
T2: BEGIN; SELECT B;  -- 读到 B=100
T1: UPDATE A = A - 100;  -- A = 0，通过检查
T2: UPDATE B = B - 100;  -- B = 0，通过检查
T1: COMMIT
T2: COMMIT

结果: A = 0, B = 0，违反 A + B >= 0！
```

**解决方案**：Serializable 隔离级别使用串行化执行

### 5. 隔离级别对比

| 隔离级别 | 脏读 | 不可重复读 | 幻读 | Write-Skew |
|---------|------|-----------|------|-----------|
| Read Uncommitted | 可能 | 可能 | 可能 | 可能 |
| Read Committed | ✗ | 可能 | 可能 | 可能 |
| Repeatable Read | ✗ | ✗ | 可能 | 可能 |
| Snapshot Isolation | ✗ | ✗ | ✗ | 可能 |
| Serializable | ✗ | ✗ | ✗ | ✗ |

### 6. 工程文件对照

| 学习卡概念 | 工程实现文件 | 关键函数 |
|-----------|-------------|---------|
| 快照创建 | mvcc_snapshot.c | `mvcc_snapshot_create()` |
| 快照复制 | mvcc_snapshot.c | `mvcc_snapshot_copy()` |
| 可见性判断 | mvcc_visibility.c | `mvcc_version_visible()` |
| 冲突检测 | mvcc_visibility.c | `mvcc_check_write_conflict()` |

---

## 总结

本学习卡演示了快照隔离的核心概念：
1. SI 与 RC 的区别在于快照刷新时机
2. First-Writer-Wins 通过检测 xmax 冲突实现
3. Write-Skew 是 SI 的固有局限性
4. Serializable 通过串行化执行避免 Write-Skew