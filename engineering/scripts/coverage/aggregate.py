#!/usr/bin/env python3
"""
engineering/scripts/coverage/aggregate.py
按 engineering/src/<lib>/ 模块聚合覆盖率。

用法：
    python3 aggregate.py <coverage.info>
    输出 JSON：{ modules: { db-storage: {lines: 72.3, branches: 65.1, functions: 78.4}, ... } }
"""
import io
import json
import re
import sys
from pathlib import Path
from collections import defaultdict

# 强制 UTF-8 输出（Windows console 默认 GBK）
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")


def parse_lcov(info_path: Path) -> dict:
    """解析 lcov .info 文件，按工程模块聚合。

    模块划分规则：
        engineering/src/<lib>/**  →  <lib>
        engineering/src/<lib>/<sub>/**  →  <lib>（递归归入 lib）
    """
    modules = defaultdict(lambda: {"lines_hit": 0, "lines_found": 0, "branches_hit": 0, "branches_found": 0, "functions_hit": 0, "functions_found": 0})

    current_file = None
    current_module = None

    with info_path.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()

            if line.startswith("SF:"):
                # Source file
                current_file = line[3:].strip()
                # 提取模块名
                m = re.search(r"engineering/src/([^/]+)/", current_file)
                if m:
                    current_module = m.group(1)
                else:
                    current_module = None

            elif line == "end_of_record":
                current_file = None
                current_module = None

            elif current_module is not None:
                if line.startswith("DA:"):
                    # DA:<line>,<exec_count>
                    parts = line[3:].split(",")
                    if len(parts) == 2:
                        modules[current_module]["lines_found"] += 1
                        if int(parts[1]) > 0:
                            modules[current_module]["lines_hit"] += 1

                elif line.startswith("BRDA:"):
                    # BRDA:<line>,<block>,<branch>,<exec>
                    parts = line[5:].split(",")
                    if len(parts) == 4 and parts[3] != "-":
                        modules[current_module]["branches_found"] += 1
                        if int(parts[3]) > 0:
                            modules[current_module]["branches_hit"] += 1

                elif line.startswith("FN:"):
                    # FN:<line>,<name>
                    modules[current_module]["functions_found"] += 1

                elif line.startswith("FNDA:"):
                    # FNDA:<exec>,<name>
                    parts = line[5:].split(",")
                    if len(parts) >= 2:
                        if int(parts[0]) > 0:
                            modules[current_module]["functions_hit"] += 1

    # 计算百分比
    result = {"modules": {}}
    for mod, counts in sorted(modules.items()):
        result["modules"][mod] = {
            "lines": round(100.0 * counts["lines_hit"] / counts["lines_found"], 1) if counts["lines_found"] > 0 else 0.0,
            "branches": round(100.0 * counts["branches_hit"] / counts["branches_found"], 1) if counts["branches_found"] > 0 else 0.0,
            "functions": round(100.0 * counts["functions_hit"] / counts["functions_found"], 1) if counts["functions_found"] > 0 else 0.0,
            "lines_found": counts["lines_found"],
            "lines_hit": counts["lines_hit"],
        }

    # 加一个 total
    total_lines_hit = sum(c["lines_hit"] for c in modules.values())
    total_lines_found = sum(c["lines_found"] for c in modules.values())
    result["total"] = {
        "lines": round(100.0 * total_lines_hit / total_lines_found, 1) if total_lines_found > 0 else 0.0,
        "lines_found": total_lines_found,
        "lines_hit": total_lines_hit,
    }

    return result


def main():
    if len(sys.argv) != 2:
        print("用法: aggregate.py <coverage.info>", file=sys.stderr)
        sys.exit(1)

    info_path = Path(sys.argv[1])
    if not info_path.exists():
        print(f"错误: {info_path} 不存在", file=sys.stderr)
        sys.exit(1)

    result = parse_lcov(info_path)
    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()