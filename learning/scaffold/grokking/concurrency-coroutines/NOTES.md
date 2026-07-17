# Python 协程基础 · 学习笔记

## 核心概念

协程是**用户态轻量级线程**，由程序自身调度而非操作系统。Python 中有两条发展线：

| 阶段 | 机制 | 说明 |
|------|------|------|
| Python 2.x | 生成器 (yield) | 单向数据产生 |
| Python 2.5+ | 增强生成器 (yield/send) | 双向数据传递，协程雏形 |
| Python 3.3+ | yield from | 委托给子生成器 |
| Python 3.5+ | async/await (PEP 492) | 原生协程语法 |
| Python 3.7+ | asyncio.run() | 简化事件循环入口 |

关键区别：生成器协程是"半协程"（只能让出给调用者），async/await 是"全协程"（可以让出给事件循环调度）。

## 工程对照

本仓库 engineering/ 中的轻量级并发模式对比：

1. **手写调度器 vs epoll 事件循环**：调度器用 deque 轮转协程，本质是 round-robin 调度。工程中的 epoll（db_server.c）用就绪 fd 队列分发事件——fd 就绪时触发回调执行。手写调度器没有 I/O 多路复用能力，仅演示调度原理。

2. **协程 vs 线程**：工程代码大量使用 pthread（如 raft.c 的选举超时线程、wal.c 的刷盘线程），线程由 OS 抢占式调度，协程由用户代码显式 yield。协程的切换成本（~200ns）远低于线程（~1-10μs），但协程无法利用多核。

3. **async/await 模式**：对应到 C 代码，等价于回调函数 + 状态机的实现。async 函数被编译器重写为状态机，每次 await 是一个状态转移。这与 C 语言中手写状态机（如协议解析、Raft 状态转换）是同一思想——只是 async/await 由编译器自动生成状态机，C 需要手动维护状态。

4. **goroutine 对照**：Go 的 goroutine 是语言内置的协程 + 调度器（GMP 模型），Python 的协程需要 asyncio 库支持。Go 协程是"抢占式协作"（在函数调用处可被抢占），Python 协程是"纯协作"——不 yield/await 就永远占用 CPU。这差异导致 Python 协程中不能有长时间运算，而 Go 的 goroutine 相对宽松。

## 面试要点

1. 生成器 `__next__()` 等价于 `send(None)`，首次调用必须用 `next()` 或 `send(None)`
2. async 函数不是协程对象，调用后返回 coroutine object，需要 await 或 create_task
3. await 只能在 async 函数中使用，破坏了函数调用的透明性（"函数染色"问题）
4. Python 的协程是协作式的——阻塞代码（如 time.sleep）会阻塞整个事件循环，必须用 asyncio.sleep
5. 协程适合 I/O 密集型，不适合 CPU 密集型
6. asyncio 的 Task 继承自 Future，Future 继承自 Awaitable（PEP 492 引入的抽象基类）
