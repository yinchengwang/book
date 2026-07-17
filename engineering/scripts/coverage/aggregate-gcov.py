#!/usr/bin/env python3
r"""
engineering/scripts/coverage/aggregate-gcov.py
聚合 gcov 输出，按 engineering/src/<lib>/ 模块聚合。

S24 用法：
    cd engineering
    cmake -B build-cov -S . -DENABLE_COVERAGE=ON
    cmake --build build-cov --parallel 4
    cd build-cov && ctest --output-on-failure -j4
    find . -name "*.gcda" | xargs gcov  # 生成 .gcov
    cd ../ && mkdir -p coverage-gcov
    find build-cov -name "*.gcov" -exec mv {} coverage-gcov/ \;
    python3 scripts/coverage/aggregate-gcov.py --gcov-dir coverage-gcov \\
        --output docs/coverage-baseline.json

输出：{ modules: { <lib>: {lines_pct, lines_found, lines_hit, branches_pct, ...} } }
"""
import argparse
import datetime
import io
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")

REPO = Path(__file__).resolve().parents[3]


def parse_gcov_file(path: Path):
    """解析单个 .gcov 文件，返回 (lines_total, lines_hit, branches_total, branches_hit, source_path)。"""
    lines_total = 0
    lines_hit = 0
    branches_total = 0
    branches_hit = 0
    source_path = ""

    with open(path, "r", encoding="utf-8", errors="ignore") as fp:
        for raw in fp:
            raw = raw.rstrip("\n")
            # "        -:    0:Source:..."（带前导空格）
            if "Source:" in raw:
                source_path = raw.split("Source:", 1)[1].strip()
                continue
            if not raw:
                continue
            stripped = raw.lstrip()
            tag = stripped[:4] if len(stripped) >= 4 else stripped
            if tag == "====":
                lines_total += 1
                lines_hit += 1
            elif stripped.startswith("####") and len(stripped) > 4 and stripped[4] in (":", " ", ":"):
                # #####  /  #####:   /  #####   都算未执行
                lines_total += 1
            elif raw.startswith("-") or raw.startswith(":"):
                continue  # 非可执行 / 表头
            elif stripped.startswith("branch"):
                m = re.search(r"taken (\d+)%", raw)
                if m:
                    branches_total += 1
                    if int(m.group(1)) > 0:
                        branches_hit += 1

    return lines_total, lines_hit, branches_total, branches_hit, source_path


def module_for(source_path: str) -> str | None:
    """从 source path 提取工程层一级目录（如 algo-prod / db/core / apps/common）。"""
    if not source_path:
        return None
    sp = Path(source_path.replace("\\", "/"))
    try:
        rel = sp.relative_to(REPO)
    except ValueError:
        return None
    parts = rel.parts
    if "engineering" not in parts:
        return None
    i = parts.index("engineering")
    # skip "engineering" and "src" or "apps" or "include" 等
    # 目标是首个真正工程子目录（不含 "src", "apps", "test", "include", "cmake"）
    skip = {"src", "apps", "test", "include", "cmake", "scripts", "docs"}
    for j in range(i + 1, len(parts)):
        if parts[j] not in skip:
            return parts[j]
    return None


def aggregate(gcov_dir: Path) -> dict:
    modules = defaultdict(lambda: {
        "lines_total": 0,
        "lines_hit": 0,
        "branches_total": 0,
        "branches_hit": 0,
        "files": 0,
    })

    if not gcov_dir.exists():
        print(f"WARN: {gcov_dir} 不存在", file=sys.stderr)
        return {}

    for gcov_file in gcov_dir.rglob("*.gcov"):
        lt, lh, bt, bh, source = parse_gcov_file(gcov_file)
        if lt == 0 and bt == 0:
            continue
        m = module_for(source)
        if m is None:
            continue
        mod = modules[m]
        mod["lines_total"] += lt
        mod["lines_hit"] += lh
        mod["branches_total"] += bt
        mod["branches_hit"] += bh
        mod["files"] += 1

    result = {}
    for name, m in modules.items():
        result[name] = {
            "lines_pct": round(m["lines_hit"] / m["lines_total"] * 100, 2) if m["lines_total"] else 0.0,
            "branches_pct": round(m["branches_hit"] / m["branches_total"] * 100, 2) if m["branches_total"] else 0.0,
            "lines_found": m["lines_total"],
            "lines_hit": m["lines_hit"],
            "branches_found": m["branches_total"],
            "branches_hit": m["branches_hit"],
            "files": m["files"],
        }
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gcov-dir", type=Path, default=Path("coverage-gcov"))
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    modules = aggregate(args.gcov_dir)

    out = {
        "generated_at": datetime.datetime.now().isoformat(),
        "track": "engineering",
        "modules": modules,
        "_note": "S24 首次真实 baseline（gcov-only 模式，无 lcov）",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(text + "\n", encoding="utf-8")
        print(f"✓ 写入 {args.output}")
    else:
        sys.stdout.write(text + "\n")


if __name__ == "__main__":
    main()
