#!/usr/bin/env python3
"""
gc.py — Python 垃圾回收演示

演示引用计数 + 循环垃圾回收 + gc 模块。
"""

import gc
import sys


# ============================================================================
# 1. 引用计数基础
# ============================================================================

def demo_refcount():
    """引用计数演示"""
    print("\n    --- 引用计数 ---")

    a = []  # 引用计数 = 1
    print(f"    a = [] 后: refcount = {sys.getrefcount(a) - 1}")

    b = a  # 引用计数 = 2
    print(f"    b = a 后: refcount = {sys.getrefcount(a) - 1}")

    del b  # 引用计数 = 1
    print(f"    del b 后: refcount = {sys.getrefcount(a) - 1}")

    del a  # 引用计数 = 0, 对象被回收
    print("    del a 后: 对象被回收")


# ============================================================================
# 2. 循环引用问题
# ============================================================================

class Node:
    """链表节点"""
    def __init__(self, value):
        self.value = value
        self.next = None

    def __repr__(self):
        return f"Node({self.value})"


def demo_cycle():
    """循环引用"""
    print("\n    --- 循环引用 ---")

    # 创建循环引用
    a = Node(1)
    b = Node(2)
    a.next = b
    b.next = a  # 形成循环

    print(f"    a = {a}, b = {b}")
    print(f"    a.next = b, b.next = a (循环)")

    # 删除引用
    del a
    del b

    # Python GC 会处理循环引用
    print("    del a, del b 后，GC 会回收")


# ============================================================================
# 3. gc 模块
# ============================================================================

def demo_gc():
    """gc 模块演示"""
    print("\n    --- gc 模块 ---")

    # 禁用自动 GC
    gc.disable()
    print(f"    GC 禁用: {gc.isenabled()}")

    # 创建循环引用
    class Cache:
        def __init__(self, key):
            self.key = key
            self.data = None

        def __repr__(self):
            return f"Cache({self.key})"

    cache1 = Cache(1)
    cache2 = Cache(2)
    cache1.ref = cache2
    cache2.ref = cache1

    # 手动触发 GC
    collected = gc.collect()
    print(f"    gc.collect() 回收: {collected} 个对象")

    gc.enable()
    print(f"    GC 启用: {gc.isenabled()}")


# ============================================================================
# 4. gc.get_stats()
# ============================================================================

def demo_gc_stats():
    """GC 统计"""
    print("\n    --- GC 统计 ---")

    # 创建大量对象触发 GC
    for _ in range(1000):
        obj = {'data': list(range(100))}

    stats = gc.get_stats()
    print(f"    GC 代数: {len(stats)}")
    for i, s in enumerate(stats):
        print(f"    Generation {i}: collections={s['collections']}, collected={s['collected']}")


# ============================================================================
# 5. 弱引用
# ============================================================================

import weakref


def demo_weakref():
    """弱引用"""
    print("\n    --- 弱引用 ---")

    class Target:
        pass

    obj = Target()
    weak = weakref.ref(obj)

    print(f"    obj exists: {weak() is not None}")
    del obj
    print(f"    after del obj: {weak() is None}")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 垃圾回收演示")
    print("=" * 60)

    demo_refcount()
    demo_cycle()
    demo_gc()
    demo_gc_stats()
    demo_weakref()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
