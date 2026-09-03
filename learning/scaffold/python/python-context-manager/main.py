#!/usr/bin/env python3
"""
context_managers.py — Python 上下文管理器演示

上下文管理器通过 with 语句管理资源，确保资源在使用后正确释放。
核心概念：
1. __enter__ 和 __exit__ 协议
2. contextlib.contextmanager 装饰器
3. @contextmanager + yield 实现
4. 嵌套上下文和资源管理最佳实践
"""

import os
import time
import tempfile
import shutil
from contextlib import contextmanager, ExitStack
from typing import Generator, Any


# ============================================================================
# 1. 类实现上下文管理器
# ============================================================================

class Timer:
    """计时器上下文管理器"""
    def __init__(self, name: str = "操作"):
        self.name = name
        self.start = None
        self.elapsed = None

    def __enter__(self):
        self.start = time.perf_counter()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.elapsed = time.perf_counter() - self.start
        print(f"    [{self.name}] 耗时: {self.elapsed:.4f}s")
        return False  # 不抑制异常


class FileHandler:
    """文件操作上下文管理器"""
    def __init__(self, filepath: str, mode: str = 'r'):
        self.filepath = filepath
        self.mode = mode
        self.file = None

    def __enter__(self):
        self.file = open(self.filepath, self.mode)
        return self.file

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.file:
            self.file.close()
        return False


class DBConnection:
    """模拟数据库连接上下文管理器"""
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.connected = False

    def __enter__(self):
        print(f"    [DB] 连接到 {self.host}:{self.port}")
        self.connected = True
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        print(f"    [DB] 断开连接")
        self.connected = False
        return False

    def query(self, sql: str):
        if not self.connected:
            raise RuntimeError("未连接数据库")
        print(f"    [DB] 执行: {sql}")
        return f"结果: {sql}"


# ============================================================================
# 2. contextlib.contextmanager 装饰器
# ============================================================================

@contextmanager
def timer_cm(name: str = "操作") -> Generator[None, None, None]:
    """使用 @contextmanager 的计时器"""
    start = time.perf_counter()
    try:
        yield
    finally:
        elapsed = time.perf_counter() - start
        print(f"    [{name}] 耗时: {elapsed:.4f}s")


@contextmanager
def temp_directory() -> Generator[str, None, None]:
    """临时目录上下文管理器"""
    tmpdir = tempfile.mkdtemp()
    print(f"    [临时目录] 创建: {tmpdir}")
    try:
        yield tmpdir
    finally:
        shutil.rmtree(tmpdir)
        print(f"    [临时目录] 清理: {tmpdir}")


@contextmanager
def managed_resource(name: str, fail_prob: float = 0) -> Generator[dict, None, None]:
    """模拟带失败概率的资源"""
    print(f"    [资源 {name}] 开启")
    resource = {"name": name, "active": True}
    try:
        yield resource
    except Exception as e:
        print(f"    [资源 {name}] 异常: {e}")
        raise
    finally:
        resource["active"] = False
        print(f"    [资源 {name}] 关闭")


# ============================================================================
# 3. ExitStack（管理多个上下文）
# ============================================================================

def demo_exit_stack():
    """ExitStack：同时管理多个上下文"""
    print("\n    --- ExitStack 演示 ---")
    resources = []

    with ExitStack() as stack:
        # 入栈多个资源
        for i in range(3):
            resource = stack.enter_context(managed_resource(f"res{i}"))
            resources.append(resource)

        # 手动弹出资源
        stack.pop_all()

    print(f"    剩余资源: {[r['name'] for r in resources]}")


# ============================================================================
# 4. 嵌套上下文
# ============================================================================

def nested_contexts():
    """嵌套上下文管理器"""
    print("\n    --- 嵌套上下文 ---")
    with Timer("外层"):
        with Timer("中层"):
            with timer_cm("内层"):
                time.sleep(0.05)


# ============================================================================
# 5. 异常处理与 __exit__
# ============================================================================

def exception_handling():
    """上下文管理器异常处理"""
    print("\n    --- 异常处理 ---")

    class SafeDiv:
        def __enter__(self):
            return self

        def __exit__(self, exc_type, exc_val, exc_tb):
            if exc_type is ZeroDivisionError:
                print(f"    捕获异常: {exc_val}")
                return True  # 抑制异常
            return False

    with SafeDiv():
        result = 10 / 0
        print(f"    不会执行到这里")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("Python 上下文管理器演示")
    print("=" * 60)

    # 1. 类实现
    print("\n[1] 类实现上下文管理器:")
    with Timer("sleep"):
        time.sleep(0.1)

    # 2. contextlib
    print("\n[2] @contextmanager 装饰器:")
    with timer_cm("上下文"):
        time.sleep(0.05)

    with temp_directory() as tmpdir:
        test_file = os.path.join(tmpdir, "test.txt")
        with open(test_file, 'w') as f:
            f.write("Hello, Context!")
        print(f"    写入文件: {test_file}")

    # 3. ExitStack
    print("\n[3] ExitStack:")
    demo_exit_stack()

    # 4. 嵌套
    print("\n[4] 嵌套上下文:")
    nested_contexts()

    # 5. 异常处理
    print("\n[5] 异常处理:")
    exception_handling()
    print("    异常被抑制，继续执行")

    # 6. 数据库连接模拟
    print("\n[6] 数据库连接:")
    with DBConnection("localhost", 5432) as conn:
        conn.query("SELECT * FROM users")

    print("\n" + "=" * 60)
    print("=== PASS ===")


if __name__ == "__main__":
    main()
