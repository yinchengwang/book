#!/usr/bin/env python3
"""
test-summary.py —— P10: 工程层测试矩阵摘要报告

从 ctest XML 输出（或直接调用 test 可执行文件）生成模块级摘要。
用法：
  python3 test-summary.py --xml build/Testing/    # 从 ctest XML 读取
  python3 test-summary.py --scan test/              # 直接扫描并运行测试
  python3 test-summary.py --ci                      # CI 兼容模式（无 XML 时）
"""

import os, sys, json, subprocess, argparse, glob
from pathlib import Path
from collections import defaultdict

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
TEST_DIR = REPO_ROOT / "test"

def discover_test_exes(test_dir: Path) -> list[Path]:
    """找到所有 gtest 可执行文件"""
    exes = []
    for exe_path in sorted(test_dir.rglob("*.exe")):
        if exe_path.suffix == ".exe":
            exes.append(exe_path)
    return exes

def run_test_exe(exe_path: Path) -> dict:
    """运行单个 gtest 可执行文件，返回结果字典"""
    try:
        result = subprocess.run(
            [str(exe_path), "--gtest_list_tests"],
            capture_output=True, encoding="utf-8", errors="replace", timeout=30
        )
        if result.returncode != 0:
            return {"exe": str(exe_path.name), "tests": 0, "passed": 0, "failed": 1, "error": result.stderr[:200]}

        # Parse test list
        tests = []
        suite = ""
        for line in result.stdout.strip().split("\n"):
            if not line:
                continue
            if line.endswith("."):
                suite = line.strip(".")
            elif line.startswith("  "):
                test_name = line.strip()
                tests.append(f"{suite}.{test_name}")

        # Now run all tests
        result = subprocess.run(
            [str(exe_path)],
            capture_output=True, encoding="utf-8", errors="replace", timeout=60
        )
        passed = result.stdout.count("[  PASSED  ]")
        failed = result.stdout.count("[  FAILED  ]")
        ran = passed + failed

        module = exe_path.parent.name if exe_path.parent.name != "test" else exe_path.stem

        return {
            "module": module,
            "exe": str(exe_path.name),
            "tests": ran,
            "passed": passed,
            "failed": failed,
            "ok": result.returncode == 0 and failed == 0,
        }
    except subprocess.TimeoutExpired:
        return {"module": "", "exe": str(exe_path.name), "tests": 0, "passed": 0, "failed": 1, "error": "timeout"}

def ci_mode() -> dict:
    """CI 模式：尝试 ctest 获取结果"""
    result = subprocess.run(
        ["ctest", "--output-on-failure", "-j4"],
        cwd=REPO_ROOT / "build", capture_output=True, encoding="utf-8", errors="replace", timeout=120
    )
    # 聚合结果
    summary = {
        "mode": "ctest",
        "returncode": result.returncode,
        "output": result.stdout[-500:] if result.stdout else "",
        "total_tests": sum(1 for line in (result.stdout or "").split("\n") if "Test #" in line),
    }
    return summary

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--xml", type=str, help="ctest XML 输出目录")
    parser.add_argument("--scan", type=str, help="测试目录")
    parser.add_argument("--ci", action="store_true", help="CI 兼容模式")
    parser.add_argument("--json", type=str, default="test-summary.json", help="输出 JSON 路径")
    parser.add_argument("--list", action="store_true", help="仅列出测试")
    args = parser.parse_args()

    if args.ci:
        summary = ci_mode()
        print(json.dumps(summary, indent=2))
        Path(args.json).write_text(json.dumps(summary, indent=2))
        return

    if args.scan:
        test_dir = Path(args.scan)
    else:
        test_dir = TEST_DIR

    exes = discover_test_exes(test_dir)
    if args.list:
        for exe in exes:
            print(f"  {exe.name} → {exe.parent}")
        print(f"\n共 {len(exes)} 个测试可执行文件")
        return

    results = []
    modules = defaultdict(lambda: {"total": 0, "passed": 0, "failed": 0, "exes": []})

    for exe in exes:
        r = run_test_exe(exe)
        results.append(r)
        mod = r.get("module", "unknown")
        modules[mod]["total"] += r["tests"]
        modules[mod]["passed"] += r["passed"]
        modules[mod]["failed"] += r["failed"]
        modules[mod]["exes"].append(r["exe"])

    total = sum(m["total"] for m in modules.values())
    total_failed = sum(m["failed"] for m in modules.values())

    summary = {
        "generated_at": "",
        "modules": {k: dict(v) for k, v in modules.items()},
        "total_tests": total,
        "total_failed": total_failed,
        "all_pass": total_failed == 0,
    }

    # 打印摘要
    print(f"\n{'='*60}")
    print(f"  工程层测试摘要（{len(exes)} 个可执行文件）")
    print(f"{'='*60}")
    for mod_name in sorted(modules.keys()):
        m = modules[mod_name]
        status = "PASS" if m["failed"] == 0 else "FAIL"
        print(f"  {status} {mod_name:25s}  {m['passed']:4d}/{m['total']:4d} pass  ({','.join(m['exes'][:3])})")
    print(f"{'='*60}")
    print(f"  总计: {total - total_failed}/{total} passed")
    if total_failed > 0:
        print(f"  FAIL {total_failed} tests FAILED")
    else:
        print(f"  PASS ALL PASS")
    print(f"{'='*60}")

    # 保存 JSON
    from datetime import datetime, timezone
    summary["generated_at"] = datetime.now(timezone.utc).isoformat()
    Path(args.json).write_text(json.dumps(summary, indent=2, ensure_ascii=False))
    print(f"\n报告已保存: {args.json}")

if __name__ == "__main__":
    main()
