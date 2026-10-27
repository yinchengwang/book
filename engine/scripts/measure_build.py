#!/usr/bin/env python3
"""measure_build.py — 统计一个 CMake/Ninja 构建树的产物规模。

后续所有「按模态独立构建」改造前后的数字对比都按本脚本口径：

- compile_units:
    优先解析 <build_dir>/build.ninja 中形如
        build <path>.o: C_COMPILER ... / CXX_COMPILER ...
    的编译行并计数（一个编译行 = 一个编译单元）。
    没有 build.ninja 时退回统计磁盘上 *.o / *.obj 文件数。
- static_libs / static_lib_bytes:
    递归统计 *.a 与 *.lib 的个数与总字节数。
- executables / executable_bytes:
    递归统计 *.exe；在非 Windows 上补充统计有可执行位且
    后缀不属于 .a/.so/.o/.lib 的普通文件。
- 只用 Python 标准库。

用法:
    python scripts/measure_build.py <build_dir>
输出:
    stdout 一个 JSON 对象。
"""

import json
import os
import re
import sys

NINJA_COMPILE_RE = re.compile(r"^build\s+[^:]+\.o(?:bj)?:\s*\S*COMPILER")

# 可执行文件判断时排除的库/对象后缀
LIB_SUFFIXES = (".a", ".lib", ".so", ".o", ".obj")


def count_compile_units_from_ninja(build_dir):
    """解析 build.ninja 统计编译单元数；文件不存在返回 None。"""
    ninja = os.path.join(build_dir, "build.ninja")
    if not os.path.isfile(ninja):
        return None
    count = 0
    with open(ninja, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            if NINJA_COMPILE_RE.match(line):
                count += 1
    return count


def count_objects_on_disk(build_dir):
    """退回口径：统计磁盘上的 *.o / *.obj。"""
    count = 0
    for root, _dirs, files in os.walk(build_dir):
        for name in files:
            if name.endswith((".o", ".obj")):
                count += 1
    return count


def collect_suffix_stats(build_dir, suffixes):
    """递归统计给定后缀集合的文件个数与总字节数。"""
    count = 0
    total = 0
    for root, _dirs, files in os.walk(build_dir):
        for name in files:
            if name.lower().endswith(suffixes):
                count += 1
                try:
                    total += os.path.getsize(os.path.join(root, name))
                except OSError:
                    pass
    return count, total


def collect_executables(build_dir):
    """统计可执行文件个数与总字节数。"""
    count = 0
    total = 0
    is_windows = os.name == "nt"
    for root, _dirs, files in os.walk(build_dir):
        for name in files:
            path = os.path.join(root, name)
            lower = name.lower()
            matched = False
            if is_windows:
                matched = lower.endswith(".exe")
            else:
                if lower.endswith(LIB_SUFFIXES):
                    matched = False
                else:
                    try:
                        matched = os.path.isfile(path) and os.access(path, os.X_OK)
                    except OSError:
                        matched = False
            if matched:
                count += 1
                try:
                    total += os.path.getsize(path)
                except OSError:
                    pass
    return count, total


def main():
    if len(sys.argv) != 2:
        print("usage: measure_build.py <build_dir>", file=sys.stderr)
        return 2
    build_dir = sys.argv[1]
    if not os.path.isdir(build_dir):
        print("error: not a directory: %s" % build_dir, file=sys.stderr)
        return 2

    compile_units = count_compile_units_from_ninja(build_dir)
    source = "build.ninja"
    if compile_units is None:
        compile_units = count_objects_on_disk(build_dir)
        source = "disk-objects-fallback"

    lib_count, lib_bytes = collect_suffix_stats(build_dir, (".a", ".lib"))
    exe_count, exe_bytes = collect_executables(build_dir)

    result = {
        "build_dir": os.path.abspath(build_dir),
        "compile_units_source": source,
        "compile_units": compile_units,
        "static_libs": lib_count,
        "static_lib_bytes": lib_bytes,
        "executables": exe_count,
        "executable_bytes": exe_bytes,
    }
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
