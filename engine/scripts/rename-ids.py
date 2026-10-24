#!/usr/bin/env python3
"""
全链路 ID 重命名脚本
- 去掉所有前缀（c-/cpp-/db-/linux-/vdb-/py-/ds-/grok-）
- 中划线改为下划线
- 处理：items-registry.js, statuses.json, r*-progress.md, learn-deep-bundle.js, learn.html
"""
import os
import re
import json
import shutil
from pathlib import Path

ROOT = Path("c:/code/book")
RADAR_DIR = ROOT / "engineering" / "apps" / "web" / "reading-radar"

PREFIX_TO_STACK = {
    "c-": "c", "cpp-": "cpp", "db-": "db", "linux-": "linux",
    "vdb-": "vdb", "py-": "py", "ds-": "ds", "grok-": "grokking",
}

def make_new_id(old_id: str) -> str:
    for prefix in PREFIX_TO_STACK:
        if old_id.startswith(prefix):
            base = old_id[len(prefix):]
            return base.replace("-", "_")
    if "-" in old_id:
        return old_id.replace("-", "_")
    return old_id

def rename_file_content(filepath: Path, replacements: dict, dry_run=False):
    """对单个文件执行替换，返回改动行数"""
    try:
        content = filepath.read_text(encoding="utf-8")
    except Exception as e:
        print(f"    [SKIP] {filepath}: {e}")
        return 0

    original = content
    for old_id, new_id in replacements.items():
        content = content.replace(old_id, new_id)

    if content == original:
        return 0

    if not dry_run:
        filepath.write_text(content, encoding="utf-8")
        # 计算改动行数（粗略）
        diff_lines = sum(1 for old_id in replacements if old_id in original)
        print(f"    [MODIFIED] {filepath.relative_to(ROOT)}")
    else:
        print(f"    [DRY] would modify: {filepath.relative_to(ROOT)}")
    return 1

def main():
    # 步骤1：从 items-registry.js 提取所有旧 ID
    registry_path = RADAR_DIR / "data" / "app" / "items-registry.js"
    content = registry_path.read_text(encoding="utf-8")

    # 匹配 "xxx-yyy":  { ... }  格式
    old_ids = re.findall(r'"([a-z][a-z0-9_-]+)":\s*\{', content)
    print(f"从 items-registry.js 提取了 {len(old_ids)} 个旧 ID")

    # 步骤2：构建替换表
    replacements = {}
    for old_id in old_ids:
        new_id = make_new_id(old_id)
        if old_id != new_id:
            replacements[old_id] = new_id

    print(f"需要重命名的 ID: {len(replacements)} 个")
    for old, new in list(replacements.items())[:10]:
        print(f"  {old} -> {new}")
    print("  ...")

    # 步骤3：验证 statuses.json 的 key 与 items-registry 一致
    statuses_path = RADAR_DIR / "data" / "statuses.json"
    statuses = json.loads(statuses_path.read_text(encoding="utf-8"))
    statuses_keys = set(statuses.keys())

    registry_keys = set(old_ids)
    missing = registry_keys - statuses_keys
    extra = statuses_keys - registry_keys
    print(f"\nstatuses.json 有 {len(statuses_keys)} 个 key")
    if missing:
        print(f"  statuses 缺少: {missing}")
    if extra:
        print(f"  statuses 多余: {extra}")

    # 步骤4：执行重命名
    print("\n=== 执行重命名 ===")
    modified_files = 0

    # 4a. items-registry.js
    modified_files += rename_file_content(registry_path, replacements)

    # 4b. statuses.json
    modified_files += rename_file_content(statuses_path, replacements)

    # 4c. r*-progress.md
    for pm in (RADAR_DIR / "data").glob("r*-progress.md"):
        modified_files += rename_file_content(pm, replacements)

    # 4d. 前端 JS/HTML 文件
    for pattern in ["**/*.js", "**/*.html"]:
        for f in RADAR_DIR.glob(pattern):
            if f.is_file() and f.name not in ("rename-ids.py",):
                modified_files += rename_file_content(f, replacements)

    print(f"\n=== 完成：共修改 {modified_files} 个文件 ===")

    # 步骤5：验证（grep 旧格式应该无结果）
    print("\n=== 验证：搜索旧格式 ID ===")
    old_patterns = [re.escape(k) for k in list(replacements.keys())[:5]]
    # 用 grep 检查是否还有遗留
    for old_id in list(replacements.keys())[:5]:
        for path in [registry_path, statuses_path]:
            c = path.read_text(encoding="utf-8")
            if old_id in c:
                print(f"  [WARN] {old_id} 仍在 {path.name}")

    print("\n验证完成。如需回退，运行: git checkout -- .")

if __name__ == "__main__":
    main()
