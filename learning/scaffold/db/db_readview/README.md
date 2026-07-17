# ReadView 机制学习卡

## 简介

ReadView（读视图）是数据库 MVCC（多版本并发控制）机制的核心组件，用于实现事务的隔离性。

### 什么是 ReadView？

ReadView 是事务在某个时间点看到的数据库状态快照。它包含：

| 字段 | 说明 |
|------|------|
| `xmin` | 快照创建时的最小活动事务 ID，小于 xmin 的事务已提交 |
| `xmax` | 快照创建时的最大事务 ID，大于等于 xmax 的事务未开始 |
| `xip_list` | 快照创建时正在执行的事务 ID 列表 |

### 可见性判断规则

```
XidInMVCCSnapshot(xid, ReadView):
  1. if xid < xmin       → 可见（事务已提交）
  2. if xid >= xmax      → 不可见（事务未开始）
  3. if xid in xip_list  → 不可见（事务正在执行）
  4. 否则                → 可见（事务已提交）
```

### MVCC 工作原理

```
时间线:
T1─────┬─────► 事务 10 插入 "Hello"
       │
T2────────────► 事务 20 修改为 "World"
       │
T3────────────────► 事务 30 删除
       │
T4──────────────────► 事务 40 插入 "New"
       │
TX─快照点─┬─► 不同事务看到不同版本
```

## 编译与运行

```bash
# 编译
gcc -std=c11 -Wall -o readview main.c

# 运行
./readview
```

或使用 Makefile：

```bash
make      # 编译
make run  # 编译并运行
make clean # 清理
```

## 学习要点

1. **快照语义**：ReadView 决定了事务看到的数据版本
2. **隔离级别**：`READ COMMITTED` 每个语句创建新快照，`REPEATABLE READ` 事务使用同一快照
3. **版本链**：元组通过版本链支持并发更新
4. **垃圾回收**：旧版本需要 VACUUM 清理

## 扩展阅读

- PostgreSQL `src/include/storage/snapshot.h`
- PostgreSQL `src/backend/utils/time/tqual.c` 可见性判断
