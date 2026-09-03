#!/usr/bin/env python3
"""
abc.py — Python 抽象基类（ABC）演示

ABC 用于定义接口规范，强制派生类实现抽象方法。
核心概念：
1. ABC + abstractmethod 定义抽象类
2. 抽象属性
3. 运行时类型检查
4. register() 虚拟子类
"""

from abc import ABC, abstractmethod, ABCMeta
from typing import Any


# ============================================================================
# 1. 基础抽象类
# ============================================================================

class Animal(ABC):
    """抽象动物类"""

    @abstractmethod
    def speak(self) -> str:
        """动物叫声"""
        pass

    @abstractmethod
    def move(self) -> str:
        """移动方式"""
        pass


class Dog(Animal):
    """狗"""

    def speak(self) -> str:
        return "汪汪！"

    def move(self) -> str:
        return "奔跑"


class Cat(Animal):
    """猫"""

    def speak(self) -> str:
        return "喵~"

    def move(self) -> str:
        return "跳跃"


def demo_basic():
    """基础抽象类"""
    print("\n    --- 抽象基类 ---")
    dog = Dog()
    cat = Cat()
    print(f"    狗: speak={dog.speak()}, move={dog.move()}")
    print(f"    猫: speak={cat.speak()}, move={cat.move()}")


# ============================================================================
# 2. 抽象属性
# ============================================================================

class Shape(ABC):
    """抽象形状类"""

    @property
    @abstractmethod
    def area(self) -> float:
        """面积"""
        pass

    @property
    @abstractmethod
    def perimeter(self) -> float:
        """周长"""
        pass


class Rectangle(Shape):
    """矩形"""

    def __init__(self, width: float, height: float):
        self.width = width
        self.height = height

    @property
    def area(self) -> float:
        return self.width * self.height

    @property
    def perimeter(self) -> float:
        return 2 * (self.width + self.height)


class Circle(Shape):
    """圆形"""

    def __init__(self, radius: float):
        self.radius = radius

    @property
    def area(self) -> float:
        import math
        return math.pi * self.radius ** 2

    @property
    def perimeter(self) -> float:
        import math
        return 2 * math.pi * self.radius


def demo_abstract_property():
    """抽象属性"""
    print("\n    --- 抽象属性 ---")
    rect = Rectangle(3, 4)
    circle = Circle(2)

    print(f"    矩形(3x4): area={rect.area:.2f}, perimeter={rect.perimeter:.2f}")
    print(f"    圆(r=2): area={circle.area:.2f}, perimeter={circle.perimeter:.2f}")


# ============================================================================
# 3. 运行时类型检查
# ============================================================================

def process_shape(shape: Shape) -> str:
    """处理形状（运行时检查）"""
    if not isinstance(shape, Shape):
        raise TypeError(f"Expected Shape, got {type(shape)}")
    return f"area={shape.area:.2f}, perimeter={shape.perimeter:.2f}"


def demo_isinstance():
    """运行时类型检查"""
    print("\n    --- 运行时检查 ---")
    shapes = [Rectangle(3, 4), Circle(2), Rectangle(5, 5)]

    for s in shapes:
        print(f"    {type(s).__name__}: {process_shape(s)}")


# ============================================================================
# 4. register() 虚拟子类
# ============================================================================

class_registered = False


class Panther:
    """美洲豹（通过 register 成为虚拟子类）"""

    def speak(self) -> str:
        return "咆哮！"

    def move(self) -> str:
        return "潜伏"


def demo_register():
    """register 虚拟子类"""
    global class_registered
    print("\n    --- register 虚拟子类 ---")

    if not class_registered:
        Animal.register(Panther)
        class_registered = True

    panther = Panther()
    print(f"    Panther 是 Animal 的虚拟子类: {isinstance(panther, Animal)}")
    print(f"    panther.speak() = {panther.speak()}")


# ============================================================================
# 5. ABCMeta 直接使用
# ============================================================================

def demo_abcmeta():
    """ABCMeta 直接使用"""
    print("\n    --- ABCMeta ---")

    MyInterface = ABCMeta('MyInterface', (), {
        'required_method': abstractmethod(lambda self: None)
    })

    print(f"    MyInterface 是 ABCMeta: {isinstance(MyInterface, ABCMeta)}")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python ABC 抽象基类演示")
    print("=" * 60)

    demo_basic()
    demo_abstract_property()
    demo_isinstance()
    demo_register()
    demo_abcmeta()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
