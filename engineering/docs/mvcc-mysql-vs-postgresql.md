# MVCC 实现方案对比：MySQL vs PostgreSQL

## 对比概述

| 维度 | MySQL (InnoDB) | PostgreSQL |
|------|----------------|------------|
| 版本存储 | Undo 日志 (回滚段) | 行内多版本 (Heap) |
| 可见性判断 | Read View + Undo 链 | xmin/xmax + 快照 |
| 垃圾回收 | Purge 线程 | VACUUM |
| 空间开销 | Undo 表空间 | 行膨胀 (死元组) |
| 回滚效率 | 快（Undo 已记录） | 快（Abort 标记） |
| 长事务影响 | Undo 空间膨胀 | 表膨胀 (VACUUM 延迟) |

## MySQL InnoDB 的 MVCC

### 版本存储：Undo Log

```
InnoDB 的版本链：

行数据：           Undo Log：
┌────────────┐     ┌──────────────────┐
│ Row (当前) │     │ Undo Record 3    │
│ TRX_ID     │ →   │ TRX_ID: 300      │
│ ROLL_PTR ──┘     │ Old: "v2"        │
│ data: "v3" │     │ ROLL_PTR ────────┼──→ Undo Record 2
└────────────┘     └──────────────────┘   │ TRX_ID: 200
                                          │ Old: "v1"
    通过 ROLL_PTR 指针                     │ ROLL_PTR ──→ Undo Record 1
    串联起所有版本                          ...

关键区别：当前行在 Heap 中，旧版本在独立的 Undo 段
```

### Read View

```c
/* InnoDB 的 Read View */
struct ReadView {
    trx_id_t m_low_limit_id;    // 大于此 ID 的事务不可见
    trx_id_t m_up_limit_id;     // 小于此 ID 的事务已提交
    trx_id_t m_creator_trx_id;  // 创建此 Read View 的事务
    trx_ids_t m_ids;            // 活跃事务 ID 集合
    bool m_closed;              // Read View 是否关闭
};
```

```
可见性判断：

1. 如果 trx_id == m_creator_trx_id：可见（自己的修改）
2. 如果 trx_id < m_up_limit_id：可见（已提交）
3. 如果 trx_id >= m_low_limit_id：不可见（未开始）
4. 如果 trx_id 在 m_ids 中：不可见（未提交）
5. 否则：可见（已提交）

如果不可见，沿 ROLL_PTR 链找到上一个可见版本
```

### Purge 机制

```
InnoDB 的 Purge 流程：

┌─────────────────────────────────────────────┐
│ Purge 线程（后台运行）                        │
│                                             │
│ 1. 等待所有活跃事务都小于某个 Undo 记录       │
│ 2. 清理不再需要的 Undo 记录                  │
│ 3. 清理删除标记的记录                        │
│ 4. 回收 Undo 表空间                         │
│                                             │
│ 与 PostgreSQL VACUUM 的区别：                │
│ - Purge 是增量式的，VACUUM 是批量式的         │
│ - Purge 不需要显式触发                      │
│ - Purge 的目标是 Undo 记录而非 Heap           │
└─────────────────────────────────────────────┘
```

## PostgreSQL 的 MVCC

### 版本存储：行内多版本

```
PostgreSQL 的版本链：

Heap 页面：
┌──────────────────────────────────────────┐
│ id=1, name='Alice', xmin=100, xmax=200  │ (历史版本)
├──────────────────────────────────────────┤
│ id=1, name='Alice Jr', xmin=200, xmax=0 │ (当前版本)
├──────────────────────────────────────────┤
│ ...                                      │
└──────────────────────────────────────────┘

关键区别：所有版本都在 Heap 页面中，xmin/xmax 区分版本
```

### 行头结构

```c
/* PostgreSQL 的行头 */
typedef struct HeapTupleHeaderData {
    TransactionId t_xmin;      // 创建此版本的事务
    TransactionId t_xmax;      // 删除此版本的事务
    union {
        CommandId t_cid;       // 命令 ID
        TransactionId t_xvac;  // VACUUM 操作的事务
    } t_field3;
    ItemPointerData t_ctid;    // 当前版本指针（指向自身或新版本）
    uint16 t_infomask2;        // 属性数和标志
    uint16 t_infomask;         // 可见性标志
    uint8  t_hoff;             // 到用户数据的偏移
    bits8  t_bits[1];          // NULL 位图
} HeapTupleHeaderData;
```

### 可见性判断

```c
/* PostgreSQL 的可见性判断 */
bool HeapTupleSatisfiesMVCC(HeapTuple htup, Snapshot snapshot) {
    // 1. 自己的修改
    if (HeapTupleHeaderXminCommitted(tup) &&
        !HeapTupleHeaderXmaxCommitted(tup) &&
        TransactionIdIsCurrentTransactionId(HeapTupleGetUpdateXid(tup)))
        return true;

    // 2. xmin 未提交
    if (!HeapTupleHeaderXminCommitted(tup)) {
        if (HeapTupleHeaderXminInvalid(tup)) return false;
        if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmin(tup)))
            return true;  // 自己的修改，可见

        // 3. xmin 在快照的活跃列表中
        if (TransactionIdIsInProgress(HeapTupleHeaderGetRawXmin(tup)))
            return false;
        return false;  // xmin 未提交
    }

    // 4. xmax 已提交
    if (HeapTupleHeaderXmaxCommitted(tup)) {
        if (HeapTupleHeaderXmaxInvalid(tup)) return false;
        if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetRawXmax(tup)))
            return true;  // 可以更新自己的删除
        return false;  // 被删除
    }

    return true;  // xmin 已提交，xmax 未提交/未删除
}
```

### VACUUM 机制

```
PostgreSQL 的 VACUUM 流程：

┌─────────────────────────────────────────────┐
│ VACUUM 进程（手动或 autovacuum）             │
│                                             │
│ 1. 扫描 Heap 页面                            │
│ 2. 标识死元组（dead tuples）                 │
│ 3. 回收死元组的空间                          │
│ 4. 更新 FSM（空闲空间映射）                  │
│ 5. 清理索引中的死项                          │
│ 6. 截断表文件（如果末尾页面为空）             │
│                                             │
│ 触发阈值：                                   │
│ dead_tuples > vacuum_threshold +             │
│   vacuum_scale_factor * live_tuples          │
└─────────────────────────────────────────────┘
```

## 深度对比

### 1. 存储效率

```
MySQL InnoDB：
- Heap 中只存当前版本 ✓
- Undo 段需要独立管理 ✗
- 回滚段可能很大 ✗

PostgreSQL：
- 所有版本在同一页 ✓（减少 IO）
- 死元组占据空间 ✗
- 页填充率可能很低 ✗

实际表现：
- 小事务：两者相当
- 长事务：PostgreSQL 表现更差（VACUUM 跟不上）
- 大 UPDATE：PostgreSQL 表现更差（同时存在新旧版本）
```

### 2. 回滚效率

```
MySQL InnoDB：
- 回滚：沿 Undo 链恢复旧值
- 实现：Undo 记录已包含旧值，直接拷贝回来
- 时间：O(n)，n 是操作数

PostgreSQL：
- 回滚：标记 xmax = 当前事务
- 实现：从未提交状态改为已删除
- 时间：O(1)，只是标记变化

PostgreSQL 的回滚更快！
```

### 3. 垃圾回收

```
MySQL InnoDB (Purge)：
✅ 增量清理，平滑
✅ 不需要人工触发
❌ Undo 清理可能不及时

PostgreSQL (VACUUM)：
✅ 批量清理，效率高
❌ 需要手动触发（或 autovacuum）
❌ 可能影响性能（IO 密集型操作）
❌ 表膨胀是常见问题

结论：MySQL 的 Purge 对 DBA 更友好
```

### 4. 长事务影响

```
MySQL InnoDB：
- 长事务阻止 Purge 清理 Undo
- Undo 表空间持续增长
- 其他事务性能不受影响

PostgreSQL：
- 长事务阻止 VACUUM 清理死元组
- Heap 页面膨胀
- 表扫描变慢（需要跳过更多死元组）
- 索引膨胀

长事务在 PostgreSQL 中后果更严重！
```

### 5. 回滚段 vs 行内版本

```
MySQL InnoDB 回滚段的好处：
✅ 清理旧版本不需要触碰 Heap
✅ 行大小保持稳定
✅ Heap 页不膨胀

PostgreSQL 行内版本的好处：
✅ 不需要额外的表空间
✅ 版本查找在同一个页面中，减少 IO
✅ 一次页面读取获取所有版本

核心理念差异：
MySQL："版本是独立的，不污染主数据"
PostgreSQL："版本是行的一部分，自然管理"
```

## 选择指南

| 场景 | 推荐 |
|------|------|
| 大量小事务 | **PostgreSQL**（回滚快，VACUUM 开销小） |
| 长事务常见 | **MySQL InnoDB**（Undo 更可控） |
| 高频 UPDATE | 权衡：PostgreSQL 空间消耗更高 |
| 只读为主 | **PostgreSQL**（读路径简单） |
| 分布式 | **MySQL InnoDB**（更容易理解的行为） |

## 总结

| 维度 | MySQL InnoDB | PostgreSQL |
|------|-------------|------------|
| 版本位置 | Undo 段（分开） | Heap 页（内联） |
| 读路径 | 沿 Undo 链 | 检查行头标记 |
| 回滚速度 | O(n) | O(1) |
| 垃圾回收 | 后台 Purge | VACUUM |
| 空间膨胀 | Undo 膨胀 | Heap 膨胀 |
| DBA 友好度 | ★★★★☆ | ★★★☆☆ |

MySQL 的设计更工程化，PostgreSQL 的设计更学术化。

两者都正确实现了 MVCC，但选择了不同的权衡。

---

*文档版本：v1.0*
*最后更新：2026-07-12*
