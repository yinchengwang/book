# MVCC 原理学习卡

## 简介

MVCC（Multi-Version Concurrency Control，多版本并发控制）是现代关系数据库实现高效并发访问的核心机制。与传统的锁机制不同，MVCC 通过为每次写操作创建新的数据版本来实现读写分离，使读操作永不阻塞写操作，写操作也不阻塞读操作。

本 scaffold 演示 MVCC 的三大核心要素：

1. **版本链（Version Chain）**：每个元组保存多个历史版本，通过 xmin/xmax/ctid 字段串联
2. **事务快照（Snapshot）**：事务开始时捕获的数据库状态视图，记录哪些事务正在运行
3. **可见性判断（Visibility）**：根据快照规则，决定返回版本链中的哪个版本

## 编译运行

```bash
make          # 编译
./mvcc        # 运行演示
make clean    # 清理
```

## 核心概念

### xmin / xmax

- **xmin**：创建该元组版本的事务 ID
- **xmax**：删除该元组版本的事务 ID（0 表示未被删除）

### ctid（版本链指针）

指向元组的下一个版本，形成从新到旧的链表。UPDATE 操作会：
1. 旧版本标记 xmax = 当前事务 ID
2. 创建新版本，链入链表头部

### 快照结构

事务开始时，快照记录：
- **xmin**：活跃事务的最小 ID
- **xmax**：已分配的最大事务 ID
- **xip_list**：所有活跃事务 ID 列表

### 可见性规则

版本 V 对快照 S 可见的条件：

```
可见(V, S) = xmin 已提交 && (xmax == 0 || xmax 未提交)
```

即：
- 创建事务已提交（或为自身事务）
- 版本未被其他已提交事务删除

## 学习要点

1. **读写不互斥**：读操作看到的是快照一致的旧版本，不受并发写入影响
2. **写写冲突检测**：通过检查目标行的 xmax 判断是否存在未提交的并发更新
3. **垃圾回收**：旧版本累积需要 GC（PostgreSQL 中为 VACUUM）清理
4. **Undo 日志**：用于回滚和回放，支持事务的回退操作

## 参考

- 工程实现：`engineering/src/db/concurrency/mvcc_version.c`
- 快照管理：`engineering/src/db/concurrency/mvcc_snapshot.c`
- 可见性判断：`engineering/src/db/concurrency/mvcc_visibility.c`
- 接口定义：`engineering/include/db/concurrency/mvcc.h`
