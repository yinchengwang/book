# Python asyncio 异步编程 · 学习笔记

## 核心概念

asyncio 是 Python 的**单线程协作式并发**框架。核心机制：

1. **event loop** — 单线程循环，调度协程执行
2. **coroutine** — async def 定义的协程函数，await 挂起并让出控制权
3. **awaitable** — 可在 await 表达式中使用的对象（coroutine / Task / Future）
4. **Task** — 封装协程为调度单元，create_task 提交到 event loop

关键区别：协程只有被 await 或 create_task 才会执行；单纯的 `coro()` 调用只创建协程对象。

## 工程对照

本仓库 engineering/src/ 中的异步 I/O 模式对比：

1. **db_server.c** — 使用 epoll 实现事件驱动，主循环等待就绪 fd，分派到处理函数。这正是 asyncio 底层的 select/epoll 机制封装——asyncio 对应用层隐藏了这些细节。db_server 的 select loop 相当于手写 event loop，asyncio 是标准化的高级抽象。

2. **WAL 日志刷新** — wal.c 使用条件变量 + 后台线程实现异步刷盘。如果移植到 Python，asyncio 的 run_in_executor 可以在不阻塞 event loop 的情况下将同步 I/O 委托给线程池处理。

3. **Raft 心跳** — raft.c 使用定时器 + 后台线程发送心跳。asyncio 的 loop.call_later 或 asyncio.sleep 循环可以更简洁地实现周期性心跳，且没有线程安全问题。

**生产建议**：Python asyncio 适合 I/O 密集的服务器（Web API / 数据库驱动 / 微服务），但要注意：a) 计算密集型任务必须委托给线程池/进程池；b) 整个调用链必须 async，与同步库互调需要小心；c) 调试比同步代码更困难（调用栈在 await 处断开）。

## 面试要点

1. asyncio.run() 在 Python 3.7+ 是标准入口，之前用 loop.run_until_complete()
2. 协程是"冷"的——必须 await 或 create_task 才会执行
3. gather 的 return_exceptions=True 参数可以捕获异常而非立即抛出
4. asyncio 不是"并行"而是"并发"——单线程交替执行
5. 第三方库如 aiohttp、asyncpg、aiofiles 提供 asyncio 原生 I/O 支持
6. Python 3.11+ 的 asyncio.TaskGroup 是 gather 的替代方案（结构化并发）
