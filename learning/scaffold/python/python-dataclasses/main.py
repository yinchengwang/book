#!/usr/bin/env python3
"""
dataclasses.py — Python dataclasses 演示

dataclasses 减少样板代码，自动生成 __init__/__repr__/__eq__ 等。
核心概念：
1. @dataclass 基础
2. field() 定制字段
3. __post_init__ 初始化后处理
4. 不可变 dataclass
5. 继承与组合
"""

from dataclasses import dataclass, field
from typing import List, Optional
from datetime import datetime


# ============================================================================
# 1. 基础 dataclass
# ============================================================================

@dataclass
class Point:
    """简单点"""
    x: int
    y: int


@dataclass
class Employee:
    """员工（带默认值）"""
    name: str
    salary: float = 5000.0
    active: bool = True


def demo_basic():
    """基础 dataclass"""
    print("\n    --- dataclass 基础 ---")
    p = Point(1, 2)
    print(f"    Point: {p}")
    print(f"    Point == Point(1,2): {p == Point(1, 2)}")

    e = Employee("Alice", 8000)
    print(f"    Employee: {e}")


# ============================================================================
# 2. field() 定制
# ============================================================================

@dataclass
class Product:
    """产品（带 field 定制）"""
    name: str
    price: float
    tags: List[str] = field(default_factory=list)
    created_at: datetime = field(default_factory=datetime.now)


@dataclass
class Inventory:
    """库存（带默认值工厂）"""
    items: List[Product] = field(default_factory=list)
    total_value: float = 0.0

    def add_item(self, product: Product):
        self.items.append(product)
        self.total_value += product.price


def demo_field():
    """field 定制"""
    print("\n    --- field 定制 ---")
    p1 = Product("Apple", 5.0, ["fruit"])
    p2 = Product("Banana", 3.0)

    inv = Inventory()
    inv.add_item(p1)
    inv.add_item(p2)
    print(f"    产品: {p1}")
    print(f"    库存总数: {len(inv.items)}, 总价值: {inv.total_value}")


# ============================================================================
# 3. __post_init__
# ============================================================================

@dataclass
class Person:
    """人员（带后初始化）"""
    first_name: str
    last_name: str
    age: int
    full_name: str = field(init=False)

    def __post_init__(self):
        self.full_name = f"{self.first_name} {self.last_name}"


@dataclass
class Config:
    """配置（验证）"""
    host: str
    port: int

    def __post_init__(self):
        if self.port < 1 or self.port > 65535:
            raise ValueError(f"Invalid port: {self.port}")


def demo_post_init():
    """__post_init__"""
    print("\n    --- __post_init__ ---")
    p = Person("John", "Doe", 30)
    print(f"    Person: {p}")
    print(f"    full_name: {p.full_name}")

    try:
        cfg = Config("localhost", 70000)
    except ValueError as e:
        print(f"    验证失败: {e} [OK]")


# ============================================================================
# 4. 不可变 dataclass
# ============================================================================

@dataclass(frozen=True)
class ImmutablePoint:
    """不可变点"""
    x: int
    y: int


def demo_frozen():
    """不可变 dataclass"""
    print("\n    --- 不可变 dataclass ---")
    p = ImmutablePoint(1, 2)
    print(f"    ImmutablePoint: {p}")

    try:
        p.x = 5
    except Exception as e:
        print(f"    修改失败: {e} [OK]")


# ============================================================================
# 5. 继承
# ============================================================================

@dataclass
class Animal:
    """动物基类"""
    name: str
    age: int


@dataclass
class Dog(Animal):
    """狗"""
    breed: str = "Unknown"


@dataclass
class Cat(Animal):
    """猫"""
    indoor: bool = True


def demo_inheritance():
    """继承"""
    print("\n    --- dataclass 继承 ---")
    dog = Dog("Buddy", 3, "Golden Retriever")
    cat = Cat("Whiskers", 2, indoor=False)
    print(f"    Dog: {dog}")
    print(f"    Cat: {cat}")


# ============================================================================
# 6. dataclass.asdict()
# ============================================================================

from dataclasses import asdict, astuple


def demo_asdict():
    """转换为字典/元组"""
    print("\n    --- asdict/astuple ---")
    p = Point(10, 20)
    print(f"    asdict: {asdict(p)}")
    print(f"    astuple: {astuple(p)}")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python dataclasses 演示")
    print("=" * 60)

    demo_basic()
    demo_field()
    demo_post_init()
    demo_frozen()
    demo_inheritance()
    demo_asdict()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
