# Python 多线程基础 · 学习笔记

## 核心概念

CPython 的 GIL（全局解释器锁）是 Python 多线程最核心的约束：任何时候只有一个线程执行 Python 字节码。这意味着：

- **CPU 密集型**：多线程无帮助，甚至因上下文切换更慢 → 改用 multiprocessing
- **I/O 密集型**：sleep/网络/磁盘操作会释放 GIL → 多线程有效

## 工程对照

Python 并发在工程中主要有三个层次：

1. **threading** — 轻量级 I/O 并发，适合网络请求、文件读写等。缺点是受 GIL 限制，CPU 密集用不了。本仓库的各类 Python SDK 和测试脚本中，threading 常用于并发测试（同时发送多个请求验证数据库并发能力）。

2. **multiprocessing** — 每个进程独立 GIL，适合 CPU 密集型。但进程间通信成本高，不适合大量短任务。对应到本仓库 C 代码中的多进程架构（如 db_server fork 模式），multiprocessing 提供更高级的抽象。

3. **asyncio** — 单线程协作式并发，适合大量 I/O 等待场景。相比 threading，asyncio 没有线程切换成本也没有竞态条件（单线程）。但需要整个调用链都 async，存量同步代码改造成本高。

**实践建议**：I/O 并发首选 asyncio，CPU 并行用 multiprocessing，只有需要混用阻塞 I/O 和轻量并发时用 threading。Python 3.13 的 experimental free-threading 模式（no-GIL）如果进入稳定版，将改变这个格局。

## 面试要点

1. GIL 的存在使得 CPython threading 不适合 CPU 密集型任务
2. threading.Lock 是重入锁（RLock），同一个线程可多次 acquire
3. threading.local() 内部实现是一个字典，按线程 id 索引
4. 多线程共享全局变量需要锁保护，但 mutable 对象的 in-place 操作（如 list.append）因 GIL 在字节码层面是安全的（CPython 实现细节，不建议依赖）
5. Python 的 threading 底层是 OS 原生线程（pthread/Windows Thread），不是 green thread
