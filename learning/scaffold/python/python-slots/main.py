#!/usr/bin/env python3
"""
__slots__.py — Python __slots__ 演示

__slots__ 是 Python 内存优化机制，限制实例可以拥有的属性。
核心概念：
1. __slots__ 限制实例属性
2. 内存节省机制
3. __slots__ 与继承
4. __slots__ 与 property
"""

import sys
from typing import Any


# ============================================================================
# 1. 基础 __slots__
# ============================================================================

class Regular:
    """普通类（不使用 __slots__）"""
    def __init__(self, name: str, value: int):
        self.name = name
        self.value = value


class Slotted:
    """使用 __slots__ 的类"""
    __slots__ = ('name', 'value')

    def __init__(self, name: str, value: int):
        self.name = name
        self.value = value


def demo_basic():
    """基础对比"""
    print("\n    --- __slots__ 基础 ---")
    r = Regular("regular", 1)
    s = Slotted("slotted", 2)

    print(f"    普通类实例 __dict__: {r.__dict__}")
    print(f"    Slotted 实例 __dict__: {s.__dict__}")

    # __slots__ 限制属性
    try:
        s.extra = "not allowed"
    except AttributeError as e:
        print(f"    添加额外属性: {e} [OK]")


# ============================================================================
# 2. 内存对比
# ============================================================================

def memory_comparison():
    """内存占用对比"""
    print("\n    --- 内存对比 ---")

    # 创建大量实例
    N = 100000

    regular_instances = [Regular(f"r{i}", i) for i in range(N)]
    slotted_instances = [Slotted(f"s{i}", i) for i in range(N)]

    regular_size = sum(sys.getsizeof(obj) + sys.getsizeof(obj.__dict__)
                       for obj in regular_instances)
    slotted_size = sum(sys.getsizeof(obj) for obj in slotted_instances)

    print(f"    创建 {N} 个实例:")
    print(f"    普通类: {regular_size / 1024 / 1024:.2f} MB")
    print(f"    __slots__: {slotted_size / 1024 / 1024:.2f} MB")
    print(f"    节省: {(1 - slotted_size / regular_size) * 100:.1f}%")


# ============================================================================
# 3. __slots__ 与继承
# ============================================================================

class BaseWithSlots:
    """带 __slots__ 的基类"""
    __slots__ = ('name',)

    def __init__(self, name: str):
        self.name = name


class DerivedWithSlots(BaseWithSlots):
    """派生类继承 __slots__"""
    __slots__ = ('value',)

    def __init__(self, name: str, value: int):
        super().__init__(name)
        self.value = value


class DerivedWithDict(BaseWithSlots):
    """派生类不继承 __slots__"""
    # 没有定义 __slots__，会获得 __dict__

    def __init__(self, name: str, value: int):
        super().__init__(name)
        self.value = value


def demo_inheritance():
    """继承与 __slots__"""
    print("\n    --- __slots__ 与继承 ---")

    d1 = DerivedWithSlots("d1", 1)
    d2 = DerivedWithDict("d2", 2)

    print(f"    派生类有 __slots__: {hasattr(d1, '__dict__')}")
    print(f"    派生类无 __slots__: {hasattr(d2, '__dict__')}")

    print(f"    d1.name={d1.name}, d1.value={d1.value}")
    print(f"    d2.name={d2.name}, d2.value={d2.value}")


# ============================================================================
# 4. __slots__ 与 property
# ============================================================================

class Optimized:
    """__slots__ 与 property 配合"""
    __slots__ = ('_name', '_age')

    def __init__(self, name: str, age: int):
        self._name = name
        self._age = age

    @property
    def name(self):
        return self._name

    @name.setter
    def name(self, value: str):
        self._name = value

    @property
    def age(self):
        return self._age

    @age.setter
    def age(self, value: int):
        if value < 0:
            raise ValueError("Age cannot be negative")
        self._age = value


def demo_with_property():
    """__slots__ 与 property"""
    print("\n    --- __slots__ 与 property ---")
    obj = Optimized("Alice", 30)
    print(f"    obj.name = {obj.name}")
    print(f"    obj.age = {obj.age}")

    obj.age = 31
    print(f"    修改后 age = {obj.age}")

    try:
        obj.age = -5
    except ValueError as e:
        print(f"    验证失败: {e} [OK]")


# ============================================================================
# 5. __slots__ 与类方法
# ============================================================================

class Point:
    """带 __slots__ 的简单类"""
    __slots__ = ('x', 'y')

    def __init__(self, x: int, y: int):
        self.x = x
        self.y = y

    def __repr__(self):
        return f"Point({self.x}, {self.y})"

    def distance_from_origin(self):
        return (self.x ** 2 + self.y ** 2) ** 0.5


def demo_class_methods():
    """__slots__ 与方法"""
    print("\n    --- __slots__ 与方法 ---")
    p = Point(3, 4)
    print(f"    {p}")
    print(f"    距离原点: {p.distance_from_origin():.2f}")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python __slots__ 演示")
    print("=" * 60)

    demo_basic()
    memory_comparison()
    demo_inheritance()
    demo_with_property()
    demo_class_methods()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
