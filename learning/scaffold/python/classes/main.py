#!/usr/bin/env python3
# classes scaffold — Python 面向对象
#
# 复现命令：
#   python3 main.py
#
# 演示 6 段：
#   [class_def]    — 类定义与 __init__
#   [inheritance]  — 继承与多态
#   [magic]        — 特殊方法（__str__/__repr__/__len__）
#   [property]     — @property 装饰器
#   [classmethod]  — @classmethod vs @staticmethod
#   [diamond]      — 多重继承与 MRO


def main():
    # === [class_def] 类定义与 __init__ ===
    print("[class_def] === 类定义与 __init__ ===")

    class Person:
        """人员类"""

        def __init__(self, name: str, age: int):
            self.name = name  # 实例属性
            self.age = age

        def greet(self) -> str:
            return f"Hello, I'm {self.name}"

        def __str__(self) -> str:
            return f"Person({self.name}, {self.age})"

    alice = Person("Alice", 30)
    bob = Person("Bob", 25)

    print(f"  alice.greet() -> '{alice.greet()}'")
    print(f"  alice         -> {alice}")
    print(f"  type(alice)   -> {type(alice).__name__}")

    # === [inheritance] 继承与多态 ===
    print("\n[inheritance] === 继承与多态 ===")

    class Student(Person):
        """学生类，继承自 Person"""

        def __init__(self, name: str, age: int, grade: str):
            super().__init__(name, age)  # 调用父类 __init__
            self.grade = grade

        def greet(self) -> str:
            # 重写父类方法，体现多态
            return f"Hi, I'm {self.name}, a {self.grade} student"

    student = Student("Charlie", 20, "Junior")
    print(f"  student.greet() -> '{student.greet()}'")
    print(f"  isinstance(student, Person)   -> {isinstance(student, Person)}")
    print(f"  isinstance(student, Student)  -> {isinstance(student, Student)}")

    # 多态：不同对象调用同一方法，结果不同
    people: list[Person] = [alice, bob, student]
    for p in people:
        print(f"  {p.name}.greet() -> '{p.greet()}'")

    # === [magic] 特殊方法 ===
    print("\n[magic] === 特殊方法 ===")

    class Vector:
        """二维向量类"""

        def __init__(self, x: float, y: float):
            self.x = x
            self.y = y

        def __repr__(self) -> str:
            # 调试友好的字符串表示
            return f"Vector({self.x}, {self.y})"

        def __str__(self) -> str:
            # 用户友好的字符串表示
            return f"({self.x}, {self.y})"

        def __add__(self, other: "Vector") -> "Vector":
            # 加法运算
            return Vector(self.x + other.x, self.y + other.y)

        def __mul__(self, scalar: float) -> "Vector":
            # 数乘运算
            return Vector(self.x * scalar, self.y * scalar)

        def __len__(self) -> int:
            # 返回向量维度
            return 2

        def __eq__(self, other: object) -> bool:
            if not isinstance(other, Vector):
                return False
            return self.x == other.x and self.y == other.y

    v1 = Vector(1.0, 2.0)
    v2 = Vector(3.0, 4.0)

    print(f"  repr(v1)     -> {repr(v1)}")
    print(f"  str(v1)      -> {str(v1)}")
    print(f"  v1 + v2      -> {v1 + v2}")
    print(f"  v1 * 2       -> {v1 * 2}")
    print(f"  len(v1)      -> {len(v1)}")
    print(f"  v1 == v1     -> {v1 == v1}")
    print(f"  v1 == v2     -> {v1 == v2}")

    # === [property] @property 装饰器 ===
    print("\n[property] === @property 装饰器 ===")

    class Circle:
        """圆类，使用 property 实现只读属性"""

        def __init__(self, radius: float):
            self._radius = radius  # 私有属性（下划线约定）

        @property
        def radius(self) -> float:
            """半径，只读属性"""
            return self._radius

        @property
        def diameter(self) -> float:
            """直径，只读属性（计算得出）"""
            return self._radius * 2

        @property
        def area(self) -> float:
            """面积，只读属性"""
            import math
            return math.pi * self._radius ** 2

    circle = Circle(5.0)
    print(f"  circle.radius   -> {circle.radius}")
    print(f"  circle.diameter -> {circle.diameter}")
    print(f"  circle.area     -> {circle.area:.2f}")

    # radius 是只读的，不能直接赋值
    # circle.radius = 10  # AttributeError

    # === [classmethod] @classmethod vs @staticmethod ===
    print("\n[classmethod] === @classmethod vs @staticmethod ===")

    class Date:
        """日期类"""

        def __init__(self, year: int, month: int, day: int):
            self.year = year
            self.month = month
            self.day = day

        @staticmethod
        def is_valid_date(year: int, month: int, day: int) -> bool:
            """静态方法：判断日期是否合法（不依赖实例）"""
            if month < 1 or month > 12:
                return False
            if day < 1 or day > 31:
                return False
            return True

        @classmethod
        def from_string(cls, date_str: str) -> "Date":
            """类方法：从字符串创建 Date 对象"""
            parts = date_str.split("-")
            year, month, day = int(parts[0]), int(parts[1]), int(parts[2])
            return cls(year, month, day)

        def __str__(self) -> str:
            return f"{self.year:04d}-{self.month:02d}-{self.day:02d}"

    # 静态方法：直接通过类调用
    print(f"  Date.is_valid_date(2024, 2, 29) -> {Date.is_valid_date(2024, 2, 29)}")
    print(f"  Date.is_valid_date(2024, 2, 30) -> {Date.is_valid_date(2024, 2, 30)}")

    # 类方法：通过字符串创建对象
    date = Date.from_string("2024-01-15")
    print(f"  Date.from_string('2024-01-15') -> {date}")

    # === [diamond] 多重继承与 MRO ===
    print("\n[diamond] === 多重继承与 MRO ===")

    class A:
        def who(self) -> str:
            return "A"

    class B(A):
        def who(self) -> str:
            return "B"

    class C(A):
        def who(self) -> str:
            return "C"

    class D(B, C):  # 菱形继承
        def who(self) -> str:
            return "D"

    d = D()
    print(f"  D().who()   -> {d.who()}")
    print(f"  MRO of D    -> {[c.__name__ for c in D.__mro__]}")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
