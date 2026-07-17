#!/usr/bin/env python3
"""
memory_leak.py — Python 内存泄漏演示

演示常见内存泄漏场景及检测方法。
"""

import gc
import sys
import tracemalloc
from typing import List


# ============================================================================
# 1. 全局列表泄漏
# ============================================================================

leaked_list: List = []  # 全局列表不断增长


def leak_via_global():
    """通过全局变量泄漏"""
    global leaked_list
    data = {'large': 'x' * 10000}
    leaked_list.append(data)


def demo_global_leak():
    """全局变量泄漏"""
    print("\n    --- 全局变量泄漏 ---")
    initial = len(leaked_list)

    for i in range(100):
        leak_via_global()

    final = len(leaked_list)
    print(f"    列表增长: {initial} -> {final} (+{final - initial})")


# ============================================================================
# 2. 闭包引用泄漏
# ============================================================================

def create_leaking_closure():
    """创建泄漏的闭包"""
    large_data = 'x' * 100000  # 闭包持有大对象

    def closure():
        return len(large_data)  # 闭包引用 large_data

    return closure


def demo_closure_leak():
    """闭包泄漏"""
    print("\n    --- 闭包泄漏 ---")

    closures = []
    for i in range(10):
        closures.append(create_leaking_closure())

    print(f"    创建了 {len(closures)} 个闭包")
    print(f"    每个闭包持有 ~100KB 数据")


# ============================================================================
# 3. 缓存未清理
# ============================================================================

cache = {}


def expensive_computation(key):
    """模拟昂贵计算"""
    if key in cache:
        return cache[key]
    result = key * key
    cache[key] = result  # 缓存永远增长
    return result


def demo_cache_leak():
    """缓存泄漏"""
    print("\n    --- 缓存泄漏 ---")
    initial_size = len(cache)

    for i in range(1000):
        expensive_computation(i)

    final_size = len(cache)
    print(f"    缓存增长: {initial_size} -> {final_size}")


# ============================================================================
# 4. 检测内存泄漏
# ============================================================================

def find_leaks():
    """使用 tracemalloc 找泄漏"""
    print("\n    --- tracemalloc 检测 ---")

    tracemalloc.start()

    # 模拟操作
    for i in range(100):
        data = {'id': i, 'payload': 'x' * 1000}

    snapshot = tracemalloc.take_snapshot()

    # 找最大的内存占用
    top_stats = snapshot.statistics('lineno')
    print("    Top 3 内存分配:")
    for stat in top_stats[:3]:
        print(f"    {stat}")

    tracemalloc.stop()


# ============================================================================
# 5. 修复方案：LRU 缓存
# ============================================================================

from functools import lru_cache


@lru_cache(maxsize=100)
def cached_computation(key):
    """使用 LRU 缓存，有大小限制"""
    return key * key


def demo_lru_cache():
    """LRU 缓存修复"""
    print("\n    --- LRU 缓存修复 ---")

    for i in range(1000):
        cached_computation(i)

    info = cached_computation.cache_info()
    print(f"    hits={info.hits}, misses={info.misses}")
    print(f"    maxsize={info.maxsize}, currsize={info.currsize}")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 内存泄漏演示")
    print("=" * 60)

    demo_global_leak()
    demo_closure_leak()
    demo_cache_leak()
    find_leaks()
    demo_lru_cache()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
