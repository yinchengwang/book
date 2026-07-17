# python-asyncio 学习笔记

## 概念地图

asyncio 是 Python 的协程/异步 I/O 库：

- **协程**：async def 定义的函数，可在 I/O 时挂起
- **事件循环**：asyncio.run() 创建事件循环驱动协程
- **await**：等待协程结果，触发上下文切换
- **并发 vs 并行**：协程是单线程并发，不是多核并行

## asyncio vs threading

| 特性 | asyncio | threading |
|------|---------|-----------|
| 线程 | 单线程 | 多线程 |
| 切换 | 协作式（await） | 抢占式 |
| 适用 | I/O 密集 | CPU 密集 |
| 复杂度 | 较高 | 锁/死锁风险 |

## 踩坑记录

1. **阻塞调用**：不要在协程中用 time.sleep()，用 await asyncio.sleep()
2. **同步代码**：同步函数不能直接 await，需用 run_in_executor
3. **异常传播**：gather 中一个协程异常会导致整体失败

## 工程对照（≥100 字硬约束）

asyncio 在现代 Python 网络编程中广泛应用：

1. **aiohttp/aiofiles**：异步 HTTP 客户端/文件操作
2. **FastAPI**：基于 asyncio 的高性能 Web 框架
3. **异步数据库**：aiomysql, asyncpg 等
4. **并发控制**：Semaphore 限制并发数
5. **超时处理**：asyncio.timeout 安全处理超时
6. **爬虫**：aiohttp + asyncio 实现高并发抓取

学完本卡能动手的事：用 aiohttp 重构同步 HTTP 请求，实现并发抓取。
