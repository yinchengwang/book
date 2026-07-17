#!/usr/bin/env python3
"""
descriptors.py — Python 描述符演示

描述符是实现了 __get__/__set__/__delete__ 的对象，可控制属性访问。
核心概念：
1. 数据描述符（实现了 __get__ 和 __set__）
2. 非数据描述符（只实现了 __get__）
3. property 是内置描述符
4. 描述符实现 ORM 字段验证
"""

from typing import Any, Callable


# ============================================================================
# 1. 基础描述符
# ============================================================================

class Positive:
    """属性值必须为正数"""

    def __set_name__(self, owner, name):
        self.name = name
        self.storage_name = f'_{name}'

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        return getattr(obj, self.storage_name, None)

    def __set__(self, obj, value):
        if value is not None and value <= 0:
            raise ValueError(f"{self.name} must be positive, got {value}")
        setattr(obj, self.storage_name, value)


class Person:
    """使用描述符"""
    age = Positive()
    score = Positive()

    def __init__(self, name: str, age: int, score: int):
        self.name = name
        self.age = age
        self.score = score


def demo_basic():
    """基础描述符"""
    print("\n    --- 基础描述符 ---")
    p = Person("Alice", 25, 100)
    print(f"    Alice: age={p.age}, score={p.score}")

    try:
        p.age = -5
    except ValueError as e:
        print(f"    设置负数 age: {e} [OK]")


# ============================================================================
# 2. 描述符实现 Lazy 属性
# ============================================================================

class Lazy:
    """延迟计算属性"""

    def __init__(self, func: Callable) -> None:
        self.func = func
        self.attr_name = None

    def __set_name__(self, owner, name):
        self.attr_name = name

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        value = self.func(obj)
        setattr(obj, self.attr_name, value)
        return value


class DataProcessor:
    """数据处理器（延迟加载）"""
    def __init__(self, data: list):
        self._raw_data = data

    @Lazy
    def processed_data(self):
        """第一次访问时才计算"""
        print(f"    [Lazy] 计算 processed_data...")
        return [x * 2 for x in self._raw_data]

    @Lazy
    def statistics(self):
        """统计信息"""
        print(f"    [Lazy] 计算 statistics...")
        return {
            'count': len(self._raw_data),
            'sum': sum(self._raw_data),
            'avg': sum(self._raw_data) / len(self._raw_data)
        }


def demo_lazy():
    """延迟属性"""
    print("\n    --- 延迟属性 ---")
    dp = DataProcessor([1, 2, 3, 4, 5])
    print(f"    第一次访问: {dp.processed_data}")
    print(f"    第二次访问（使用缓存）: {dp.processed_data}")
    print(f"    统计: {dp.statistics}")


# ============================================================================
# 3. 描述符实现日志记录
# ============================================================================

class Logged:
    """属性访问日志"""

    def __set_name__(self, owner, name):
        self.name = name
        self.storage_name = f'_{name}'

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        value = getattr(obj, self.storage_name, None)
        print(f"    [读取] {self.name} = {value}")
        return value

    def __set__(self, obj, value):
        print(f"    [写入] {self.name} = {value}")
        setattr(obj, self.storage_name, value)


class Account:
    """带日志的账户"""
    balance = Logged()
    name = Logged()

    def __init__(self, name: str, balance: float):
        self.name = name
        self.balance = balance


def demo_logged():
    """属性访问日志"""
    print("\n    --- 属性访问日志 ---")
    acc = Account("Test", 1000)
    acc.balance += 500
    print(f"    当前余额: {acc.balance}")


# ============================================================================
# 4. method_descriptor（函数描述符）
# ============================================================================

def demo_method_descriptor():
    """方法是非数据描述符"""
    print("\n    --- 方法描述符 ---")

    class MyClass:
        def method(self):
            return "instance method"

        @classmethod
        def classmethod_demo(cls):
            return "class method"

        @staticmethod
        def staticmethod_demo():
            return "static method"

    obj = MyClass()
    print(f"    实例方法: {obj.method()}")
    print(f"    类方法: {MyClass.classmethod_demo()}")
    print(f"    静态方法: {MyClass.staticmethod_demo()}")


# ============================================================================
# 5. 描述符验证链
# ============================================================================

class Validator(Positive):
    """带验证链的描述符"""

    def __init__(self, min_val=None, max_val=None):
        self.min_val = min_val
        self.max_val = max_val

    def __set__(self, obj, value):
        if value is not None:
            if self.min_val is not None and value < self.min_val:
                raise ValueError(f"Value {value} < min {self.min_val}")
            if self.max_val is not None and value > self.max_val:
                raise ValueError(f"Value {value} > max {self.max_val}")
        super().__set__(obj, value)


class Config:
    """配置类（多描述符）"""
    port = Validator(min_val=1, max_val=65535)
    timeout = Validator(min_val=0, max_val=300)
    retries = Validator(min_val=0, max_val=10)

    def __init__(self, port: int, timeout: int, retries: int):
        self.port = port
        self.timeout = timeout
        self.retries = retries


def demo_validator():
    """验证链"""
    print("\n    --- 验证链 ---")
    cfg = Config(8080, 30, 3)
    print(f"    有效配置: port={cfg.port}, timeout={cfg.timeout}")

    try:
        cfg.port = 70000
    except ValueError as e:
        print(f"    无效端口: {e} [OK]")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 描述符演示")
    print("=" * 60)

    demo_basic()
    demo_lazy()
    demo_logged()
    demo_method_descriptor()
    demo_validator()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
