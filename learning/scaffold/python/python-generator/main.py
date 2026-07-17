#!/usr/bin/env python3
"""
generators.py — Python 生成器演示

生成器是惰性求值的迭代器，通过 yield 语句产生值。
核心概念：
1. 生成器函数使用 yield 返回值
2. 生成器对象是迭代器，支持 next()
3. 生成器表达式类似列表推导式但惰性求值
4. itertools 模块提供丰富的生成器工具
"""

import itertools
from typing import Iterator, Generator


# ============================================================================
# 1. 基础生成器
# ============================================================================

def count_up_to(n: int) -> Generator[int, None, None]:
    """从 1 数到 n"""
    for i in range(1, n + 1):
        yield i


def fibonacci_generator(n: int) -> Generator[int, None, None]:
    """生成前 n 个斐波那契数"""
    a, b = 0, 1
    for _ in range(n):
        yield a
        a, b = b, a + b


def primes_generator(limit: int) -> Generator[int, None, None]:
    """生成小于 limit 的所有素数"""
    for num in range(2, limit):
        is_prime = True
        for i in range(2, int(num ** 0.5) + 1):
            if num % i == 0:
                is_prime = False
                break
        if is_prime:
            yield num


# ============================================================================
# 2. 生成器表达式
# ============================================================================

def demo_generator_expressions():
    """生成器表达式 vs 列表推导式"""
    # 列表推导式（立即求值）
    squares_list = [x ** 2 for x in range(10)]
    print(f"    列表推导式: {squares_list}")

    # 生成器表达式（惰性求值）
    squares_gen = (x ** 2 for x in range(10))
    print(f"    生成器表达式: {squares_gen}")
    print(f"    next(): {next(squares_gen)}")
    print(f"    next(): {next(squares_gen)}")
    print(f"    list(): {list(squares_gen)}")


# ============================================================================
# 3. itertools 应用
# ============================================================================

def demo_itertools():
    """itertools 模块常用函数"""
    print("\n    --- itertools 演示 ---")

    # count: 无限计数器
    counter = itertools.count(start=10, step=2)
    print(f"    count(10,2): 前5个 {list(itertools.islice(counter, 5))}")

    # cycle: 无限循环
    cycle = itertools.cycle(['A', 'B', 'C'])
    print(f"    cycle: 前7个 {list(itertools.islice(cycle, 7))}")

    # accumulate: 累积计算
    acc = itertools.accumulate([1, 2, 3, 4, 5])
    print(f"    accumulate: {list(acc)}")

    # chain: 连接多个迭代器
    ch = itertools.chain([1, 2], ['a', 'b'], [3, 4])
    print(f"    chain: {list(ch)}")

    # compress: 按条件过滤
    data = ['A', 'B', 'C', 'D', 'E']
    selectors = [True, False, True, False, True]
    print(f"    compress: {list(itertools.compress(data, selectors))}")

    # groupby: 分组
    data = [('A', 1), ('A', 2), ('B', 1), ('B', 3), ('C', 1)]
    grouped = {k: list(v) for k, v in itertools.groupby(data, key=lambda x: x[0])}
    print(f"    groupby: {grouped}")

    # permutations: 排列
    print(f"    permutations('ABC',2): {list(itertools.permutations('ABC', 2))}")

    # combinations: 组合
    print(f"    combinations('ABC',2): {list(itertools.combinations('ABC', 2))}")


# ============================================================================
# 4. 生成器管道
# ============================================================================

def is_even(n: int) -> bool:
    return n % 2 == 0


def double(n: int) -> int:
    return n * 2


def pipeline_example():
    """管道式处理：数据流经多个转换"""
    numbers = range(1, 21)

    # 管道：过滤偶数 -> 平方 -> 取前5
    result = list(
        itertools.islice(
            map(double,
                filter(is_even, numbers)
            ),
            5
        )
    )
    print(f"    管道 [1..20] -> 过滤偶数 -> 平方 -> 取前5: {result}")

    # 使用生成器链
    def generator_pipeline(data: Iterator[int]) -> Generator[int, None, None]:
        for x in data:
            if x % 2 == 0:  # 过滤
                yield x ** 2  # 转换

    result2 = list(itertools.islice(generator_pipeline(range(1, 21)), 5))
    print(f"    生成器管道: {result2}")


# ============================================================================
# 5. 协程风格的生成器（数据消费）
# ============================================================================

def data_processor():
    """协程风格的生成器：边生产边消费"""
    total = 0
    count = 0
    while True:
        value = yield  # 接收外部值
        if value is None:
            break
        total += value
        count += 1
    return total / count if count > 0 else 0


# ============================================================================
# 6. 生成器发送值
# ============================================================================

def counter_with_send():
    """使用 send() 向生成器发送值"""
    def echo():
        while True:
            received = yield
            print(f"    收到: {received}")
    gen = echo()
    next(gen)  # 启动生成器
    gen.send("Hello")
    gen.send(42)
    gen.send([1, 2, 3])


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 生成器演示")
    print("=" * 60)

    # 1. 基础生成器
    print("\n[1] 基础生成器:")
    print(f"    count_up_to(5): {list(count_up_to(5))}")
    print(f"    fibonacci_generator(10): {list(fibonacci_generator(10))}")
    print(f"    primes_generator(50): {list(primes_generator(50))}")

    # 2. 生成器表达式
    print("\n[2] 生成器表达式:")
    demo_generator_expressions()

    # 3. itertools 应用
    print("\n[3] itertools 应用:")
    demo_itertools()

    # 4. 管道
    print("\n[4] 生成器管道:")
    pipeline_example()

    # 5. 协程风格
    print("\n[5] 协程风格（send）:")
    counter_with_send()

    # 6. yield from
    print("\n[6] yield from（委托生成器）:")
    def gen1():
        yield 1
        yield 2

    def gen2():
        yield 3
        yield from gen1()  # 委托给 gen1
        yield 4

    print(f"    yield from: {list(gen2())}")

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
