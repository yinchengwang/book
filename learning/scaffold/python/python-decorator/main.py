#!/usr/bin/env python3
"""
decorators.py — Python 装饰器演示

装饰器是 Python 核心语法，用于在不修改原函数的前提下扩展功能。
核心概念：
1. 装饰器是一个接收函数并返回函数的函数
2. @decorator 语法糖等价于 func = decorator(func)
3. functools.wraps 保留原函数的元信息
4. 带参数的装饰器需要多层嵌套
"""

import functools
import time
from typing import Callable, Any


# ============================================================================
# 1. 基础装饰器
# ============================================================================

def simple_decorator(func: Callable) -> Callable:
    """最简单的装饰器：在函数执行前后打印信息"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        print(f"[装饰器] 调用 {func.__name__} 前")
        result = func(*args, **kwargs)
        print(f"[装饰器] 调用 {func.__name__} 后")
        return result
    return wrapper


@simple_decorator
def greet(name: str) -> str:
    """问候函数"""
    return f"Hello, {name}!"


# ============================================================================
# 2. 带参数的装饰器
# ============================================================================

def repeat(times: int):
    """带参数的装饰器工厂：重复执行函数指定次数"""
    def decorator(func: Callable) -> Callable:
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            results = []
            for _ in range(times):
                result = func(*args, **kwargs)
                results.append(result)
            return results
        return wrapper
    return decorator


@repeat(times=3)
def say_hello(name: str) -> str:
    return f"Hello, {name}!"


# ============================================================================
# 3. 计时装饰器
# ============================================================================

def timer(func: Callable) -> Callable:
    """计时装饰器：测量函数执行时间"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        start = time.perf_counter()
        result = func(*args, **kwargs)
        elapsed = time.perf_counter() - start
        print(f"[计时] {func.__name__} 执行耗时: {elapsed:.6f}s")
        return result
    return wrapper


@timer
def slow_function(n: int) -> int:
    """模拟耗时操作"""
    total = 0
    for i in range(n):
        total += i ** 2
    return total


# ============================================================================
# 4. 参数验证装饰器
# ============================================================================

def validate_positive(func: Callable) -> Callable:
    """参数验证装饰器：确保参数为正数"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        # 检查位置参数
        for i, arg in enumerate(args):
            if isinstance(arg, (int, float)) and arg <= 0:
                raise ValueError(f"参数 {i} 必须为正数，实际为 {arg}")
        # 检查关键字参数
        for key, value in kwargs.items():
            if isinstance(value, (int, float)) and value <= 0:
                raise ValueError(f"参数 {key} 必须为正数，实际为 {value}")
        return func(*args, **kwargs)
    return wrapper


@validate_positive
def divide(a: float, b: float) -> float:
    """安全除法"""
    return a / b


# ============================================================================
# 5. 类装饰器
# ============================================================================

def add_method(cls: type) -> type:
    """类装饰器：为类添加新方法"""
    def new_method(self):
        return f"来自新方法的问候: {self.name}"
    cls.greet = new_method
    return cls


@add_method
class Person:
    def __init__(self, name: str):
        self.name = name

    def say_name(self):
        return f"我的名字是 {self.name}"


# ============================================================================
# 6. 装饰器栈（多个装饰器叠加）
# ============================================================================

def debug(func: Callable) -> Callable:
    """调试装饰器：打印函数调用信息"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        print(f"[调试] 调用 {func.__name__}({args}, {kwargs})")
        result = func(*args, **kwargs)
        print(f"[调试] {func.__name__} 返回 {result}")
        return result
    return wrapper


def memoize(func: Callable) -> Callable:
    """记忆化装饰器：缓存结果"""
    cache: dict = {}

    @functools.wraps(func)
    def wrapper(*args):
        if args in cache:
            print(f"[记忆] 命中缓存: {args}")
            return cache[args]
        result = func(*args)
        cache[args] = result
        return result
    return wrapper


@debug
@memoize
def fibonacci(n: int) -> int:
    """斐波那契数列（带记忆化）"""
    if n < 2:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)


# ============================================================================
# 7. 使用 functools.wraps 保留元信息
# ============================================================================

def documented(func: Callable) -> Callable:
    """演示 functools.wraps 的作用"""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        return func(*args, **kwargs)
    return wrapper


@documented
def calculate(x: int, y: int) -> int:
    """计算 x + y"""
    return x + y


# ============================================================================
# 主函数：演示所有装饰器
# ============================================================================

def main():
    print("=" * 60)
    print("Python 装饰器演示")
    print("=" * 60)

    # 1. 基础装饰器
    print("\n[1] 基础装饰器:")
    result = greet("Alice")
    print(f"    返回值: {result}")

    # 2. 带参数的装饰器
    print("\n[2] 带参数的装饰器:")
    results = say_hello("Bob")
    print(f"    重复3次: {results}")

    # 3. 计时装饰器
    print("\n[3] 计时装饰器:")
    slow_function(10000)

    # 4. 参数验证装饰器
    print("\n[4] 参数验证装饰器:")
    try:
        divide(10, 2)
        print("    divide(10, 2) = 5 [OK]")
    except ValueError as e:
        print(f"    错误: {e}")

    try:
        divide(-5, 2)
    except ValueError as e:
        print(f"    divide(-5, 2) 抛出异常: {e} [OK]")

    # 5. 类装饰器
    print("\n[5] 类装饰器:")
    p = Person("Charlie")
    print(f"    {p.say_name()}")
    print(f"    {p.greet()}")  # 来自类装饰器添加的方法

    # 6. 装饰器栈
    print("\n[6] 装饰器栈 (debug + memoize):")
    for i in range(5):
        print(f"    fibonacci({i}) = {fibonacci(i)}")

    # 7. functools.wraps 保留元信息
    print("\n[7] functools.wraps 保留元信息:")
    print(f"    函数名: {calculate.__name__}")
    print(f"    文档: {calculate.__doc__}")

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
