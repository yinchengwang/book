#!/usr/bin/env python3
# context_managers scaffold — Python 上下文管理器
#
# 复现命令：
#   python3 main.py
#
# 演示 5 段：
#   [with_basic]   — with 语句基本用法
#   [enter_exit]   — __enter__/__exit__ 协议
#   [contextlib]   — contextlib 工具
#   [nested]       — 嵌套上下文管理器
#   [redirect]     — 上下文管理器应用


import sys
import os
import tempfile
import time
from contextlib import contextmanager


def main():
    # === [with_basic] with 语句基本用法 ===
    print("[with_basic] === with 语句基本用法 ===")

    # 文件操作
    with open(tempfile.mktemp(), 'w') as f:
        f.write("Hello, Context Manager!")
        print("  文件已写入")

    # 锁操作
    import threading
    lock = threading.Lock()

    with lock:
        print("  锁已获取，正在访问共享资源")
        print("  锁将在 with 块结束时自动释放")

    # === [enter_exit] __enter__/__exit__ 协议 ===
    print("\n[enter_exit] === __enter__/__exit__ 协议 ===")

    class Timer:
        """计时器上下文管理器"""

        def __init__(self, name: str = "Timer"):
            self.name = name
            self.start = None
            self.elapsed = None

        def __enter__(self):
            self.start = time.time()
            print(f"  [{self.name}] 开始")
            return self  # 返回值绑定到 as 后的变量

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.elapsed = time.time() - self.start
            print(f"  [{self.name}] 结束，耗时 {self.elapsed:.4f}s")
            # 返回 False 表示不抑制异常
            return False

    with Timer("计算任务") as t:
        # 模拟计算
        result = sum(i ** 2 for i in range(10000))

    print(f"    结果: {result}")

    # === [contextlib] contextlib 工具 ===
    print("\n[contextlib] === contextlib 工具 ===")

    # @contextmanager 装饰器
    @contextmanager
    def managed_resource(name: str, value: int):
        """用生成器实现上下文管理器"""
        print(f"  [{name}] 获取资源 (value={value})")
        try:
            yield value  # 相当于 __enter__ 返回值
        finally:
            print(f"  [{name}] 释放资源")

    with managed_resource("数据库连接", 42) as conn:
        print(f"    使用连接: {conn}")

    # 嵌套 contextmanager
    @contextmanager
    def tag(name: str):
        """输出 HTML 标签"""
        print(f"<{name}>")
        yield
        print(f"</{name}>")

    with tag("html"):
        print("  内容")
        with tag("body"):
            print("    页面内容")

    # === [nested] 嵌套上下文管理器 ===
    print("\n[nested] === 嵌套上下文管理器 ===")

    # 多个 with 语句可以合并
    with open(tempfile.mktemp(), 'w') as f1, \
         open(tempfile.mktemp(), 'w') as f2:
        f1.write("文件1")
        f2.write("文件2")
        print("  两个文件同时打开")

    # === [redirect] 上下文管理器应用 ===
    print("\n[redirect] === 上下文管理器应用 ===")

    from contextlib import redirect_stdout, suppress

    # 重定向 stdout
    output_file = tempfile.mktemp()
    with open(output_file, 'w') as f:
        with redirect_stdout(f):
            print("这条输出被重定向到文件")
            print("而不是打印到终端")

    with open(output_file, 'r') as f:
        content = f.read().strip()
        print(f"  重定向的内容: {content}")

    # suppress 忽略特定异常
    with suppress(FileNotFoundError):
        os.remove("nonexistent_file.txt")
        print("  文件不存在但被忽略")

    # 组合使用
    class Database:
        """模拟数据库连接"""
        def __init__(self):
            self.connected = False

        def connect(self):
            self.connected = True
            print("  数据库已连接")

        def close(self):
            self.connected = False
            print("  数据库已关闭")

    @contextmanager
    def db_transaction(db):
        """数据库事务上下文管理器"""
        try:
            yield db
            print("  提交事务")
        except Exception as e:
            print(f"  回滚事务: {e}")
            raise

    db = Database()
    db.connect()
    with db_transaction(db):
        print("    执行 SQL...")

    print("\n=== PASS ===")

if __name__ == "__main__":
    main()
