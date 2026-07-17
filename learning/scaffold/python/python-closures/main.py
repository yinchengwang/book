#!/usr/bin/env python3
"""
closures.py — Python 闭包演示

闭包是引用了外部作用域变量的函数。
核心概念：
1. 嵌套函数引用外部变量
2. 闭包变量存储在 __closure__ 属性
3. nonlocal 关键字修改外部变量
4. 闭包工厂函数
"""

from typing import Callable


# ============================================================================
# 1. 基础闭包
# ============================================================================

def outer(x: int) -> Callable:
    """基础闭包：内部函数引用外部变量"""
    def inner(y: int) -> int:
        return x + y  # 引用外部变量 x
    return inner


# ============================================================================
# 2. 闭包工厂
# ============================================================================

def make_multiplier(factor: int) -> Callable:
    """乘数工厂：创建指定倍数的乘法器"""
    def multiplier(n: int) -> int:
        return n * factor
    return multiplier


def make_power(exp: int) -> Callable:
    """幂函数工厂"""
    def power(base: int) -> int:
        result = 1
        for _ in range(exp):
            result *= base
        return result
    return power


def make_logger(prefix: str) -> Callable:
    """日志工厂"""
    def log(*args):
        print(f"    [{prefix}] {' '.join(str(a) for a in args)}")
    return log


# ============================================================================
# 3. nonlocal 修改外部变量
# ============================================================================

def counter(start: int = 0) -> Callable:
    """计数器闭包"""
    count = start

    def inc(delta: int = 1) -> int:
        nonlocal count
        count += delta
        return count

    def get() -> int:
        return count

    def reset():
        nonlocal count
        count = start

    return inc, get, reset


# ============================================================================
# 4. 闭包实现私有变量
# ============================================================================

def make_stack():
    """栈数据结构（闭包实现私有变量）"""
    items = []

    def push(item):
        items.append(item)

    def pop():
        if not items:
            return None
        return items.pop()

    def peek():
        if not items:
            return None
        return items[-1]

    def is_empty():
        return len(items) == 0

    def size():
        return len(items)

    return push, pop, peek, is_empty, size


# ============================================================================
# 5. 闭包实现记忆化
# ============================================================================

def memoize(func: Callable) -> Callable:
    """记忆化装饰器（闭包实现）"""
    cache = {}

    def wrapper(*args):
        if args not in cache:
            cache[args] = func(*args)
        return cache[args]

    wrapper.cache = cache  # 暴露缓存用于调试
    return wrapper


@memoize
def fibonacci(n: int) -> int:
    if n < 2:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)


# ============================================================================
# 6. 闭包延迟求值
# ============================================================================

def lazy(fn: Callable) -> Callable:
    """延迟求值包装器"""
    value = None
    evaluated = False

    def get_value():
        nonlocal value, evaluated
        if not evaluated:
            value = fn()
            evaluated = True
        return value

    return get_value


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 闭包演示")
    print("=" * 60)

    # 1. 基础闭包
    print("\n[1] 基础闭包:")
    add5 = outer(5)
    add10 = outer(10)
    print(f"    outer(5)(3) = {add5(3)}")
    print(f"    outer(10)(3) = {add10(3)}")

    # 2. 闭包工厂
    print("\n[2] 闭包工厂:")
    double = make_multiplier(2)
    triple = make_multiplier(3)
    print(f"    double(7) = {double(7)}")
    print(f"    triple(7) = {triple(7)}")
    print(f"    平方: {make_power(2)(5)}")
    print(f"    立方: {make_power(3)(2)}")

    logger = make_logger("INFO")
    logger("系统启动")
    logger("用户登录", "admin")

    # 3. nonlocal
    print("\n[3] nonlocal 修改外部变量:")
    inc, get, reset = counter(10)
    print(f"    初始值: {get()}")
    print(f"    inc(5): {inc(5)}")
    print(f"    inc(3): {inc(3)}")
    print(f"    当前值: {get()}")
    reset()
    print(f"    reset 后: {get()}")

    # 4. 私有变量
    print("\n[4] 闭包实现私有变量（栈）:")
    push, pop, peek, is_empty, size = make_stack()
    push(1)
    push(2)
    push(3)
    print(f"    size={size()}, peek={peek()}")
    print(f"    pop: {pop()}")
    print(f"    pop: {pop()}")
    print(f"    is_empty: {is_empty()}")

    # 5. 记忆化
    print("\n[5] 闭包记忆化:")
    fib = fibonacci
    for i in range(10):
        print(f"    fib({i}) = {fib(i)}")
    print(f"    缓存大小: {len(fib.cache)}")

    # 6. 延迟求值
    print("\n[6] 延迟求值:")
    import time
    expensive = lazy(lambda: (time.sleep(0.1), 42)[1])
    print(f"    第一次调用: {expensive()}")
    print(f"    第二次调用（使用缓存）: {expensive()}")

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
