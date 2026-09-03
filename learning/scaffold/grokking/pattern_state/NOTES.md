# 状态模式 学习笔记

## 核心概念

- **状态模式 (State)**: 对象内部状态变化时改变其行为
- **状态接口**: 所有状态类实现相同接口
- **状态转换**: 由状态类自身决定下一个状态
- **消除分支**: 用多态替代 if-else/switch 状态判断

## 状态模式 vs 策略模式

| 维度 | 状态模式 | 策略模式 |
|------|----------|----------|
| 目的 | 状态驱动行为变化 | 算法可替换 |
| 转换控制 | 状态类自身控制 | 上下文控制 |
| 状态/策略数量 | 有限且已知 | 可无限扩展 |
| 耦合 | 状态之间可能耦合 | 策略之间独立 |

## 工程对照

状态机在数据库系统中无处不在。`engineering/src/db/txn/` 中的事务管理器维护了每个事务的状态机：IN_PROGRESS -> COMMITTED 或 IN_PROGRESS -> ABORTED。`engineering/include/db/lock.h` 中定义的锁管理器状态 (LOCK_ACQUIRED, LOCK_WAITING, LOCK_RELEASED) 也是典型的状态机。`engineering/src/db/buf/bufmgr.c` 中的页面状态（干净/脏/固定/未固定）同样符合状态模式。最典型的应用是 PostgreSQL Wire 协议的状态机——`engineering/src/db/core/db_server.c` 处理客户端连接时，根据连接状态执行不同行为。

## 面试要点

1. 状态转换可以用状态表驱动, 不必每个状态一个类
2. 有限状态机 (FSM) 比状态模式更适用于简单场景
3. TCP 连接的状态转换是经典面试题
