# C++ 多线程基础 · 学习笔记

## 核心概念

| 机制 | 用途 | 适用场景 |
|------|------|----------|
| std::thread | 创建操作系统线程 | 独立并发任务 |
| std::promise / future | 单向值传递 | 线程返回结果或异常 |
| std::packaged_task | 包装函数为异步单元 | 任务队列 / 线程池 |
| std::async | 高级异步启动 | 简单并发调用 |

## 工程对照

本仓库 engineering/src/ 下有大量线程使用场景，对照如下：

1. **Storage Engine Buffer Pool** (`bufmgr.c`)：使用互斥锁保护共享哈希表和 Clock-Sweep 扫描，相当于 std::thread + std::mutex 的典型组合。如果改用 std::future 模式，脏页刷盘可以变为异步任务，减少前台延迟。

2. **WAL 日志** (`wal.c`)：后台刷盘线程使用条件变量等待 buffer 写满信号。这正是 std::thread + std::condition_variable 的标准模式。C++17 的 std::future 可以简化这一流程——将刷盘任务 packaged_task 提交给后台线程池，避免手动管理线程生命周期。

3. **Raft 共识模块** (`raft.c`)：Leader 选举需要超时等待和心跳发送，每个节点有独立的线程循环。如果使用 std::async 启动选举超时检测，代码会比原始 pthread 更简洁、异常安全。

4. **Initdb 工具** (`initdb.c`)：多表创建可以并行化——使用 std::async 并发初始化多个系统表，再用 future::get 汇总结果，相比顺序执行减少初始化时间。

**关键差异**：工程代码使用 pthread 或 C 风格线程 API（因为代码库以 C 为主），C++17 的线程库提供了 RAII 包装（std::thread 析构自动 detach 或 join）、异常安全的 future 传值、以及更高层的 async 抽象。生产环境如直接使用 C++17，应优先用 std::async 而非裸 std::thread，以减少手动线程管理带来的生命周期 bug。

## 面试要点

1. std::thread 必须 join 或 detach，析构时若仍 joinable 会触发 std::terminate
2. future::get() 只能调用一次，第二次调用抛异常
3. std::async 默认策略（std::launch::deferred | std::launch::async）由实现决定，不确定是否真正并行
4. packaged_task 可移动但不可复制，适合放入容器（如任务队列）
