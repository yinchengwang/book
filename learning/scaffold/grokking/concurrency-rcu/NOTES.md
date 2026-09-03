# RCU (Read-Copy-Update) 学习笔记

## 核心概念

**RCU 三阶段**：
1. **Read**：读者通过 rcu_dereference 读取共享指针，读侧临界区无锁
2. **Copy-Update**：写者创建新数据副本、修改、然后原子更新指针
3. **Reclaim**：写者等待宽限期（所有在旧指针期间开始读的读者完成）后回收旧数据

**RCU 优势**：读侧性能极高（仅一个原子加载），不存在锁竞争和 cacheline bouncing。适用于读远多于写的场景，如 Linux 内核的路由表、文件系统 dentry 缓存。

## 用户态 vs 内核态 RCU

| 特性 | 用户态（本演示） | Linux 内核 |
|------|-----------------|------------|
| 读侧开销 | atomic_fetch_add（需计数） | 编译器屏障即可 |
| 宽限期检测 | epoch 轮询 + thrd_yield | quiescent state 检测 |
| 实现复杂度 | 简单近似 | 精巧（Tree RCU、nocb 等变体） |
| 适用场景 | 概念学习 | 生产环境 |

## 工程对照

本项目数据库存储引擎使用 Buffer Pool 的 Clock-Sweep 置换策略来管理并发页面访问，其中页面固定（page pin）机制与 RCU 的"读者计数"思路相似——在访问页面时 pin 住页面防止被驱逐，完成后 unpin。区别在于：
- **Lock-based**（数据库 Buffer Pool）：读页需要持有共享锁或 pin 计数。写页时需要排他锁，读者和写者互斥。
- **RCU 思路**：读者完全无锁，写者通过 Copy 创建新版本，用宽限期保证旧版本无人使用后再回收。这在吞吐量上远优于锁方案，但要求数据有版本概念且内存开销略大。

在 PostgreSQL 等生产级数据库中，MVCC（多版本并发控制）的"读不阻塞写、写不阻塞读"与 RCU 有异曲同工之处，都是通过维护多个版本来避免读写冲突。

## 面试要点

1. 理解 RCU 的"三阶段"模式（Read-Copy-Update），尤其是"Update 后还要等宽限期才能 Reclaim"
2. 对比读写锁：RCU 读侧完全 wait-free，而读写锁读侧也需要原子操作获取锁
3. RCU 要求数据有指针级别间接性（通过指针切换版本）
4. 宽限期（Grace Period）是 RCU 最难正确实现的部分
5. 在用户态编程中，可用 epoch-based reclamation (EBR) 近似 RCU
