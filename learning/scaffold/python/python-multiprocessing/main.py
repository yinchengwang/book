#!/usr/bin/env python3
"""
multiprocessing.py — Python 多进程演示

multiprocessing 绕过 GIL，实现真正的并行计算。
核心概念：
1. Process 创建进程
2. Pool 进程池
3. Queue/Pipe 进程间通信
4. 共享内存与 Manager
5. 进程锁与同步
"""

import multiprocessing as mp
import time
from concurrent.futures import ProcessPoolExecutor
from typing import List


# ============================================================================
# 1. 基础多进程
# ============================================================================

def cpu_bound_task(n: int) -> int:
    """CPU 密集型任务"""
    return sum(i * i for i in range(n))


def demo_basic_process():
    """基础多进程"""
    print("\n    --- 基础多进程 ---")
    processes = []
    start = time.perf_counter()

    for i in range(4):
        p = mp.Process(target=cpu_bound_task, args=(1000000,))
        processes.append(p)
        p.start()

    for p in processes:
        p.join()

    elapsed = time.perf_counter() - start
    print(f"    4 个进程并行完成，耗时: {elapsed:.2f}s")


# ============================================================================
# 2. 进程池 Pool
# ============================================================================

def heavy_compute(n: int) -> int:
    """模拟重计算"""
    time.sleep(0.1)
    return n * n


def demo_pool():
    """进程池演示"""
    print("\n    --- ProcessPoolExecutor ---")
    start = time.perf_counter()

    with ProcessPoolExecutor(max_workers=4) as executor:
        futures = [executor.submit(heavy_compute, i) for i in range(20)]
        results = [f.result() for f in futures]

    elapsed = time.perf_counter() - start
    print(f"    20 个任务完成，耗时: {elapsed:.2f}s")
    print(f"    结果前5个: {results[:5]}")


# ============================================================================
# 3. Pool.map/starmap
# ============================================================================

def power(base: int, exp: int) -> int:
    """幂运算"""
    return base ** exp


def demo_pool_map():
    """Pool.map 批量映射"""
    print("\n    --- Pool.map/starmap ---")
    with mp.Pool(4) as pool:
        # map：单参数函数
        results = pool.map(power, [1, 2, 3, 4, 5], chunksize=2)
        print(f"    map 结果: {results}")

        # starmap：多参数函数
        results = pool.starmap(power, [(2, 1), (2, 2), (2, 3), (3, 2)])
        print(f"    starmap 结果: {results}")


# ============================================================================
# 4. 进程间通信 Queue
# ============================================================================

def producer(queue: mp.Queue, items: List[int]):
    """生产者"""
    for item in items:
        queue.put(item)
        print(f"    [生产] {item}")
    queue.put(None)  # 发送结束信号


def consumer(queue: mp.Queue):
    """消费者"""
    total = 0
    while True:
        item = queue.get()
        if item is None:
            break
        total += item
        print(f"    [消费] {item}, 总计: {total}")
    return total


def demo_queue():
    """Queue 进程间通信"""
    print("\n    --- Queue 通信 ---")
    queue = mp.Queue()

    p_producer = mp.Process(target=producer, args=(queue, [1, 2, 3, 4, 5]))
    p_consumer = mp.Process(target=consumer, args=(queue,))

    p_producer.start()
    p_consumer.start()
    p_consumer.join()

    print(f"    消费总计: {queue.get() if not queue.empty() else '已返回'}")


# ============================================================================
# 5. Pipe（双工通信）
# ============================================================================

def worker_pipe(conn):
    """管道工作进程"""
    while True:
        msg = conn.recv()
        if msg == "quit":
            break
        print(f"    [工作进程] 收到: {msg}")
        conn.send(f"响应: {msg.upper()}")


def demo_pipe():
    """Pipe 双工通信"""
    print("\n    --- Pipe 双工通信 ---")
    parent_conn, child_conn = mp.Pipe()

    p = mp.Process(target=worker_pipe, args=(child_conn,))
    p.start()

    messages = ["hello", "world", "quit"]
    for msg in messages:
        parent_conn.send(msg)
        response = parent_conn.recv()
        print(f"    [主进程] 收到响应: {response}")

    p.join()


# ============================================================================
# 6. 共享内存
# ============================================================================

def increment_shared(shared_array, lock, times: int):
    """使用共享内存"""
    for _ in range(times):
        with lock:
            shared_array[0] += 1


def demo_shared_memory():
    """共享内存"""
    print("\n    --- 共享内存 ---")
    # 创建共享数组
    shared_value = mp.Value('i', 0)  # 'i' = signed int
    lock = mp.Lock()

    processes = []
    for _ in range(4):
        p = mp.Process(target=increment_shared, args=(shared_value, lock, 10000))
        processes.append(p)
        p.start()

    for p in processes:
        p.join()

    print(f"    预期值: 40000")
    print(f"    实际值: {shared_value.value}")
    print(f"    线程安全: {'[OK]' if shared_value.value == 40000 else '[FAIL]'}")


# ============================================================================
# 7. Manager（共享数据结构）
# ============================================================================

def worker_manager(shared_dict, shared_list):
    """使用 Manager"""
    shared_dict["worker"] = "active"
    shared_list.append("item_from_worker")


def demo_manager():
    """Manager 共享数据结构"""
    print("\n    --- Manager ---")
    with mp.Manager() as manager:
        shared_dict = manager.dict()
        shared_list = manager.list()

        p = mp.Process(target=worker_manager, args=(shared_dict, shared_list))
        p.start()
        p.join()

        print(f"    shared_dict: {dict(shared_dict)}")
        print(f"    shared_list: {list(shared_list)}")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python multiprocessing 多进程演示")
    print("=" * 60)

    demo_basic_process()
    demo_pool()
    demo_pool_map()
    demo_queue()
    demo_pipe()
    demo_shared_memory()
    demo_manager()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
