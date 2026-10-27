#!/usr/bin/env python3
"""check_layering.py — 分层门禁：检查目录内不得 include 模态私有头。

用法:
    python scripts/check_layering.py <dir> [<dir> ...]

规则（violation 模式）：
    db/storage/<modality>/...   各模态存储引擎私有头
    db/index/...                索引实现（含 vector_index）
    db/sql/                     SQL 引擎内部
    db/graph/                   图引擎头（graph_sql_functions 等私有 API）
    db/vectorized/              向量化执行内部

公共层（core / storage 公共设施）只允许依赖更低的公共层；
模态私有 API 一律经 include/db/mm_engine_register.h 的注册入口反转。

只检查 .c/.h/.cpp 文件中的 #include 指令；只用标准库。
命中 violation 时退出码 1 并逐条列出，否则打印 OK。
"""

import os
import re
import sys

VIOLATION_RE = re.compile(
    r'^\s*#\s*include\s*[<"]'
    r'db/(?:'
    r'storage/(?:kv|vector|graph|doc|ts|spatial|yang|stream|columnar|st|rdf|sparse)/'
    r'|index/'
    r'|sql/'
    r'|graph/'
    r'|vectorized/'
    r')'
)

SCAN_SUFFIXES = ('.c', '.h', '.cpp', '.cc')


def scan_dir(root):
    violations = []
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            if not name.endswith(SCAN_SUFFIXES):
                continue
            path = os.path.join(dirpath, name)
            try:
                with open(path, 'r', encoding='utf-8', errors='replace') as f:
                    for lineno, line in enumerate(f, 1):
                        if VIOLATION_RE.match(line):
                            violations.append((path, lineno, line.strip()))
            except OSError:
                continue
    return violations


def main():
    if len(sys.argv) < 2:
        print('usage: check_layering.py <dir> [<dir> ...]', file=sys.stderr)
        return 2
    all_violations = []
    for d in sys.argv[1:]:
        if not os.path.isdir(d):
            print('error: not a directory: %s' % d, file=sys.stderr)
            return 2
        all_violations.extend(scan_dir(d))

    if all_violations:
        for path, lineno, line in all_violations:
            print('VIOLATION %s:%d: %s' % (path, lineno, line))
        print('FAILED: %d modality-private include(s) found' % len(all_violations))
        return 1
    print('OK: no modality-private includes in %s' % ', '.join(sys.argv[1:]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
