#!/usr/bin/env python3
"""
profiling.py — Python 性能分析演示

演示 cProfile + timeit + tracemalloc。
"""

import cProfile
import pstats
import timeit
import tracemalloc
from io import StringIO


# ============================================================================
# 1. timeit（微基准测试）
# ============================================================================

def demo_timeit():
    """timeit 精确测量小代码片段"""
    print("\n    --- timeit ---")

    # 列表推导式 vs 生成器
    code1 = "[x**2 for x in range(1000)]"
    code2 = "list(x**2 for x in range(1000))"

    t1 = timeit.timeit(code1, number=1000)
    t2 = timeit.timeit(code2, number=1000)

    print(f"    列表推导式: {t1:.4f}s")
    print(f"    生成器: {t2:.4f}s")
    print(f"    列表推导式更快: {t1 < t2}")


# ============================================================================
# 2. cProfile（函数级分析）
# ============================================================================

def slow_function():
    """模拟慢函数"""
    total = 0
    for i in range(10000):
        total += i ** 2
    return total


def another_function():
    """另一个函数"""
    return sum(range(5000))


def profiled_function():
    """被分析的函数"""
    result = slow_function()
    result += another_function()
    return result


def demo_cprofile():
    """cProfile 函数级分析"""
    print("\n    --- cProfile ---")

    profiler = cProfile.Profile()
    profiler.enable()

    # 执行被分析的代码
    for _ in range(100):
        profiled_function()

    profiler.disable()

    # 输出统计信息
    s = StringIO()
    ps = pstats.Stats(profiler, stream=s).sort_stats('cumulative')
    ps.print_stats(5)  # 前 5 行
    print(s.getvalue()[:500])


# ============================================================================
# 3. tracemalloc（内存分析）
# ============================================================================

def create_objects():
    """创建对象"""
    result = []
    for i in range(100):
        result.append({'id': i, 'data': 'x' * 100})
    return result


def demo_tracemalloc():
    """tracemalloc 内存分析"""
    print("\n    --- tracemalloc ---")

    tracemalloc.start()

    # 执行前
    snapshot1 = tracemalloc.take_snapshot()

    # 执行代码
    objects = create_objects()

    # 执行后
    snapshot2 = tracemalloc.take_snapshot()

    # 对比
    stats = snapshot2.compare_to(snapshot1, 'lineno')

    print("    内存增长 Top 3:")
    for stat in stats[:3]:
        print(f"    {stat}")

    current, peak = tracemalloc.get_traced_memory()
    print(f"    当前: {current / 1024:.1f} KB, 峰值: {peak / 1024:.1f} KB")

    tracemalloc.stop()


# ============================================================================
# 4. memory_profiler
# ============================================================================

def demo_memory_usage():
    """内存使用情况"""
    print("\n    --- 内存使用 ---")

    import sys

    # 估算对象大小
    data = {'key': 'value', 'numbers': list(range(100))}
    size = sys.getsizeof(data)
    print(f"    dict 大小: {size} bytes")

    # 列表
    lst = list(range(1000))
    size = sys.getsizeof(lst)
    print(f"    list(1000) 大小: {size} bytes")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 性能分析演示")
    print("=" * 60)

    demo_timeit()
    demo_cprofile()
    demo_tracemalloc()
    demo_memory_usage()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
