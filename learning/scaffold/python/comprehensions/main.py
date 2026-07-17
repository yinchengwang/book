#!/usr/bin/env python3
# comprehensions scaffold — Python 推导式
#
# 复现命令：
#   python3 main.py
#
# 演示 5 段：
#   [list_comp]   — 列表推导式
#   [dict_comp]   — 字典推导式
#   [set_comp]    — 集合推导式
#   [generator]   — 生成器表达式
#   [filter_map]  — 推导式 vs map/filter


def main():
    # === [list_comp] 列表推导式 ===
    print("[list_comp] === 列表推导式 ===")

    # 基本语法：[表达式 for item in iterable]
    squares = [x ** 2 for x in range(1, 6)]
    print(f"  [x**2 for x in range(1,6)] -> {squares}")

    # 带条件：[表达式 for item in iterable if 条件]
    evens = [x for x in range(1, 11) if x % 2 == 0]
    print(f"  [x for x in 1..10 if even] -> {evens}")

    # 多重循环
    pairs = [(x, y) for x in range(3) for y in range(3)]
    print(f"  [(x,y) for x in 0..2 for y in 0..2] -> {pairs}")

    # 嵌套推导式：矩阵转置
    matrix = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
    transposed = [[row[i] for row in matrix] for i in range(3)]
    print(f"  矩阵转置: {transposed}")

    # 条件表达式：[真值 if 条件 else 假值 for ...]
    fizzbuzz = ["Fizz" if x % 3 == 0 else x for x in range(1, 11)]
    print(f"  FizzBuzz 简化: {fizzbuzz}")

    # === [dict_comp] 字典推导式 ===
    print("\n[dict_comp] === 字典推导式 ===")

    # 基本语法：{key: value for item in iterable}
    word = "hello"
    char_count = {c: word.count(c) for c in set(word)}
    print(f"  '{word}' 字符计数: {char_count}")

    # 字典反转
    original = {"a": 1, "b": 2, "c": 3}
    inverted = {v: k for k, v in original.items()}
    print(f"  字典反转: {inverted}")

    # 筛选
    scores = {"Alice": 85, "Bob": 92, "Charlie": 78, "Diana": 95}
    passed = {name: score for name, score in scores.items() if score >= 80}
    print(f"  及格学生: {passed}")

    # === [set_comp] 集合推导式 ===
    print("\n[set_comp] === 集合推导式 ===")

    # 基本语法：{表达式 for item in iterable}
    numbers = [1, 2, 2, 3, 3, 3, 4, 4, 4, 4]
    unique_squares = {x ** 2 for x in numbers}
    print(f"  去重平方: {unique_squares}")

    # 字符串处理
    text = "Hello World"
    vowels = {c.lower() for c in text if c.lower() in "aeiou"}
    print(f"  '{text}' 元音字母: {vowels}")

    # === [generator] 生成器表达式 ===
    print("\n[generator] === 生成器表达式 ===")

    # 语法类似列表推导，但用圆括号
    # 惰性求值，不立即分配内存
    gen = (x ** 2 for x in range(1, 1000001))
    print(f"  生成器对象: {gen}")
    print(f"  类型: {type(gen).__name__}")

    # 迭代生成器
    first_five = [next(gen) for _ in range(5)]
    print(f"  前5个值: {first_five}")

    # sum() 迭代：内存高效
    total = sum(x for x in range(1, 1000001))
    print(f"  sum(1..1000000): {total}")

    # === [filter_map] 推导式 vs map/filter ===
    print("\n[filter_map] === 推导式 vs map/filter ===")

    data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

    # 用 map/filter
    result_map_filter = list(map(lambda x: x ** 2, filter(lambda x: x % 2 == 0, data)))
    print(f"  map+filter: {result_map_filter}")

    # 用列表推导式（更直观）
    result_comp = [x ** 2 for x in data if x % 2 == 0]
    print(f"  列表推导式: {result_comp}")

    # 两者等价
    print(f"  结果相同: {result_map_filter == result_comp}")

    # 性能对比（列表推导通常更快）
    import timeit
    t1 = timeit.timeit(lambda: [x ** 2 for x in range(1000) if x % 2 == 0], number=1000)
    t2 = timeit.timeit(lambda: list(map(lambda x: x ** 2, filter(lambda x: x % 2 == 0, range(1000)))), number=1000)
    print(f"  列表推导式耗时: {t1:.4f}s")
    print(f"  map+filter 耗时: {t2:.4f}s")
    print(f"  列表推导式更快: {t2 > t1}")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
