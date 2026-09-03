#!/usr/bin/env python3
# functions scaffold — Python 函数
#
# 复现命令：
#   python3 main.py
#
# 演示 7 段：
#   [def]     — 函数定义与调用
#   [default] — 默认参数
#   [args]    — *args 可变位置参数
#   [kwargs]  — **kwargs 可变关键字参数
#   [lambda]  — lambda 表达式
#   [closure] — 闭包
#   [builtin] — 内置函数 map/filter/reduce

from functools import reduce


def main():
    # === [def] 函数定义与调用 ===
    print("[def] === 函数定义与调用 ===")

    def greet(name: str) -> str:
        """问候函数，返回问候字符串"""
        return f"Hello, {name}!"

    message = greet("Alice")
    print(f"  greet('Alice') -> '{message}'")

    # 多返回值（实际是 tuple）
    def divide(a: float, b: float) -> tuple[float, float]:
        quotient = int(a // b)
        remainder = a % b
        return quotient, remainder

    q, r = divide(10, 3)
    print(f"  divide(10, 3) -> quotient={q}, remainder={r}")

    # === [default] 默认参数 ===
    print("\n[default] === 默认参数 ===")

    def connect(host: str, port: int = 5432, timeout: int = 30) -> str:
        """连接数据库，返回连接字符串"""
        return f"postgresql://{host}:{port} (timeout={timeout}s)"

    print(f"  connect('localhost')         -> {connect('localhost')}")
    print(f"  connect('db.example.com', 5433) -> {connect('db.example.com', 5433)}")
    print(f"  connect('db.example.com', 5433, 60) -> {connect('db.example.com', 5433, 60)}")

    # 默认参数陷阱：不要用可变对象作为默认值
    def append_to(element, target=[]):  # 陷阱！列表是可变类型
        target.append(element)
        return target

    def append_fixed(element, target=None):  # 正确做法
        if target is None:
            target = []
        target.append(element)
        return target

    print(f"  append_to(1) -> {append_to(1)}")  # 每次调用会累积！
    print(f"  append_to(2) -> {append_to(2)}")  # 结果是 [1, 2] 而非 [2]

    # === [args] *args 可变位置参数 ===
    print("\n[args] === *args 可变位置参数 ===")

    def sum_all(*args: int) -> int:
        """求所有参数之和"""
        total = 0
        for num in args:
            total += num
        return total

    print(f"  sum_all(1, 2, 3)         -> {sum_all(1, 2, 3)}")
    print(f"  sum_all(10, 20, 30, 40)  -> {sum_all(10, 20, 30, 40)}")

    # 解包参数
    numbers = [1, 2, 3, 4, 5]
    print(f"  sum_all(*numbers)        -> {sum_all(*numbers)}")

    # === [kwargs] **kwargs 可变关键字参数 ===
    print("\n[kwargs] === **kwargs 可变关键字参数 ===")

    def make_user(name: str, **kwargs) -> dict:
        """创建用户，附加可选字段"""
        user = {"name": name}
        user.update(kwargs)
        return user

    user1 = make_user("Alice", age=30, city="Beijing")
    user2 = make_user("Bob", role="admin")

    print(f"  make_user('Alice', age=30, city='Beijing') -> {user1}")
    print(f"  make_user('Bob', role='admin')              -> {user2}")

    # 解包字典
    extra = {"email": "alice@example.com", "phone": "123456"}
    user3 = make_user("Charlie", **extra)
    print(f"  make_user('Charlie', **extra)               -> {user3}")

    # === [lambda] lambda 表达式 ===
    print("\n[lambda] === lambda 表达式 ===")

    # 匿名函数
    square = lambda x: x ** 2
    print(f"  square(5) -> {square(5)}")

    # 带条件表达式
    max_num = lambda a, b: a if a > b else b
    print(f"  max_num(3, 7) -> {max_num(3, 7)}")

    # 与高阶函数结合
    nums = [3, 1, 4, 1, 5, 9, 2, 6]
    sorted_nums = sorted(nums, key=lambda x: -x)  # 降序排列
    print(f"  sorted([3,1,4,1,5,9,2,6], key=lambda x: -x) -> {sorted_nums}")

    # === [closure] 闭包 ===
    print("\n[closure] === 闭包 ===")

    def make_multiplier(factor: int):
        """创建乘法器闭包"""
        def multiplier(x: int) -> int:
            return x * factor
        return multiplier

    double = make_multiplier(2)
    triple = make_multiplier(3)
    ten_times = make_multiplier(10)

    print(f"  double(5)    -> {double(5)}")
    print(f"  triple(5)    -> {triple(5)}")
    print(f"  ten_times(5) -> {ten_times(5)}")

    # 闭包捕获变量
    def counter(start=0):
        count = start
        def inc():
            nonlocal count
            count += 1
            return count
        return inc

    c = counter(0)
    print(f"  counter(): {c()}, {c()}, {c()}, {c()}")

    # === [builtin] 内置高阶函数 ===
    print("\n[builtin] === map/filter/reduce ===")

    nums = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

    # map: 对每个元素应用函数
    squares = list(map(lambda x: x ** 2, nums))
    print(f"  map(x**2, 1..10) -> {squares}")

    # filter: 筛选满足条件的元素
    evens = list(filter(lambda x: x % 2 == 0, nums))
    print(f"  filter(even, 1..10) -> {evens}")

    # reduce: 累积计算
    total = reduce(lambda a, b: a + b, nums, 0)
    product = reduce(lambda a, b: a * b, nums, 1)
    print(f"  reduce(+, 1..10)    -> {total}")
    print(f"  reduce(*, 1..10)   -> {product}")

    # 组合使用
    result = reduce(
        lambda a, b: a + b,
        map(lambda x: x ** 2,
            filter(lambda x: x % 2 == 0, nums))
    )
    print(f"  sum of squares of evens -> {result}")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
