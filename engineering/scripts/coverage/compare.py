#!/usr/bin/env python3
"""
engineering/scripts/coverage/compare.py
对比当前 coverage 与 baseline，输出 ⚠️/🚨 标记。

用法：
    python3 compare.py <current.json> <baseline.json>
    输出 markdown 表格，可直接用作 PR comment。
"""
import io
import json
import sys
from pathlib import Path

# 强制 UTF-8 输出（Windows console 默认 GBK）
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")


THRESHOLD_WARN = 2.0    # 下降 > 2% → ⚠️
THRESHOLD_ALERT = 5.0   # 下降 > 5% → 🚨


def load_json(path: Path) -> dict:
    if not path.exists():
        return {"modules": {}, "total": {}}
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def diff_modules(current: dict, baseline: dict) -> list:
    rows = []
    all_modules = sorted(set(current.get("modules", {}).keys()) | set(baseline.get("modules", {}).keys()))

    for mod in all_modules:
        cur = current.get("modules", {}).get(mod, {})
        base = baseline.get("modules", {}).get(mod, {})

        cur_lines = cur.get("lines", 0.0)
        base_lines = base.get("lines", 0.0)
        delta = cur_lines - base_lines

        if delta < -THRESHOLD_ALERT:
            marker = "🚨"
        elif delta < -THRESHOLD_WARN:
            marker = "⚠️"
        elif delta > 0:
            marker = "✅"
        else:
            marker = "  "

        rows.append({
            "module": mod,
            "current": cur_lines,
            "baseline": base_lines,
            "delta": round(delta, 1),
            "marker": marker,
        })

    return rows


def main():
    if len(sys.argv) != 3:
        print("用法: compare.py <current.json> <baseline.json>", file=sys.stderr)
        sys.exit(1)

    current = load_json(Path(sys.argv[1]))
    baseline = load_json(Path(sys.argv[2]))

    rows = diff_modules(current, baseline)

    # 输出 markdown 表格
    print("## Coverage Report")
    print()
    print("| Module | Lines | Baseline | Δ |")
    print("|--------|-------|----------|---|")
    for r in rows:
        print(f"| `{r['module']}` | {r['current']}% | {r['baseline']}% | {r['delta']:+.1f}% {r['marker']} |")

    # 输出 total
    cur_total = current.get("total", {}).get("lines", 0.0)
    base_total = baseline.get("total", {}).get("lines", 0.0)
    delta_total = cur_total - base_total
    print(f"| **TOTAL** | **{cur_total}%** | {base_total}% | {delta_total:+.1f}% |")
    print()
    print(f"_阈值: 下降 > {THRESHOLD_WARN}% ⚠️, 下降 > {THRESHOLD_ALERT}% 🚨_")

    # 退出码：仅 🚨 触发非零退出
    if any(r["marker"] == "🚨" for r in rows):
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()