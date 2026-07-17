#!/usr/bin/env python3
# modules scaffold — Python 模块与包
#
# 复现命令：
#   python3 main.py
#
# 演示 5 段：
#   [import]     — import 与 from import
#   [package]    — 包结构与 __init__.py
#   [name]       — __name__ 的作用
#   [path]       — 模块搜索路径
#   [alias]      — 导入别名与再导出

import sys
import os
import utils
from utils import add, subtract, multiply, divide


def main():
    # === [import] import 与 from import ===
    print("[import] === import 与 from import ===")

    # 标准库导入
    print(f"  sys.version     -> {sys.version.split()[0]}")
    print(f"  os.getcwd()     -> {os.getcwd()}")

    # 使用 utils 模块的函数
    result = add(10, 20)
    print(f"  add(10,20)      -> {result}")

    # 使用别名避免命名冲突
    import utils as u
    result = u.subtract(100, 30)
    print(f"  u.subtract(100,30) -> {result}")

    # === [package] 包结构 ===
    print("\n[package] === 包结构 ===")

    # 查看模块的 __file__ 属性（定义位置）
    print(f"  utils.__file__  -> {utils.__file__}")
    print(f"  utils.__name__  -> {utils.__name__}")

    # 查看已导入的模块
    print(f"  'utils' in sys.modules -> {'utils' in sys.modules}")

    # === [name] __name__ 的作用 ===
    print("\n[name] === __name__ 的作用 ===")

    # __name__ 在直接运行时是 '__main__'
    # 在被导入时是模块名
    print(f"  main.py __name__ -> {__name__}")
    print(f"  utils.__name__   -> {utils.__name__}")

    # 典型的 if __name__ == "__main__": 用法
    print("  if __name__ == '__main__': 的典型用途：")
    print("    1. 直接运行时执行测试代码")
    print("    2. 被导入时不执行测试代码")

    # === [path] 模块搜索路径 ===
    print("\n[path] === 模块搜索路径 ===")

    print(f"  sys.path 前3项:")
    for i, p in enumerate(sys.path[:3]):
        print(f"    [{i}] {p}")

    # 动态添加搜索路径
    custom_path = "/tmp/my_modules"
    if os.path.exists(custom_path):
        sys.path.insert(0, custom_path)
        print(f"  添加自定义路径: {custom_path}")

    # === [alias] 导入别名与再导出 ===
    print("\n[alias] === 导入别名与再导出 ===")

    # 使用别名
    import statistics as stats
    data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    mean = stats.mean(data)
    stdev = stats.stdev(data)
    print(f"  statistics.mean([1..10])  -> {mean}")
    print(f"  statistics.stdev([1..10]) -> {stdev:.2f}")

    # 直接调用导入的函数
    print(f"  divide(10, 2)     -> {divide(10, 2)}")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
