#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
# ffi 演示脚本
# 演示 ctypes + cffi 与 C 代码交互
# =============================================================================

import sys
import os
from pathlib import Path
from ctypes import (
    CDLL, c_int, c_double, c_char_p, c_void_p,
    Structure, POINTER, c_size_t, byref, create_string_buffer
)

# =============================================================================
# 1. ctypes 基础
# =============================================================================
def demo_ctypes_basic():
    """演示 ctypes 基础：加载 libc 并调用标准函数"""
    print("\n" + "=" * 60)
    print("1. ctypes 基础 - 调用 libc 函数")
    print("=" * 60)

    # 加载 C 标准库
    if sys.platform == 'darwin':
        libc = CDLL('libc.dylib')
    elif sys.platform == 'linux':
        libc = CDLL('libc.so.6')
    elif sys.platform == 'win32':
        from ctypes import windll
        libc = CDLL('msvcrt.dll')
    else:
        print("不支持的平台")
        return

    # 1.1 atoi（字符串转整数）
    libc.atoi.restype = c_int
    result = libc.atoi(b'42')
    print(f"libc.atoi('42') = {result}")

    # 1.2 字符串函数
    result = libc.strlen(b'Hello, Ctypes!')
    print(f"libc.strlen('Hello, Ctypes!') = {result}")

    log_ok("ctypes 基础演示完成")

# =============================================================================
# 2. ctypes 自定义函数
# =============================================================================
def demo_ctypes_custom():
    """演示调用自定义 C 函数"""
    print("\n" + "=" * 60)
    print("2. ctypes 自定义函数调用")
    print("=" * 60)

    DEMO_DIR = Path('/tmp/python_ffi_demo')
    DEMO_DIR.mkdir(exist_ok=True)

    # 创建简单的 C 代码
    c_code = """
#include <math.h>

double add(double a, double b) {
    return a + b;
}

double multiply_array(double* arr, int n) {
    double result = 1.0;
    for (int i = 0; i < n; i++) {
        result *= arr[i];
    }
    return result;
}

void reverse_string(char* str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char tmp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = tmp;
    }
}
"""

    c_file = DEMO_DIR / 'helper.c'
    so_file = DEMO_DIR / 'helper.so'
    c_file.write_text(c_code)

    # 编译为 so
    compile_cmd = f"cd {DEMO_DIR} && gcc -shared -fPIC -o helper.so helper.c -lm"
    ret = os.system(compile_cmd)
    if ret != 0:
        print("⚠ 编译失败（需要 gcc），跳过 ctypes 自定义函数调用")
        return

    # 加载共享库
    lib = CDLL(str(so_file))

    # 2.1 简单运算
    lib.add.restype = c_double
    lib.add.argtypes = [c_double, c_double]
    result = lib.add(3.14, 2.86)
    print(f"add(3.14, 2.86) = {result:.2f}")

    # 2.2 数组操作
    lib.multiply_array.restype = c_double
    lib.multiply_array.argtypes = [POINTER(c_double), c_int]

    arr = (c_double * 5)(1.0, 2.0, 3.0, 4.0, 5.0)
    result = lib.multiply_array(arr, 5)
    print(f"multiply_array([1,2,3,4,5]) = {result:.0f}")

    # 2.3 字符串操作
    lib.reverse_string.argtypes = [c_char_p]
    buf = create_string_buffer(b"hello world")
    lib.reverse_string(buf)
    print(f"reverse_string('hello world') = '{buf.value.decode()}'")

    log_ok("ctypes 自定义函数调用完成")

# =============================================================================
# 3. cffi 演示
# =============================================================================
def demo_cffi():
    """演示 cffi 库使用"""
    print("\n" + "=" * 60)
    print("3. cffi 演示")
    print("=" * 60)

    try:
        import cffi
    except ImportError:
        print("⚠ cffi 未安装，跳过演示")
        print("  安装: pip install cffi")
        return

    ffibuilder = cffi.FFI()
    ffibuilder.cdef("""
        double add(double, double);
        int strlen(const char*);
    """)

    # 打开 libc 并调用
    if sys.platform == 'linux':
        lib = ffibuilder.dlopen('libc.so.6')
    else:
        print("⚠ cffi libc 演示仅支持 Linux")
        return

    result = lib.add(2.5, 3.7)
    print(f"cffi add(2.5, 3.7) = {result:.1f}")

    length = lib.strlen(b"Hello cffi!")
    print(f"cffi strlen('Hello cffi!') = {length}")

    log_ok("cffi 演示完成")

# =============================================================================
# 4. 性能对比
# =============================================================================
def demo_performance():
    """演示 Python vs C 函数性能对比"""
    print("\n" + "=" * 60)
    print("4. ctypes 性能对比")
    print("=" * 60)

    from time import time

    DEMO_DIR = Path('/tmp/python_ffi_demo')
    so_file = DEMO_DIR / 'helper.so'

    if not so_file.exists():
        print("⚠ so 未找到，跳过性能对比")
        return

    lib = CDLL(str(so_file))
    lib.multiply_array.restype = c_double
    lib.multiply_array.argtypes = [POINTER(c_double), c_int]

    n = 100000
    arr_py = [float(i) for i in range(1, 101)]
    arr_c = (c_double * len(arr_py))(*arr_py)

    # Python 版本
    def py_multiply(data):
        result = 1.0
        for x in data:
            result *= x
        return result

    start = time()
    for _ in range(1000):
        py_multiply(arr_py)
    py_time = time() - start

    # C 版本（通过 ctypes）
    start = time()
    for _ in range(1000):
        lib.multiply_array(arr_c, len(arr_py))
    c_time = time() - start

    print(f"Python: {py_time:.4f}s")
    print(f"C (ctypes): {c_time:.4f}s")

    log_ok("性能对比完成")

# =============================================================================
# 辅助函数
# =============================================================================
def log_ok(msg):
    print(f"✓ {msg}")

# =============================================================================
# 主函数
# =============================================================================
def main():
    print("\n" + "╔" + "=" * 58 + "╗")
    print("║" + " " * 16 + "FFI 演示（ctypes + cffi）" + " " * 18 + "║")
    print("║" + " " * 12 + "Python 与 C 语言互操作" + " " * 12 + "║")
    print("╚" + "=" * 58 + "╝")

    demo_ctypes_basic()
    demo_ctypes_custom()
    demo_cffi()
    demo_performance()

    print("\n" + "=" * 60)
    print("演示完成！关键知识点：")
    print("=" * 60)
    print("• from ctypes import CDLL   # 加载共享库")
    print("• lib.func.restype = c_int  # 指定返回类型")
    print("• lib.func.argtypes = [...] # 指定参数类型")
    print("• create_string_buffer()    # 可变字符串缓冲区")
    print("• cffi：更现代、更安全的 FFI 方案")
    print("=" * 60)

if __name__ == '__main__':
    main()
