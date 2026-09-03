"""
concurrency-asyncio — Python asyncio 异步编程基础

演示 async/await / create_task / gather /
      asyncio.Queue / 同步 vs 异步耗时对比
"""

import asyncio
import time
from typing import List


# ============================================================
# 1. async/await 基本语法
# ============================================================
async def greet(name: str, delay: float) -> str:
    """async 函数返回协程对象"""
    await asyncio.sleep(delay)  # 非阻塞等待
    return f"Hello, {name}!"


async def demo_async_await():
    print("=== 1. async/await 基本语法 ===")
    result = await greet("World", 0.2)
    print(f"  {result}\n")


# ============================================================
# 2. asyncio.create_task — 并发执行
# ============================================================
async def count(label: str, n: int):
    for i in range(1, n + 1):
        await asyncio.sleep(0.1)
        print(f"  [{label}] count {i}")
    return f"{label} done"


async def demo_create_task():
    print("=== 2. create_task 并发 ===")
    t1 = asyncio.create_task(count("A", 3))
    t2 = asyncio.create_task(count("B", 3))
    t3 = asyncio.create_task(count("C", 3))

    # 三个任务交错执行（非阻塞 sleep 让出控制权）
    results = await asyncio.gather(t1, t2, t3)
    for r in results:
        print(f"  -> {r}")
    print()


# ============================================================
# 3. asyncio.gather — 等待多个任务
# ============================================================
async def fetch_url(name: str, delay: float) -> dict:
    """模拟异步 HTTP 请求"""
    await asyncio.sleep(delay)
    return {"url": name, "status": 200, "data": f"content_from_{name}"}


async def demo_gather():
    print("=== 3. gather 并发等待 ===")
    urls = ["google.com", "github.com", "python.org"]
    delays = [0.3, 0.5, 0.2]

    tasks = [fetch_url(url, delay) for url, delay in zip(urls, delays)]

    start = time.perf_counter()
    results = await asyncio.gather(*tasks)
    elapsed = time.perf_counter() - start

    for r in results:
        print(f"  {r['url']}: status={r['status']}")
    print(f"  总耗时: {elapsed:.3f}s (≈ max delay)\n")


# ============================================================
# 4. asyncio.Queue — 生产者消费者
# ============================================================
async def producer(queue: asyncio.Queue, n: int):
    for i in range(n):
        item = f"msg-{i}"
        await queue.put(item)
        print(f"  [生产者] 放入: {item}")
        await asyncio.sleep(0.05)
    # 发送结束标记
    await queue.put(None)


async def consumer(name: str, queue: asyncio.Queue):
    while True:
        item = await queue.get()
        if item is None:
            queue.task_done()
            # 把结束标记传给下一个消费者
            await queue.put(None)
            print(f"  [消费者 {name}] 收到结束信号")
            break
        print(f"  [消费者 {name}] 处理: {item}")
        await asyncio.sleep(0.1)
        queue.task_done()


async def demo_queue():
    print("=== 4. Queue 生产者消费者 ===")
    queue: asyncio.Queue = asyncio.Queue()

    prod = asyncio.create_task(producer(queue, 5))
    cons1 = asyncio.create_task(consumer("A", queue))
    cons2 = asyncio.create_task(consumer("B", queue))

    await asyncio.gather(prod, cons1, cons2)
    print()


# ============================================================
# 5. 同步 vs 异步耗时对比
# ============================================================
def sync_fetch(urls: List[str]) -> float:
    start = time.perf_counter()
    for url in urls:
        time.sleep(0.3)  # 阻塞等待
        print(f"  [同步] 完成: {url}")
    return time.perf_counter() - start


async def async_fetch(urls: List[str]) -> float:
    start = time.perf_counter()
    async def fetch_one(url: str):
        await asyncio.sleep(0.3)
        print(f"  [异步] 完成: {url}")

    tasks = [fetch_one(url) for url in urls]
    await asyncio.gather(*tasks)
    return time.perf_counter() - start


async def demo_compare():
    print("=== 5. 同步 vs 异步耗时对比 ===")
    urls = [f"site-{i}.com" for i in range(5)]

    sync_time = sync_fetch(urls)
    async_time = await async_fetch(urls)

    print(f"\n  同步 5 个请求: {sync_time:.3f}s")
    print(f"  异步 5 个请求: {async_time:.3f}s")
    print(f"  加速比: {sync_time / async_time:.1f}x\n")


# ============================================================
# 主函数
# ============================================================
async def main():
    print("Python asyncio 异步编程演示")
    print("==========================\n")

    await demo_async_await()
    await demo_create_task()
    await demo_gather()
    await demo_queue()
    await demo_compare()

    print("全部演示完成.")


if __name__ == "__main__":
    asyncio.run(main())
