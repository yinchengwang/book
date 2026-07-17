#!/usr/bin/env python3
"""
type_hints.py — Python 类型提示演示

类型提示是 Python 3.5+ 引入的功能，提高代码可读性和 IDE 支持。
核心概念：
1. TypeVar 泛型变量
2. Generic 泛型类
3. Protocol 结构子类型
4. Union/Optional
5. mypy 静态检查
"""

from typing import (
    TypeVar, Generic, List, Dict, Set, Tuple, Optional,
    Union, Callable, Protocol, Any, Iterable, Iterator
)


# ============================================================================
# 1. 基础类型注解
# ============================================================================

def greet(name: str) -> str:
    """带类型注解的函数"""
    return f"Hello, {name}!"


def process_numbers(numbers: List[int]) -> Tuple[int, float]:
    """列表处理，返回 (和, 平均值)"""
    total = sum(numbers)
    avg = total / len(numbers) if numbers else 0
    return total, avg


def demo_basic():
    """基础类型注解"""
    print("\n    --- 基础类型注解 ---")
    print(f"    greet('World'): {greet('World')}")
    total, avg = process_numbers([1, 2, 3, 4, 5])
    print(f"    process_numbers([1,2,3,4,5]): total={total}, avg={avg}")


# ============================================================================
# 2. TypeVar 泛型
# ============================================================================

T = TypeVar('T')
K = TypeVar('K')
V = TypeVar('V')


class Stack(Generic[T]):
    """泛型栈"""

    def __init__(self) -> None:
        self._items: List[T] = []

    def push(self, item: T) -> None:
        self._items.append(item)

    def pop(self) -> T:
        if not self._items:
            raise IndexError("Stack is empty")
        return self._items.pop()

    def peek(self) -> T:
        return self._items[-1]

    def is_empty(self) -> bool:
        return len(self._items) == 0


def demo_generics():
    """泛型类"""
    print("\n    --- 泛型类 ---")
    int_stack = Stack[int]()
    int_stack.push(1)
    int_stack.push(2)
    print(f"    int_stack.pop(): {int_stack.pop()}")

    str_stack = Stack[str]()
    str_stack.push("hello")
    str_stack.push("world")
    print(f"    str_stack.pop(): {str_stack.pop()}")


# ============================================================================
# 3. Protocol（结构子类型）
# ============================================================================

class Drawable(Protocol):
    """可绘制协议"""
    def draw(self) -> None:
        ...


class Circle:
    def __init__(self, radius: float):
        self.radius = radius

    def draw(self) -> None:
        print(f"    绘制圆，半径={self.radius}")


class Square:
    def __init__(self, side: float):
        self.side = side

    def draw(self) -> None:
        print(f"    绘制正方形，边长={self.side}")


def render_all(drawables: Iterable[Drawable]) -> None:
    """渲染所有可绘制对象"""
    print("\n    --- Protocol 渲染 ---")
    for d in drawables:
        d.draw()


# ============================================================================
# 4. Union 和 Optional
# ============================================================================

def parse_number(s: str) -> Union[int, float, None]:
    """解析数字，失败返回 None"""
    try:
        if '.' in s:
            return float(s)
        return int(s)
    except ValueError:
        return None


def process_value(value: Optional[str] = None) -> str:
    """Optional 等价于 Union[T, None]"""
    if value is None:
        return "未提供"
    return f"值: {value}"


def demo_union():
    """Union 和 Optional"""
    print("\n    --- Union/Optional ---")
    print(f"    parse_number('42'): {parse_number('42')}")
    print(f"    parse_number('3.14'): {parse_number('3.14')}")
    print(f"    parse_number('abc'): {parse_number('abc')}")
    print(f"    process_value(): {process_value()}")
    print(f"    process_value('hi'): {process_value('hi')}")


# ============================================================================
# 5. Callable 类型
# ============================================================================

def apply_twice(func: Callable[[int], int], value: int) -> int:
    """对值应用函数两次"""
    return func(func(value))


def double(x: int) -> int:
    return x * 2


def demo_callable():
    """Callable 类型"""
    print("\n    --- Callable ---")
    result = apply_twice(double, 5)
    print(f"    apply_twice(double, 5): {result}")


# ============================================================================
# 6. 类型别名
# ============================================================================

# 类型别名
Matrix = List[List[float]]
Distance = float


def matrix_multiply(a: Matrix, b: Matrix) -> Matrix:
    """矩阵乘法（简化版）"""
    n = len(a)
    result = [[0.0] * n for _ in range(n)]
    for i in range(n):
        for j in range(n):
            for k in range(n):
                result[i][j] += a[i][k] * b[k][j]
    return result


def demo_type_alias():
    """类型别名"""
    print("\n    --- 类型别名 ---")
    a: Matrix = [[1, 2], [3, 4]]
    b: Matrix = [[5, 6], [7, 8]]
    result = matrix_multiply(a, b)
    print(f"    2x2 矩阵乘法: {result}")


# ============================================================================
# 7. 泛型函数
# ============================================================================

def first(iterator: Iterator[T]) -> Optional[T]:
    """返回迭代器第一个元素"""
    try:
        return next(iterator)
    except StopIteration:
        return None


def reverse(items: List[T]) -> List[T]:
    """反转列表"""
    return items[::-1]


def demo_generic_func():
    """泛型函数"""
    print("\n    --- 泛型函数 ---")
    nums = [1, 2, 3, 4, 5]
    print(f"    first([1,2,3]): {first(iter(nums))}")
    print(f"    reverse(['a','b','c']): {reverse(['a','b','c'])}")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 类型提示演示")
    print("=" * 60)

    demo_basic()
    demo_generics()

    print("\n[3] Protocol（结构子类型）:")
    shapes: List[Drawable] = [Circle(1.0), Square(2.0)]
    render_all(shapes)

    demo_union()
    demo_callable()
    demo_type_alias()
    demo_generic_func()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
