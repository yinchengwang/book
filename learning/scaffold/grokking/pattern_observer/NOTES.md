# 观察者模式 学习笔记

## 核心概念

- **观察者模式 (Observer)**: 定义一对多依赖关系, 主题状态变化时自动通知所有观察者
- **Subject (主题)**: 维护观察者列表, 提供 attach/detach/notify 接口
- **Observer (观察者)**: 定义 update 接口, 接收主题的状态变化通知
- **发布-订阅 (Pub-Sub)**: Observer 的变体, 通过事件通道解耦发布者和订阅者, 支持更灵活的事件筛选

## 三种模式对比

| 特性 | 经典 Observer | 发布-订阅 (Pub-Sub) | 事件驱动 (Event-Driven) |
|------|---------------|---------------------|------------------------|
| 耦合度 | 观察者持有主题引用 | 完全解耦, 通过事件通道 | 完全解耦, 通过事件总线 |
| 通信方式 | 直接调用 update | 中介者转发事件 | 异步消息队列 |
| 事件过滤 | 收到所有通知, 自行过滤 | 按事件名订阅 | 按事件类型/模式匹配 |
| 是否支持异步 | 通常同步 | 可同步可异步 | 通常异步 |
| 适用场景 | GUI 组件、状态监控 | 消息中间件、事件总线 | 分布式系统、微服务 |

## 工程对照: WAL 日志系统

本仓库的 WAL (Write-Ahead Log) 系统 (`engineering/src/db/storage/wal/wal.c`) 本质上也使用了观察者思想:

- **WAL 日志 → Subject (事件源)**: WAL 产生的每条日志记录 (INSERT/UPDATE/DELETE/COMMIT) 相当于一个"事件"。日志系统本身不关心谁消费这些事件, 只是"记录并通知"。

- **Buffer Pool → Observer (观察者)**: Buffer Pool 需要根据 WAL 日志的写入情况决定脏页何时落盘 (Steal/Force 策略)。WAL 记录写入后, Buffer Pool 需要对日志数据进行 flush 调度。

- **Checkpoint → Observer (观察者)**: Checkpoint 进程监听日志增长, 当日志达到一定量级时触发检查点, 确保对应 LSN 之前的脏页已落盘。

- **Crash Recovery → Observer (观察者)**: 数据库崩溃恢复时, Recovery 模块"回放"WAL 日志, 重做已提交事务、回滚未提交事务。

对比表格:

| WAL 系统角色 | 观察者模式角色 | 说明 |
|-------------|---------------|------|
| WAL 写入器 | Subject | 产生日志事件 (INSERT/COMMIT 等) |
| Buffer Pool | Observer | 监听日志, 协调脏页刷盘 |
| Checkpoint 进程 | Observer | 监听日志量, 触发检查点 |
| Crash Recovery | Observer | 重放日志, 恢复数据库状态 |

这种设计体现了**控制反转**原则——WAL 系统的核心写入逻辑不感知外部模块的存在, 而是通过"通知"机制让外部模块自主响应。这正是观察者模式的核心价值: **降低核心路径与扩展功能之间的耦合**。

## 面试要点

1. **观察者 vs 发布-订阅**: 经典 Observer 是直接耦合 (Subject 持有 Observer 引用), Pub-Sub 通过中间事件通道解耦 (如 EventEmitter), 后者更灵活但更复杂

2. **Python 实现注意事项**: 注意循环引用和内存泄漏——Subject 强引用 Observer 时, 双方析构可能出问题; 可使用弱引用 (`weakref`) 避免

3. **线程安全**: 多线程环境下 attach/detach/notify 需要加锁保护观察者列表, 否则可能触发 `ConcurrentModificationException`

4. **性能考量**: 如果观察者数量很大 (>1000), 通知过程可能成为性能瓶颈, 考虑异步通知或批量处理

5. **框架中的观察者**: Django Signals、Flask Signals (Blinker)、RxPy (ReactiveX) 都是观察者模式的高阶应用

6. **C/C++ 中的实现**: 函数指针回调、信号槽 (Qt)、`std::function` + 事件列表 是最常见的实现方式
