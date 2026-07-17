"""
concurrency-python-threading — Python 多线程基础

演示 threading.Thread / GIL 限制 / I/O 与 CPU 对比 /
       threading.Lock / threading.local
"""

import threading
import time
from typing import List

# ============================================================
# 1. threading.Thread — 基本线程创建与 join
# ============================================================
def worker(name: str, delay: float):
    print(f"  [{name}] 开始, thread_id={threading.get_ident()}")
    time.sleep(delay)
    print(f"  [{name}] 结束")


def demo_basic_thread():
    print("=== 1. 基本线程 ===")
    threads: List[threading.Thread] = []
    for i in range(3):
        t = threading.Thread(target=worker, args=(f"Worker-{i}", 0.2))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()
    print("  全部 join 完成\n")


# ============================================================
# 2. GIL 限制 — CPU 密集型任务在 threading 下不并行
# ============================================================
def cpu_intensive(n: int) -> int:
    """纯粹的 CPU 计算，GIL 阻止并行"""
    total = 0
    for i in range(n):
        total += i * i
    return total


def demo_gil_cpu_bound():
    print("=== 2. GIL 限制: CPU 密集型 (串行化) ===")
    n = 20_000_000

    start = time.perf_counter()
    results: List[int] = [0] * 4
    ts: List[threading.Thread] = []

    for i in range(4):
        t = threading.Thread(target=lambda idx=i: results.__setitem__(idx, cpu_intensive(n)))
        ts.append(t)
        t.start()

    for t in ts:
        t.join()
    elapsed = time.perf_counter() - start
    print(f"  4 线程 CPU 任务耗时: {elapsed:.3f}s (接近串行)\n")


# ============================================================
# 3. I/O 绑定的线程确实受益
# ============================================================
def io_simulated(name: str, count: int):
    for i in range(count):
        time.sleep(0.05)  # 模拟 I/O 等待 (sleep 释放 GIL)


def demo_io_benefit():
    print("=== 3. I/O 密集型: 线程确实受益 ===")
    count = 10

    # 串行执行
    start = time.perf_counter()
    for i in range(4):
        io_simulated(f"serial-{i}", count)
    serial_time = time.perf_counter() - start
    print(f"  串行 I/O 耗时: {serial_time:.3f}s")

    # 多线程并发
    start = time.perf_counter()
    ts: List[threading.Thread] = []
    for i in range(4):
        t = threading.Thread(target=io_simulated, args=(f"thread-{i}", count))
        ts.append(t)
        t.start()
    for t in ts:
        t.join()
    parallel_time = time.perf_counter() - start
    print(f"  多线程 I/O 耗时: {parallel_time:.3f}s (接近 1/4)\n")


# ============================================================
# 4. threading.Lock — 线程安全
# ============================================================
counter = 0
counter_lock = threading.Lock()


def unsafe_increment(n: int):
    global counter
    for _ in range(n):
        # 没有锁：数据竞争
        tmp = counter
        counter = tmp + 1


def safe_increment(n: int):
    global counter
    for _ in range(n):
        with counter_lock:
            tmp = counter
            counter = tmp + 1


def demo_lock():
    print("=== 4. Lock 线程安全 ===")
    n = 100_000

    # 无锁：结果错误
    global counter
    counter = 0
    ts = [threading.Thread(target=unsafe_increment, args=(n,)) for _ in range(4)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    print(f"  无锁 counter = {counter} (期望 {4 * n})")

    # 有锁：结果正确
    counter = 0
    ts = [threading.Thread(target=safe_increment, args=(n,)) for _ in range(4)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    print(f"  有锁 counter = {counter} (期望 {4 * n})\n")


# ============================================================
# 5. threading.local() — 线程局部存储
# ============================================================
thread_data = threading.local()


def show_local(name: str):
    """每个线程的 local 独立"""
    if not hasattr(thread_data, "value"):
        thread_data.value = 0
    thread_data.value += 1
    print(f"  [{name}] local.value = {thread_data.value}")


def demo_thread_local():
    print("=== 5. threading.local ===")
    ts = [threading.Thread(target=show_local, args=(f"T-{i}",)) for i in range(3)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    # 主线程的值不受影响
    print(f"  [main] local.value = {getattr(thread_data, 'value', '未设置')}\n")


# ============================================================
# 主函数
# ============================================================
if __name__ == "__main__":
    print("Python 多线程基础演示")
    print("====================\n")

    demo_basic_thread()
    demo_gil_cpu_bound()
    demo_io_benefit()
    demo_lock()
    demo_thread_local()

    print("全部演示完成.")
