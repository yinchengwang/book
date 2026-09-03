#!/usr/bin/env python3
"""
concurrency.py — Python 并发编程综合演示

本文件综合演示 Python 并发编程三大方向：
1. threading - 多线程（I/O 密集）
2. multiprocessing - 多进程（CPU 密集）
3. asyncio - 协程（高并发 I/O）
"""

import time
import threading
import multiprocessing as mp
import asyncio
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor


# ============================================================================
# 1. threading 多线程
# ============================================================================

def io_task(task_id: int) -> str:
    """模拟 I/O 任务"""
    time.sleep(0.5)
    return f"task-{task_id}"


def demo_threading():
    """多线程演示"""
    print("\n    --- threading 多线程 ---")
    start = time.perf_counter()

    with ThreadPoolExecutor(max_workers=4) as executor:
        futures = [executor.submit(io_task, i) for i in range(4)]
        results = [f.result() for f in futures]

    elapsed = time.perf_counter() - start
    print(f"    4 个 I/O 任务: {elapsed:.2f}s")
    print(f"    结果: {results}")


# ============================================================================
# 2. multiprocessing 多进程
# ============================================================================

def cpu_task(n: int) -> int:
    """模拟 CPU 密集任务"""
    return sum(i * i for i in range(n))


def demo_multiprocessing():
    """多进程演示"""
    print("\n    --- multiprocessing 多进程 ---")
    start = time.perf_counter()

    with ProcessPoolExecutor(max_workers=4) as executor:
        futures = [executor.submit(cpu_task, 1000000) for _ in range(4)]
        results = [f.result() for f in futures]

    elapsed = time.perf_counter() - start
    print(f"    4 个 CPU 任务: {elapsed:.2f}s")
    print(f"    结果: {results[:2]}...")


# ============================================================================
# 3. asyncio 协程
# ============================================================================

async def async_io_task(task_id: int) -> str:
    """异步 I/O 任务"""
    await asyncio.sleep(0.5)
    return f"async-task-{task_id}"


async def demo_asyncio():
    """异步协程演示"""
    print("\n    --- asyncio 协程 ---")
    start = time.perf_counter()

    results = await asyncio.gather(
        *[async_io_task(i) for i in range(4)]
    )

    elapsed = time.perf_counter() - start
    print(f"    4 个异步任务: {elapsed:.2f}s")
    print(f"    结果: {results}")


# ============================================================================
# 4. 对比总结
# ============================================================================

def comparison():
    """并发方式对比"""
    print("\n    --- 并发方式对比 ---")
    print("    | 方式 | 适用场景 | GIL |")
    print("    |------|----------|-----|")
    print("    | threading | I/O 密集 | 限制 |")
    print("    | multiprocessing | CPU 密集 | 无 |")
    print("    | asyncio | 高并发 I/O | 无 |")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 并发编程综合演示")
    print("=" * 60)

    demo_threading()
    demo_multiprocessing()
    asyncio.run(demo_asyncio())
    comparison()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
