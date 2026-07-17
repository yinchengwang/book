# Barrier 学习笔记

## 核心概念

Barrier（屏障/栅障）是多线程同步原语，用于在多个线程之间建立汇合点。所有线程到达 barrier 后同时放行，继续后续执行。

**关键区别**：
- **Mutex**：保护临界区互斥访问，解决"竞争"
- **Condition Variable**：等待某个条件成立，解决"条件等待"
- **Barrier**：等待 N 个线程全部到达，解决"阶段同步"

## 三种同步机制对比

| 维度 | Mutex | Cond Var | Barrier |
|------|-------|----------|---------|
| 目的 | 互斥访问 | 条件等待 | 线程汇合 |
| 参与者 | 任意线程 | 一个信号 + N 个等待者 | 固定 N 个线程 |
| 重入 | 部分支持（递归锁） | 否 | 否 |
| 典型模式 | lock/unlock | wait/signal | wait/wait |
| 等待条件 | 锁被释放 | 谓词为真 | N 个线程到达 |

## 工程对照

本项目 `engineering/src/db/consensus/raft.c` 实现了 Raft 共识算法，其中 leader 选举本质上是一种 barrier 模式的变体：follower 节点等待 leader 的心跳超时，然后同时发起选举。不同之处在于：

1. **超时偏移**：Raft 使用随机化超时（`random_election_timeout`）来避免"同步选举风暴"，而 barrier 要求所有线程精确到达同一点后放行。Barrier 的"全部到达"语义在分布式系统中需要引入故障检测和超时机制才能实现——这正是 Raft 的超时设计要解决的问题。

2. **故障容错**：Barrier 假设所有线程最终都能到达，一旦某个线程崩溃，所有线程永久阻塞。工程系统中的 barrier-like 同步（如分布式屏障 `MPI_Barrier`、Spark 的 stage 边界）都引入了超时和故障恢复机制。

3. **状态复制**：Barrier 的"分阶段计算"模式与 Raft 的"log entry 提交"在结构上相似——每个阶段对应一个 term/round，所有节点在进入下一轮前必须确认当前轮已达成一致。区别在于 Raft 通过 majority 而非全部节点达成一致。

此外，数据库查询执行引擎中的 exchange operator（如 PostgreSQL 的 `Gather` 节点）也使用 barrier 来同步并行 worker 的输出。

## 面试要点

1. Barrier 适合"分治-汇合"模式：每个线程处理子问题后汇总
2. `pthread_barrier_wait` 返回值 `PTHREAD_BARRIER_SERIAL_THREAD` 可用于指定一个线程执行串行动作
3. Barrier 与 cond var 的关键差异：cond var 需要手动管理谓词，barrier 不需要
4. 工程中 barrier 常用于 SIMD/GPU 编程中的 warp 内同步
