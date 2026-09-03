#!/usr/bin/env python3
"""
metaclass.py — Python 元类演示

元类是创建类的类，type() 是 Python 内置的元类。
核心概念：
1. type() 可以动态创建类
2. __new__ 控制类创建过程
3. __init__ 初始化类属性
4. ORM 场景的元类应用
"""

from typing import Any, Dict, List, Optional, Type, Callable


# ============================================================================
# 1. type() 动态创建类
# ============================================================================

def demo_type_function():
    """type() 三个参数版本：type(name, bases, attrs)"""
    print("\n    --- type() 动态创建类 ---")

    # 动态创建简单类
    Point = type('Point', (), {
        'x': 0,
        'y': 0,
        'distance': lambda self: (self.x ** 2 + self.y ** 2) ** 0.5
    })

    p = Point()
    p.x, p.y = 3, 4
    print(f"    Point(3,4) 距离原点: {p.distance():.2f}")

    # 带继承的动态类
    class Base:
        value = 100

    Derived = type('Derived', (Base,), {
        'name': 'derived',
        'get_value': lambda self: self.value
    })

    d = Derived()
    print(f"    继承自 Base: value={d.get_value()}")


# ============================================================================
# 2. 基础元类
# ============================================================================

class Meta(type):
    """自定义元类：打印类创建信息"""
    def __new__(mcs, name, bases, attrs):
        print(f"    [Meta] 创建类: {name}")
        print(f"    [Meta] 基类: {bases}")
        print(f"    [Meta] 属性数: {len(attrs)}")
        return super().__new__(mcs, name, bases, attrs)


class MyClass(metaclass=Meta):
    """使用 Meta 元类"""
    x = 10
    y = 20


# ============================================================================
# 3. ORM 风格的元类
# ============================================================================

class Field:
    """字段描述符"""
    def __init__(self, field_type: str, default: Any = None):
        self.field_type = field_type
        self.default = default
        self.name = None

    def __set_name__(self, owner, name):
        self.name = name

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        return obj.__dict__.get(self.name, self.default)

    def __set__(self, obj, value):
        obj.__dict__[self.name] = value


class ModelMeta(type):
    """Model 元类：自动收集字段并创建表结构"""
    def __new__(mcs, name, bases, attrs):
        # 收集所有字段
        fields = {}
        for attr_name, attr_value in attrs.items():
            if isinstance(attr_value, Field):
                fields[attr_name] = attr_value

        # 添加 _meta 属性
        attrs['_fields'] = fields
        attrs['_table_name'] = name.lower() + 's'

        cls = super().__new__(mcs, name, bases, attrs)

        # 打印表结构
        print(f"    [ORM] 表: {cls._table_name}")
        for fname, f in fields.items():
            print(f"    [ORM]   - {fname}: {f.field_type}")

        return cls


class Model(metaclass=ModelMeta):
    """Model 基类"""
    pass


class User(Model):
    """用户模型"""
    id = Field('INTEGER', 0)
    name = Field('VARCHAR(100)', '')
    email = Field('VARCHAR(255)', '')
    age = Field('INTEGER', 0)


class Product(Model):
    """产品模型"""
    id = Field('INTEGER', 0)
    name = Field('VARCHAR(200)', '')
    price = Field('DECIMAL(10,2)', 0.0)


# ============================================================================
# 4. 元类注册模式
# ============================================================================

class RegistryMeta(type):
    """注册表元类：自动注册子类"""
    _registry: Dict[str, type] = {}

    def __new__(mcs, name, bases, attrs):
        cls = super().__new__(mcs, name, bases, attrs)
        if name != 'Base':
            mcs._registry[name] = cls
            print(f"    [注册] {name} -> {mcs._registry[name]}")
        return cls


class PluginBase(metaclass=RegistryMeta):
    """插件基类"""
    _registry = {}

    def run(self):
        raise NotImplementedError


class TextPlugin(PluginBase):
    def run(self):
        return "文本插件"


class ImagePlugin(PluginBase):
    def run(self):
        return "图像插件"


# ============================================================================
# 5. 元类 + 装饰器
# ============================================================================

def add_method(name: str):
    """为类添加方法的装饰器"""
    def decorator(cls):
        def new_method(self):
            return f"{name}: {self}"
        cls.greet = new_method
        return cls
    return decorator


@add_method("打招呼")
class Greeting:
    def __init__(self, name):
        self.name = name

    def __str__(self):
        return self.name


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 元类演示")
    print("=" * 60)

    # 1. type() 动态创建
    print("\n[1] type() 动态创建类:")
    demo_type_function()

    # 2. 自定义元类
    print("\n[2] 自定义元类:")
    print(f"    MyClass.x = {MyClass.x}")

    # 3. ORM 元类
    print("\n[3] ORM 元类:")
    u = User()
    u.id = 1
    u.name = "Alice"
    u.email = "alice@example.com"
    print(f"    User 实例: id={u.id}, name={u.name}")
    print(f"    _table_name: {User._table_name}")

    p = Product()
    p.id = 1
    p.name = "Laptop"
    p.price = 999.99
    print(f"    Product 实例: {p.name} = ${p.price}")

    # 4. 注册表元类
    print("\n[4] 注册表元类:")
    print(f"    注册表: {list(RegistryMeta._registry.keys())}")
    for name, plugin_cls in RegistryMeta._registry.items():
        if name != 'Base':
            print(f"    {name}.run() = {plugin_cls().run()}")

    # 5. 元类 + 装饰器
    print("\n[5] 元类 + 装饰器:")
    g = Greeting("Bob")
    print(f"    {g.greet()}")

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
