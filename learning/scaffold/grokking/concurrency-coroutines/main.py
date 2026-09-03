"""
concurrency-coroutines — Python 协程基础

演示 generator-based coroutines / 手写调度器 /
      async/await 原生协程 / 协作式 vs 抢占式对比
"""

import time
from collections import deque
from typing import Any, Deque, Generator, List, Optional


# ============================================================
# 1. Generator-based coroutines (yield/send)
# ============================================================
def simple_coroutine():
    """生成器协程——通过 send 接收数据"""
    print("  协程启动")
    x = yield  # 第一次 next() 后停在这里
    print(f"  收到: {x}")
    y = yield x * 2
    print(f"  收到: {y}")
    yield y * 2


def demo_generator_coroutine():
    print("=== 1. 生成器协程 (yield/send) ===")
    coro = simple_coroutine()

    # 启动协程到第一个 yield
    result = next(coro)
    print(f"  第一次 yield 返回: {result}")

    # send 发送值并恢复执行到下一个 yield
    result = coro.send(10)
    print(f"  第二次 yield 返回: {result}")

    result = coro.send(20)
    print(f"  第三次 yield 返回: {result}")
    print()


# ============================================================
# 2. 简单协程调度器/事件循环
# ============================================================
class SimpleScheduler:
    """手写协作式调度器"""

    def __init__(self):
        self._ready: Deque[Generator] = deque()
        self._task_map: dict = {}

    def spawn(self, coro: Generator) -> int:
        tid = id(coro)
        self._ready.append(coro)
        self._task_map[tid] = coro
        return tid

    def run(self):
        while self._ready:
            coro = self._ready.popleft()
            try:
                next(coro)  # 执行到下一个 yield
                self._ready.append(coro)  # 重新加入队列
            except StopIteration:
                pass  # 协程结束


def task_a():
    for i in range(3):
        print(f"    [A] step {i}")
        yield  # 让出控制权


def task_b():
    for i in range(3):
        print(f"    [B] step {i}")
        yield


def demo_scheduler():
    print("=== 2. 手写调度器 (协作式) ===")
    sched = SimpleScheduler()
    sched.spawn(task_a())
    sched.spawn(task_b())
    sched.run()
    print()


# ============================================================
# 3. 手动上下文切换
# ============================================================
def manual_switch():
    print("=== 3. 手动上下文切换 ===")
    # 手动交替执行两个生成器
    def gen1():
        yield "gen1-step1"
        yield "gen1-step2"
        yield "gen1-step3"

    def gen2():
        yield "gen2-step1"
        yield "gen2-step2"

    g1 = gen1()
    g2 = gen2()

    # 协程调度：交替从两个生成器取值
    tasks = [g1, g2]
    while tasks:
        for g in tasks[:]:
            try:
                val = next(g)
                print(f"  切换 -> {val}")
            except StopIteration:
                tasks.remove(g)
    print()


# ============================================================
# 4. async/await 原生协程 (Python 3.5+)
# ============================================================
async def async_task(name: str, steps: int):
    """原生协程函数"""
    import asyncio
    for i in range(steps):
        print(f"    [{name}] step {i}")
        await asyncio.sleep(0.05)  # 非阻塞等待
    return f"{name} done"


async def demo_async_await_coroutines():
    print("=== 4. async/await 原生协程 ===")
    import asyncio

    t1 = asyncio.create_task(async_task("X", 3))
    t2 = asyncio.create_task(async_task("Y", 3))

    results = await asyncio.gather(t1, t2)
    for r in results:
        print(f"  -> {r}")
    print()


# ============================================================
# 5. 协作式 vs 抢占式对比
# ============================================================
def demo_coop_vs_preemptive():
    print("=== 5. 协作式 vs 抢占式对比 ===")

    # --- 协作式 (手写调度器) ---
    print("  协作式: 协程显式 yield 让出 CPU")
    start = time.perf_counter()

    sched = SimpleScheduler()

    def coop_task(n: int, label: str):
        for i in range(n):
            # 模拟计算
            _ = [x * x for x in range(1000)]
            yield  # 主动让出

    sched.spawn(coop_task(5, "T1"))
    sched.spawn(coop_task(5, "T2"))
    sched.run()

    coop_time = time.perf_counter() - start
    print(f"  协作式总耗时: {coop_time:.4f}s")

    # --- 抢占式 (OS 线程) ---
    print("\n  抢占式: 操作系统自动切换线程")
    start = time.perf_counter()

    import threading
    done = [False, False]

    def preemp_task(n: int, idx: int):
        for _ in range(n):
            _ = [x * x for x in range(1000)]
        done[idx] = True

    threads = [
        threading.Thread(target=preemp_task, args=(5, 0)),
        threading.Thread(target=preemp_task, args=(5, 1)),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    preemp_time = time.perf_counter() - start
    print(f"  抢占式总耗时: {preemp_time:.4f}s")

    print(f"\n  对比: 协作式 {coop_time:.4f}s vs 抢占式 {preemp_time:.4f}s")
    print("  (两者完成相同工作量，区别在调度方式)\n")


# ============================================================
# 主函数
# ============================================================
def main():
    import asyncio

    print("Python 协程基础演示")
    print("==================\n")

    demo_generator_coroutine()
    demo_scheduler()
    manual_switch()
    asyncio.run(demo_async_await_coroutines())
    demo_coop_vs_preemptive()

    print("全部演示完成.")


if __name__ == "__main__":
    main()
