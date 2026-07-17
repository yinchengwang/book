#!/usr/bin/env python3
"""
bytecode.py — Python 字节码演示

使用 dis 模块分析 Python 字节码。
"""

import dis
import sys


# ============================================================================
# 1. dis 反汇编基础
# ============================================================================

def demo_dis_basic():
    """反汇编函数"""
    print("\n    --- dis 反汇编 ---")

    def add(a, b):
        return a + b

    print("    add 函数字节码:")
    dis.dis(add)


# ============================================================================
# 2. 对比不同实现
# ============================================================================

def for_loop_sum(n):
    """for 循环求和"""
    total = 0
    for i in range(n):
        total += i
    return total


def sum_builtin(n):
    """内置 sum"""
    return sum(range(n))


def demo_comparison():
    """对比不同实现"""
    print("\n    --- for vs sum ---")

    print("    for_loop_sum:")
    dis.dis(for_loop_sum)

    print("    sum_builtin:")
    dis.dis(sum_builtin)


# ============================================================================
# 3. 查看代码对象属性
# ============================================================================

def demo_code_attrs():
    """代码对象属性"""
    print("\n    --- 代码对象属性 ---")

    def example():
        x = 1
        y = 2
        return x + y

    print(f"    co_code: {example.__code__.co_code[:20]}...")
    print(f"    co_varnames: {example.__code__.co_varnames}")
    print(f"    co_consts: {example.__code__.co_consts}")
    print(f"    co_names: {example.__code__.co_names}")
    print(f"    co_stacksize: {example.__code__.co_stacksize}")


# ============================================================================
# 4. Python 版本字节码差异
# ============================================================================

def demo_version():
    """Python 版本信息"""
    print("\n    --- Python 版本 ---")
    print(f"    Python: {sys.version}")
    print(f"    字节码指令数: {len(dis.opmap)}")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 字节码演示")
    print("=" * 60)

    demo_dis_basic()
    demo_code_attrs()
    demo_version()

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
