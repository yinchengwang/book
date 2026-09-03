#!/usr/bin/env python3
# utils 模块 — 工具函数集合
#
# 提供基本的数学运算函数，供 modules/main.py 演示导入

__all__ = ["add", "subtract", "multiply", "divide", "power"]


def add(a: int | float, b: int | float) -> int | float:
    """加法"""
    return a + b


def subtract(a: int | float, b: int | float) -> int | float:
    """减法"""
    return a - b


def multiply(a: int | float, b: int | float) -> int | float:
    """乘法"""
    return a * b


def divide(a: int | float, b: int | float) -> float:
    """除法（浮点结果）"""
    if b == 0:
        raise ZeroDivisionError("除数不能为零")
    return a / b


def power(base: int | float, exp: int | float) -> int | float:
    """幂运算"""
    return base ** exp


def _helper(x: int | float) -> int | float:
    """内部辅助函数（不导出）"""
    return x * 2


if __name__ == "__main__":
    # 模块自测代码
    print("utils 模块自测：")
    print(f"  add(1, 2)      -> {add(1, 2)}")
    print(f"  subtract(5, 3) -> {subtract(5, 3)}")
    print(f"  multiply(4, 5) -> {multiply(4, 5)}")
    print(f"  divide(10, 2)  -> {divide(10, 2)}")
    print(f"  power(2, 3)    -> {power(2, 3)}")
