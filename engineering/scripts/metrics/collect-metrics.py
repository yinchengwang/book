#!/usr/bin/env python3
"""
engineering/scripts/metrics/collect-metrics.py
收集双轨指标：ctest 数、LOC、状态。输出 JSON。

S20 用法：
    python3 collect-metrics.py > ../docs/project-metrics.json

约定：所有路径相对仓库根
"""
import json
import re
import subprocess
import sys
import os
from pathlib import Path

# 强制 UTF-8 输出
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")
else:
    import io  # noqa: F401

REPO = Path(__file__).resolve().parents[3]


def count_tests(build_dir: Path, label: str) -> int:
    """统计 ctest 数（*test/ctest/Temporary/CTestCostData.txt 不存在时返回 0）。"""
    if not build_dir.exists():
        return 0
    test_files = list(build_dir.rglob("[Tt]est*"))
    # 用 ctest -N 看 test 列表（dry-run，不运行测试）
    try:
        out = subprocess.run(
            ["ctest", "-N", "--test-dir", str(build_dir)],
            capture_output=True, text=True, timeout=60,
        ).stdout
        return len(re.findall(r"^\s*Test\s*#", out, re.MULTILINE))
    except Exception:
        return 0


def count_loc(path: Path, ext: str) -> int:
    """统计某扩展名的代码行数（含空行、注释）。"""
    if not path.exists():
        return 0
    total = 0
    for f in path.rglob(f"*.{ext}"):
        if "build" in str(f) or "node_modules" in str(f):
            continue
        try:
            with open(f, "r", encoding="utf-8", errors="ignore") as fp:
                total += sum(1 for _ in fp)
        except Exception:
            pass
    return total


def collect() -> dict:
    eng_root = REPO / "engineering"
    learn_root = REPO / "learning"

    metrics = {
        "generated_at": __import__("datetime").datetime.now().isoformat(),
        "track": "dual",
        "engineering": {
            "ctest_total": count_tests(eng_root / "build", "engineering"),
            "lines_c": count_loc(eng_root / "src", "c"),
            "lines_cpp": count_loc(eng_root / "src", "cpp"),
            "lines_h": count_loc(eng_root / "include", "h"),
        },
        "learning": {
            "ctest_total": count_tests(REPO / "build-learning", "learning"),
            "lines_c": count_loc(learn_root / "ds-c/orig", "c"),
            "notes_markdown": count_loc(learn_root / "notes", "md"),
        },
    }
    return metrics


if __name__ == "__main__":
    json.dump(collect(), sys.stdout, ensure_ascii=False, indent=2)
    sys.stdout.write("\n")
