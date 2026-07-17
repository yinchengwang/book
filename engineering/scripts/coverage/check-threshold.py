#!/usr/bin/env python3
"""
engineering/scripts/coverage/check-threshold.py
对 coverage-current.json 检查阈值：
- 核心模块（db-core, db-storage-engine, algo-prod）lines_pct ≥ 80%
- 其他模块 lines_pct ≥ 50%
- 失败 exit 1

用法：python3 check-threshold.py <coverage-current.json>
"""
import argparse
import io
import json
import sys
from pathlib import Path

if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")

CORE_MODULES = {"db-core", "db-storage-engine", "algo-prod", "vector_index"}
THRESHOLD_CORE = 80.0
THRESHOLD_OTHER = 50.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("current_json", type=Path, help="aggregate.py 输出的 coverage JSON")
    args = parser.parse_args()

    if not args.current_json.exists():
        print(f"ERR: {args.current_json} 不存在", file=sys.stderr)
        sys.exit(1)

    data = json.loads(args.current_json.read_text(encoding="utf-8"))
    modules = data.get("modules", {})

    rows = []
    failures = []

    for name, info in sorted(modules.items()):
        lines_pct = info.get("lines_pct", info.get("lines", 0.0))
        threshold = THRESHOLD_CORE if name in CORE_MODULES else THRESHOLD_OTHER
        status = "✅" if lines_pct >= threshold else "❌"
        if lines_pct < threshold:
            failures.append((name, lines_pct, threshold))
        rows.append((name, lines_pct, threshold, status))

    print("\n=== Coverage Threshold Check ===\n")
    print(f"| {'模块':<32} | {'当前':>6} | {'阈值':>6} | 状态 |")
    print("|" + "-" * 34 + "|" + "-" * 8 + "|" + "-" * 8 + "|--------|")
    for name, pct, thr, st in rows:
        print(f"| {name:<32} | {pct:>5.1f}% | {thr:>5.1f}% |  {st} |")

    if failures:
        print(f"\n❌ {len(failures)} 个模块未达到阈值：\n")
        for name, pct, thr in failures:
            print(f"  - {name:<32} {pct:>5.1f}% < {thr:.1f}%")
        sys.exit(1)
    print("\n✅ 全部模块达标。\n")


if __name__ == "__main__":
    main()
