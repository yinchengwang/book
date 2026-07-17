# 工程对照笔记

## 现实数据库的死锁处理

### PostgreSQL

PostgreSQL 采用**死锁检测 + 超时回滚**策略：

```c
// PG 核心死锁检测逻辑（简化示意）
// 源码位置：src/backend/storage/lmgr/deadlock.c

typedef struct {
    int     waitThreshold;    // 等待时间阈值
    bool    deadlockFound;    // 是否检测到死锁
} DeadLockInfo;

// PG 使用 wait threshold 来平衡检测开销
// 默认 1s 检测一次，可配置 deadlock_timeout 参数
```

**关键参数**：
- `deadlock_timeout`：检测间隔（默认 1s）
- `lock_timeout`：锁等待超时（防止无限等待）
- `statement_timeout`：语句执行超时

### MySQL InnoDB

InnoDB 采用**等待图 + 超时 + 牺牲者选择**：

```c
// InnoDB 死锁检测核心
// 源码位置：storage/innobase/lock/lock0deadlock.cc

struct lock_t {
    trx_t*       trx;           // 所属事务
    dict_table_t* table;        // 表
    lock_mode_t  mode;          // 锁模式（IX、IS、X、S）
    hash_node_t  hash;          // 哈希链表
    lock_t*      prev;          // 前驱
    lock_t*      next;          // 后继
};

// InnoDB 维护多个锁链表：
// - table-level locks
// - record-level locks
// - index-level locks
```

**检测策略**：
1. 每当锁请求等待时触发检测
2. 构建等待图（Wait-For Graph）
3. 检测到环后，选择回滚代价最小的事务
4. 回滚量由 `innodb_deadlock_detect_count` 限制

### SQLite

SQLite 采用**急切回滚策略**：
- 写锁请求被拒绝时立即回滚当前写事务
- 不构建复杂的等待图
- 适用于低并发场景

## 工程实现对比

| 特性 | PostgreSQL | MySQL InnoDB | SQLite |
|------|------------|--------------|--------|
| 检测时机 | 定时检测 | 等待时触发 | 无检测 |
| 检测算法 | 等待图 + DFS | 等待图 + 环路搜索 | N/A |
| 超时机制 | deadlock_timeout | innodb_lock_wait_timeout | busy_timeout |
| 牺牲者选择 | 等待时间最长 | 工作量最少 | 当前事务 |
| 检测粒度 | 页面/元组锁 | 行级锁 | 数据库锁 |

## 工程中的优化

### 1. 检测时机优化

- **不要每次加锁都检测**：开销太大
- **按比例检测**：锁等待数量超过阈值时触发
- **随机检测**：避免所有事务同时检测

### 2. 等待图压缩

- **超时边剪枝**：长期等待的边可能是死锁源
- **依赖压缩**：合并相同资源的等待链

### 3. 牺牲者选择启发式

```
启发式规则（InnoDB）：
1. 选择修改最少行的事务
2. 选择最年轻的事务（最近开始）
3. 优先回滚只读事务（如果标记了）
4. 限制单个事务回滚量
```

## 本项目学习代码与工程实现对照

| 学习代码 | 工程实现 | 说明 |
|----------|----------|------|
| `WaitEdge` 结构 | `lock_t` 结构 | 表示等待关系 |
| `dfs_detect_cycle()` | `deadlock_check()` | 环检测算法 |
| `resolve_deadlock_by_victim()` | InnoDB victim selection | 牺牲者选择 |
| 超时检测 | `deadlock_timeout` | 定时检测 |

## 延伸学习

1. **分布式死锁**：多节点数据库的分布式等待图
2. **乐观并发控制**：通过版本号避免死锁
3. **两阶段锁的变体**：严格两阶段锁（Strict 2PL）、强严格两阶段锁（SS2PL）
4. **图数据库死锁**：属性图中的死锁模式

## 课后思考

1. 为什么 PostgreSQL 选择定时检测而不是每次等待时检测？
2. InnoDB 的行级锁如何构建等待图？
3. 如果两个事务分别持有对方的锁并请求对方的资源，这是死锁吗？
4. 分布式数据库如何处理跨节点死锁？
