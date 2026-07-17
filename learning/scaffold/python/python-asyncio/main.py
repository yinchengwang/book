#!/usr/bin/env python3
"""
asyncio.py — Python 异步编程演示

asyncio 是 Python 的协程库，支持高并发 I/O 操作。
核心概念：
1. async/await 语法
2. 协程函数 vs 协程对象
3. asyncio.run() 事件循环
4. 并发任务：create_task, gather, wait
5. 异步上下文管理器
"""

import asyncio
import time
from typing import List


# ============================================================================
# 1. 基础协程
# ============================================================================

async def say_hello(name: str, delay: float = 1.0):
    """异步问候函数"""
    print(f"    [{name}] 开始...")
    await asyncio.sleep(delay)  # 模拟异步 I/O
    print(f"    [{name}] 你好！")
    return f"Hello from {name}"


async def main_basic():
    """基础协程演示"""
    print("\n    --- 基础协程 ---")
    # 直接调用协程函数返回协程对象
    coro = say_hello("Alice")
    print(f"    协程对象: {coro}")
    # 用 asyncio.run() 或 await 执行
    result = await coro
    print(f"    返回值: {result}")


# ============================================================================
# 2. 并发任务
# ============================================================================

async def fetch_data(url: str, delay: float = 1.0):
    """模拟异步 HTTP 请求"""
    print(f"    [获取] {url} 开始...")
    await asyncio.sleep(delay)  # 模拟网络延迟
    print(f"    [获取] {url} 完成")
    return f"数据: {url}"


async def demo_gather():
    """asyncio.gather 并发执行多个协程"""
    print("\n    --- asyncio.gather ---")
    start = time.perf_counter()

    # 并发执行三个请求（总耗时 ~1s 而非 3s）
    results = await asyncio.gather(
        fetch_data("https://api.example.com/users", 1.0),
        fetch_data("https://api.example.com/posts", 0.8),
        fetch_data("https://api.example.com/comments", 1.2),
    )

    elapsed = time.perf_counter() - start
    print(f"    总耗时: {elapsed:.2f}s（并发优势）")
    return results


async def demo_create_task():
    """create_task 创建后台任务"""
    print("\n    --- create_task ---")
    task = asyncio.create_task(say_hello("Bob", 0.5))

    # 任务创建后立即继续执行
    print("    主协程继续执行...")
    await asyncio.sleep(0.3)
    print("    主协程做一些其他事情...")

    # 等待任务完成
    result = await task
    print(f"    任务结果: {result}")


# ============================================================================
# 3. asyncio.wait
# ============================================================================

async def demo_wait():
    """asyncio.wait 等待任务集合"""
    print("\n    --- asyncio.wait ---")

    async def long_task(name: str, secs: float):
        await asyncio.sleep(secs)
        return f"{name} 完成"

    task1 = asyncio.create_task(long_task("任务A", 2.0))
    task2 = asyncio.create_task(long_task("任务B", 1.0))
    task3 = asyncio.create_task(long_task("任务C", 0.5))

    # 等待所有任务完成
    done, pending = await asyncio.wait([task1, task2, task3])

    for task in done:
        print(f"    {task.result()}")


# ============================================================================
# 4. 异步生成器
# ============================================================================

async def async_page_crawler(urls: List[str]):
    """异步生成器：逐个产出结果"""
    for url in urls:
        await asyncio.sleep(0.1)  # 模拟网络请求
        yield f"已抓取: {url}"


async def demo_async_generator():
    """异步生成器演示"""
    print("\n    --- 异步生成器 ---")
    urls = ["page1", "page2", "page3", "page4", "page5"]

    # 异步迭代异步生成器
    async for result in async_page_crawler(urls):
        print(f"    {result}")


# ============================================================================
# 5. 异步上下文管理器
# ============================================================================

class AsyncTimer:
    """异步计时器"""
    def __init__(self, name: str):
        self.name = name
        self.start = None

    async def __aenter__(self):
        self.start = time.perf_counter()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        elapsed = time.perf_counter() - self.start
        print(f"    [{self.name}] 耗时: {elapsed:.4f}s")


async def demo_async_cm():
    """异步上下文管理器"""
    print("\n    --- 异步上下文管理器 ---")
    async with AsyncTimer("异步操作"):
        await asyncio.sleep(0.5)


# ============================================================================
# 6. 信号量控制并发数
# ============================================================================

async def bounded_fetch(semaphore: asyncio.Semaphore, url: str):
    """带信号量限制的并发请求"""
    async with semaphore:
        await asyncio.sleep(0.2)
        return f"完成: {url}"


async def demo_semaphore():
    """信号量控制最大并发数"""
    print("\n    --- 信号量控制并发 ---")
    sem = asyncio.Semaphore(2)  # 最多同时 2 个

    urls = ["a", "b", "c", "d", "e"]
    tasks = [bounded_fetch(sem, url) for url in urls]
    results = await asyncio.gather(*tasks)
    for r in results:
        print(f"    {r}")


# ============================================================================
# 7. 超时处理
# ============================================================================

async def demo_timeout():
    """asyncio.timeout 超时处理"""
    print("\n    --- 超时处理 ---")

    async def slow_task():
        await asyncio.sleep(10)
        return "完成"

    try:
        async with asyncio.timeout(1.0):
            result = await slow_task()
            print(f"    结果: {result}")
    except asyncio.TimeoutError:
        print("    超时！任务被取消")


# ============================================================================
# 主函数
# ============================================================================

async def main():
    print("=" * 60)
    print("Python asyncio 异步编程演示")
    print("=" * 60)

    await main_basic()
    await demo_gather()
    await demo_create_task()
    await demo_wait()
    await demo_async_generator()
    await demo_async_cm()
    await demo_semaphore()
    await demo_timeout()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    asyncio.run(main())
