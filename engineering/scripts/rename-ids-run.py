#!/usr/bin/env python3
"""
全链路 ID 重命名脚本
- 规则：去掉前缀，中划线改下划线，冲突加栈后缀
- 特殊处理：grok-deadlock 有两个条目（OS版+MySQL版），通过 product 字段分发到不同新 ID
- 处理：items-registry.js, statuses.json, r*-progress.md, 所有前端 JS/HTML
"""
import re
import json
from pathlib import Path

ROOT = Path("c:/code/book")
RADAR_DIR = ROOT / "engineering" / "apps" / "web" / "reading-radar"

PREFIX_TO_STACK = {
    "c-": "c", "cpp-": "cpp", "db-": "db", "linux-": "linux",
    "vdb-": "vdb", "py-": "py", "ds-": "ds", "grok-": "grokking",
}

# ============================================================
# 手工审核过的冲突映射表
# ============================================================
MANUAL_RENAME = {
    "c-control-flow":    "control_flow",
    "py-control-flow":   "control_flow_py",
    "c-stdlib":          "stdlib",
    "py-stdlib":         "stdlib_py",
    "c-sort-basic":      "sort_basic",
    "ds-sort-basic":     "sort_basic_ds",
    "c-sort-advanced":   "sort_advanced",
    "ds-sort-advanced":  "sort_advanced_ds",
    "c-dp":              "dp",
    "ds-dp":             "dp_ds",
    "c-gdb":             "gdb",
    "cpp-gdb":           "gdb_cpp",
    "c-git":             "git",
    "cpp-git":           "git_cpp",
    "py-git":            "git_py",
    "c-profiling":       "profiling",
    "cpp-profiling":     "profiling_cpp",
    "py-profiling":      "profiling_py",
    "c-ci":              "ci",
    "py-ci":             "ci_py",
    "cpp-lockfree":      "lockfree",
    "ds-lockfree":       "lockfree_ds",
    "cpp-docker":        "docker",
    "py-docker":         "docker_py",
    "db-row-format":     "row_format",
    "grok-row_format":   "row_format_grokking",
    "db-buffer-pool":    "buffer_pool",
    "grok-buffer_pool":  "buffer_pool_grokking",
    "db-deadlock":       "deadlock",
    "linux-pagecache":   "pagecache",
    "grok-pagecache":    "pagecache_grokking",
    "linux-zero-copy":   "zero_copy",
    "grok-zero_copy":    "zero_copy_grokking",
    "c-string-match":    "string_match",
    "ds-string-match":   "string_match_ds",
}

def auto_new_id(old_id: str) -> str:
    for prefix in PREFIX_TO_STACK:
        if old_id.startswith(prefix):
            return old_id[len(prefix):].replace("-", "_")
    if "-" in old_id:
        return old_id.replace("-", "_")
    return old_id

def safe_read(path: Path) -> str:
    for enc in ["utf-8", "gbk", "latin-1"]:
        try:
            return path.read_text(encoding=enc)
        except (UnicodeDecodeError, LookupError):
            continue
    return path.read_text(encoding="utf-8", errors="replace")

def rename_file(filepath: Path, replacements: dict) -> bool:
    try:
        content = safe_read(filepath)
    except Exception as e:
        print(f"    [SKIP] {filepath.name}: {e}")
        return False

    original = content
    for old_id, new_id in replacements.items():
        content = content.replace(old_id, new_id)

    if content == original:
        return False

    try:
        filepath.write_text(content, encoding="utf-8")
        hits = sum(1 for k in replacements if k in original)
        print(f"    [OK] {filepath.relative_to(ROOT)} ({hits} 处)")
    except Exception as e:
        print(f"    [FAIL] {filepath.name}: {e}")
        return False
    return True

def parse_registry_entries(content: str) -> list:
    """
    逐行解析 items-registry.js，收集每个条目的 (old_id, entry_lines, product)。
    返回 list of (old_id, entry_text, product)。
    正确处理重复 key（如 grok-deadlock 有两个）。
    """
    entries = []
    lines = content.split('\n')
    i = 0
    while i < len(lines):
        m = re.search(r'"([a-z][a-z0-9_-]+)":\s*\{', lines[i])
        if m:
            old_id = m.group(1)
            # 收集这个条目（多行 block）
            entry_lines = [lines[i]]
            brace_count = lines[i].count('{') - lines[i].count('}')
            j = i + 1
            while j < len(lines) and brace_count > 0:
                entry_lines.append(lines[j])
                brace_count += lines[j].count('{') - lines[j].count('}')
                j += 1
            entry_text = '\n'.join(entry_lines)
            # 提取 product 字段
            prod_m = re.search(r'product:"([^"]+)"', entry_text)
            product = prod_m.group(1) if prod_m else "unknown"
            entries.append((old_id, entry_text, product))
            i = j
        else:
            i += 1
    return entries

def rewrite_registry(registry_path: Path, replacements: dict):
    """重写 items-registry.js：先替换普通 key，再处理 grok-deadlock 重复"""
    content = safe_read(registry_path)
    original = content

    # 步骤 1：普通替换（处理 db-deadlock -> deadlock）
    for old_id, new_id in replacements.items():
        if old_id != "grok-deadlock":   # grok-deadlock 特殊处理
            content = re.sub(
                rf'"({re.escape(old_id)})":\s*\{{',
                f'"{new_id}": {{',
                content
            )

    # 步骤 2：grok-deadlock 有两个条目，按 product 分发
    # 收集 grok-deadlock 的两个 entry text
    entries = parse_registry_entries(original)
    deadlock_entries = [(oid, et, prod) for oid, et, prod in entries if oid == "grok-deadlock"]

    if len(deadlock_entries) == 2:
        # 第一个（product=os） -> deadlock_grokking_os
        # 第二个（product=mysql） -> deadlock_grokking_mysql
        for idx, (oid, entry_text, product) in enumerate(deadlock_entries):
            new_id = "deadlock_grokking_os" if product == "os" else "deadlock_grokking_mysql"
            # 替换这个 entry 的 key（精确替换，不影响其他内容）
            # entry_text 形如:   "grok-deadlock": { product:"os", ...
            old_key_line = re.search(r'"grok-deadlock":\s*\{', entry_text)
            if old_key_line:
                new_key_line = f'"{new_id}": {{'
                content = content.replace(
                    old_key_line.group(0),
                    new_key_line,
                    1  # 只替换第一个匹配
                )
        print(f"    [INFO] grok-deadlock 重复 key 已按 product 分发（OS->os, MySQL->mysql）")
    elif len(deadlock_entries) == 1:
        content = re.sub(
            r'"grok-deadlock":\s*\{',
            '"deadlock_grokking": {',
            content
        )

    if content == original:
        return False

    registry_path.write_text(content, encoding="utf-8")
    print(f"    [OK] {registry_path.relative_to(ROOT)} (items-registry 重写)")
    return True

def main():
    print("=== 全链路 ID 重命名脚本 ===\n")

    # 1. 从 items-registry.js 提取所有旧 ID（逐行解析，保留重复 key）
    registry_path = RADAR_DIR / "data" / "app" / "items-registry.js"
    content = safe_read(registry_path)
    entries = parse_registry_entries(content)
    all_old_ids = [oid for oid, _, _ in entries]
    print(f"[1] 从 items-registry.js 提取了 {len(all_old_ids)} 个条目")

    # 2. 构建替换表（排除 grok-deadlock，特殊处理）
    replacements = {}
    for old_id in all_old_ids:
        if old_id == "grok-deadlock":
            continue   # 由 rewrite_registry 按 product 分发
        if old_id in MANUAL_RENAME:
            replacements[old_id] = MANUAL_RENAME[old_id]
        else:
            replacements[old_id] = auto_new_id(old_id)

    # 检查普通替换表的冲突
    from collections import Counter
    new_id_counts = Counter(replacements.values())
    remaining = {nid: cnt for nid, cnt in new_id_counts.items() if cnt > 1}
    if remaining:
        print(f"\n[WARN] 仍有 {len(remaining)} 个新 ID 冲突:")
        for nid, cnt in remaining.items():
            sources = [old for old, new in replacements.items() if new == nid]
            print(f"  {nid}: <- {sources}")
        return

    print(f"[2] 替换表：{len(replacements)} 个映射 + grok-deadlock(product感知)")

    # 3. 验证 statuses.json
    statuses_path = RADAR_DIR / "data" / "statuses.json"
    statuses = json.loads(safe_read(statuses_path))
    missing = set(all_old_ids) - set(statuses.keys())
    extra = set(statuses.keys()) - set(all_old_ids)
    print(f"[3] statuses.json: {len(statuses)} keys | 缺口: {len(missing)} | 多余: {len(extra)}")

    # 3b. statuses.json 中 grok-deadlock 也是重复 key，需特殊处理
    statuses_raw = safe_read(statuses_path)
    s_entries = re.findall(r'"grok-deadlock":\s*"([^"]+)"', statuses_raw)
    if len(s_entries) == 2:
        print(f"    [INFO] statuses.json 中 grok-deadlock 有 {len(s_entries)} 个条目（重复 key），将合并")
        # statuses.json 的 grok-deadlock 两个值应该分别是 "todo"/"done"，合并到 deadlock_grokking_os

    # 4. 执行替换
    print("\n[4] 执行替换...")
    modified_count = 0

    # 4a. items-registry.js（特殊重写）
    if rewrite_registry(registry_path, replacements):
        modified_count += 1

    # 4b. statuses.json（标准替换 + grok-deadlock 合并）
    if rename_file(statuses_path, replacements):
        modified_count += 1

    # statuses.json 的 grok-deadlock 合并处理（手动）
    statuses_after = safe_read(statuses_path)
    # 如果还有 "grok-deadlock" key，合并到 deadlock_grokking
    if '"grok-deadlock"' in statuses_after:
        # 找两个值
        matches = re.findall(r'"grok-deadlock":\s*"([^"]+)"', statuses_after)
        if matches:
            last_val = matches[-1]  # 取最后一个值
            statuses_after = re.sub(r',?\s*"grok-deadlock":\s*"[^"]+"', '', statuses_after)
            # 注入 deadlock_grokking
            # 在 deadlock 后插入 deadlock_grokking
            statuses_after = re.sub(
                r'("deadlock":\s*"[^"]+")',
                f'\\1,\n  "deadlock_grokking": "{last_val}"',
                statuses_after
            )
            statuses_path.write_text(statuses_after, encoding="utf-8")
            print(f"    [OK] statuses.json (grok-deadlock 合并到 deadlock_grokking)")
            modified_count += 1

    # 4c. r*-progress.md
    for pm in sorted((RADAR_DIR / "data").glob("r*-progress.md")):
        if rename_file(pm, replacements):
            modified_count += 1

    # 4d. 前端 JS/HTML
    skip = {"rename-ids.py", "rename-ids-run.py"}
    for pattern in ["data/**/*.js", "data/**/*.html", "*.html", "server.js"]:
        for fp in RADAR_DIR.glob(pattern):
            if fp.name in skip or not fp.is_file():
                continue
            if rename_file(fp, replacements):
                modified_count += 1

    print(f"\n=== 完成：共修改 {modified_count} 个文件 ===")

    # 5. 验证
    print("\n[5] 验证...")
    new_content = safe_read(registry_path)
    sample = list(replacements.items())[:10]
    all_replaced = all(old not in new_content for old, _ in sample)
    print(f"    {'[PASS]' if all_replaced else '[FAIL]'} 抽样 10 个旧 ID 全部替换成功")

    # 检查 grok-deadlock 是否残留
    if "grok-deadlock" in new_content:
        print(f"    [FAIL] grok-deadlock 仍存在")
    else:
        print(f"    [PASS] grok-deadlock 已完全消除")

    # 检查新 ID 总数
    new_ids = re.findall(r'"([a-z][a-z0-9_]+)":\s*\{', new_content)
    print(f"    新 ID 总数: {len(new_ids)}")

    print(f"\n回退: cd c:/code/book && git checkout -- .")
    print(f"提交: git add -A && git commit -m 'chore(reading-radar): 全链路 card ID 重命名（去掉前缀，统一下划线，18 个冲突已解决）'")

if __name__ == "__main__":
    main()
