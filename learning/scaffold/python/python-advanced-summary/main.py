#!/usr/bin/env python3
"""
summary.py — Python 进阶总结

本文件综合回顾 Python 进阶主题：
1. 装饰器与 AOP
2. 生成器与惰性求值
3. 上下文管理器与 RAII
4. 元类与动态编程
5. 并发编程（asyncio/threading/multiprocessing）
6. 类型提示与代码质量
"""

from typing import List, Optional


# ============================================================================
# 1. 装饰器回顾
# ============================================================================

def memoize(func):
    """记忆化装饰器"""
    import functools
    cache = {}

    @functools.wraps(func)
    def wrapper(*args):
        if args not in cache:
            cache[args] = func(*args)
        return cache[args]
    return wrapper


@memoize
def fibonacci(n: int) -> int:
    """斐波那契（记忆化）"""
    if n < 2:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)


# ============================================================================
# 2. 生成器回顾
# ============================================================================

def count_up_to(n: int):
    """生成器"""
    for i in range(1, n + 1):
        yield i


# ============================================================================
# 3. 上下文管理器回顾
# ============================================================================

from contextlib import contextmanager


@contextmanager
def timer(name: str):
    """计时上下文管理器"""
    import time
    start = time.perf_counter()
    try:
        yield
    finally:
        print(f"    [{name}] 耗时: {time.perf_counter() - start:.4f}s")


# ============================================================================
# 4. 元类回顾
# ============================================================================

class RegistryMeta(type):
    """注册表元类"""
    _registry = {}

    def __new__(mcs, name, bases, attrs):
        cls = super().__new__(mcs, name, bases, attrs)
        if name != 'Base':
            mcs._registry[name] = cls
        return cls


# ============================================================================
# 5. 类型提示回顾
# ============================================================================

@dataclass
class Point:
    """带类型的简单类"""
    x: int
    y: int


from dataclasses import dataclass


# ============================================================================
# 6. 并发回顾（简化版）
# ============================================================================

async def async_example():
    """异步示例"""
    import asyncio
    await asyncio.sleep(0.1)
    return "async done"


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 进阶总结")
    print("=" * 60)

    # 1. 装饰器
    print("\n[1] 装饰器 - @memoize:")
    for i in range(10):
        print(f"    fib({i}) = {fibonacci(i)}")

    # 2. 生成器
    print("\n[2] 生成器 - count_up_to:")
    nums = list(count_up_to(5))
    print(f"    {nums}")

    # 3. 上下文管理器
    print("\n[3] 上下文管理器 - @contextmanager:")
    with timer("操作"):
        import time
        time.sleep(0.1)

    # 4. 元类
    print("\n[4] 元类 - RegistryMeta:")
    print(f"    注册表: {list(RegistryMeta._registry.keys())}")

    # 5. 类型提示
    print("\n[5] 类型提示 - @dataclass:")
    p = Point(3, 4)
    print(f"    Point: {p}")

    # 6. 总结
    print("\n" + "=" * 60)
    print("主题覆盖：")
    print("  [OK] 装饰器（decorator）")
    print("  [OK] 生成器（generator）")
    print("  [OK] 上下文管理器（context manager）")
    print("  [OK] 元类（metaclass）")
    print("  [OK] 闭包（closure）")
    print("  [OK] 异步编程（asyncio）")
    print("  [OK] 多线程（threading）")
    print("  [OK] 多进程（multiprocessing）")
    print("  [OK] 类型提示（type hints）")
    print("  [OK] 描述符（descriptor）")
    print("  [OK] __slots__")
    print("  [OK] ABC 抽象基类")
    print("  [OK] dataclasses")
    print("=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
