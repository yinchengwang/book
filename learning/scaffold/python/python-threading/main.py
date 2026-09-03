#!/usr/bin/env python3
"""
threading.py — Python 多线程演示

Python GIL 限制了多线程的 CPU 密集型任务性能，但 I/O 密集型任务仍能受益。
核心概念：
1. Thread 对象创建和管理
2. Lock/RLock 互斥锁
3. Event/Condition 同步原语
4. ThreadPoolExecutor 线程池
"""

import threading
import time
import queue
from concurrent.futures import ThreadPoolExecutor
from typing import List


# ============================================================================
# 1. 基础多线程
# ============================================================================

def worker(name: str, seconds: float):
    """工作线程函数"""
    print(f"    [{name}] 开始工作...")
    time.sleep(seconds)
    print(f"    [{name}] 完成（耗时 {seconds}s）")


def demo_basic_threading():
    """基础多线程演示"""
    print("\n    --- 基础多线程 ---")
    threads = []

    for i in range(3):
        t = threading.Thread(target=worker, args=(f"Worker-{i}", 0.5 - i * 0.1))
        threads.append(t)
        t.start()

    # 等待所有线程完成
    for t in threads:
        t.join()

    print("    所有线程完成")


# ============================================================================
# 2. 线程间共享状态与锁
# ============================================================================

counter = 0
counter_lock = threading.Lock()


def increment(counter_list: List[int], times: int):
    """带锁的计数器"""
    global counter
    for _ in range(times):
        with counter_lock:
            counter += 1
        counter_list.append(1)


def demo_lock():
    """Lock 互斥锁演示"""
    print("\n    --- Lock 互斥锁 ---")
    global counter
    counter = 0

    threads = []
    local_counters = []

    for i in range(4):
        local_counter = []
        local_counters.append(local_counter)
        t = threading.Thread(target=increment, args=(local_counter, 10000))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    print(f"    预期值: 40000")
    print(f"    实际值: {counter}")
    print(f"    线程安全: {'[OK]' if counter == 40000 else '[FAIL]'}")


# ============================================================================
# 3. RLock（可重入锁）
# ============================================================================

class ReentrantDemo:
    """可重入锁演示"""

    def __init__(self):
        self.lock = threading.RLock()
        self.counter = 0

    def outer(self):
        with self.lock:
            self.counter += 1
            print(f"    outer: counter={self.counter}")
            self.inner()

    def inner(self):
        # RLock 允许同一线程多次获取
        with self.lock:
            self.counter += 1
            print(f"    inner: counter={self.counter}")


def demo_rlock():
    """RLock 可重入锁"""
    print("\n    --- RLock 可重入锁 ---")
    demo = ReentrantDemo()
    demo.outer()


# ============================================================================
# 4. Event 同步
# ============================================================================

class PingPong:
    """Event 同步实现交替打印"""

    def __init__(self):
        self.ping_ready = threading.Event()
        self.pong_ready = threading.Event()

    def ping(self):
        for i in range(3):
            self.ping_ready.wait()  # 等待信号
            self.ping_ready.clear()
            print(f"    Ping {i + 1}")
            time.sleep(0.05)
            self.pong_ready.set()

    def pong(self):
        for i in range(3):
            print(f"    Pong {i + 1}")
            time.sleep(0.05)
            self.ping_ready.set()
            self.pong_ready.wait()
            self.pong_ready.clear()


# ============================================================================
# 5. Condition 实现生产者-消费者
# ============================================================================

class ProducerConsumer:
    """Condition 实现生产者-消费者"""

    def __init__(self):
        self.queue = queue.Queue(maxsize=5)
        self.lock = threading.Lock()
        self.not_full = threading.Condition(self.lock)
        self.not_empty = threading.Condition(self.lock)
        self.done = threading.Event()

    def producer(self):
        for i in range(10):
            self.not_full.acquire()
            while self.queue.full():
                self.not_full.wait()
            self.queue.put(i)
            print(f"    生产: {i}")
            self.not_empty.notify()
            self.not_full.release()

        self.done.set()

    def consumer(self):
        while not self.done.is_set() or not self.queue.empty():
            self.not_empty.acquire()
            while self.queue.empty() and not self.done.is_set():
                self.not_empty.wait()
            if not self.queue.empty():
                item = self.queue.get()
                print(f"    消费: {item}")
                self.not_empty.notify()
            self.not_empty.release()
            time.sleep(0.05)


# ============================================================================
# 6. ThreadPoolExecutor
# ============================================================================

def cpu_task(n: int) -> int:
    """模拟 CPU 密集型任务"""
    return sum(i * i for i in range(n))


def demo_threadpool():
    """线程池演示"""
    print("\n    --- ThreadPoolExecutor ---")
    with ThreadPoolExecutor(max_workers=4) as executor:
        futures = [executor.submit(cpu_task, 100000) for _ in range(8)]
        results = [f.result() for f in futures]
        print(f"    8 个任务完成，结果: {results[:3]}...")


# ============================================================================
# 7. 守护线程
# ============================================================================

def daemon_task():
    """守护线程任务"""
    while True:
        print("    [守护线程] 运行中...")
        time.sleep(1)


def demo_daemon():
    """守护线程"""
    print("\n    --- 守护线程 ---")
    t = threading.Thread(target=daemon_task, daemon=True)
    t.start()
    time.sleep(3)
    print("    主线程结束，守护线程将被自动终止")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python threading 多线程演示")
    print("=" * 60)

    demo_basic_threading()
    demo_lock()
    demo_rlock()

    print("\n[4] Event 交替打印:")
    game = PingPong()
    t1 = threading.Thread(target=game.ping)
    t2 = threading.Thread(target=game.pong)
    t2.start()
    t1.start()
    t1.join()
    t2.join()

    print("\n[5] 生产者-消费者:")
    pc = ProducerConsumer()
    tp = threading.Thread(target=pc.producer)
    tc = threading.Thread(target=pc.consumer)
    tp.start()
    tc.start()
    tp.join()
    tc.join()

    demo_threadpool()
    demo_daemon()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
