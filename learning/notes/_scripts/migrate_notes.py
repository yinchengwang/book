#!/usr/bin/env python3
"""
learning/notes/_scripts/migrate_notes.py
学习轨道笔记迁移脚本。

功能：
- 扫描 learning/notes/ 下所有 .md 文件
- 根据目录/文件名启发式归类到 stack
- 自动补 frontmatter（已存在则跳过）
- 幂等：可重复运行

用法：
    python3 migrate_notes.py [--dry-run] [--limit 20]
    --dry-run    只统计，不实际修改
    --limit N    只处理前 N 个（Phase B 验证用）
"""
import io
import re
import sys
from pathlib import Path
from datetime import date

# 强制 UTF-8 输出
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")

VAULT = Path("learning/notes")
TODAY = date.today().isoformat()

# 启发式归类规则：directory → stack
DIR_STACK = {
    "linux": "linux",
    "HardWare": "_hardware",
    "LLM": "python",
    "Redis": "db",
    "diary": "_diary",
    "excerpt": "_excerpts",
    "language": "c",       # 默认归 c，部分归 cpp 见文件名
    "linux": "linux",
    "neo4j": "db",
    "paper": "_papers",
    "sqlite": "db",
    "vectordb": "db",
    "test": "_test",
    "db": "db",
    "c": "c",
    "ds": "ds",
    "cpp": "cpp",
    "python": "python",
}

# 文件名启发式：filename pattern → stack
FILE_STACK = {
    r"^ds-": "ds",
    r"^c-": "c",
    r"^cpp-": "cpp",
    r"^linux-": "linux",
    r"^db-": "db",
    r"^sql": "db",
    r"^redis": "db",
    r"^python": "python",
    r"^llm": "python",
    r"^hard": "_hardware",
    r"^paper": "_papers",
}

# 默认 frontmatter 模板
DEFAULT_FRONTMATTER = """---
stack: {stack}
difficulty: medium
status: in-progress
links: []
ref: []
created: {today}
updated: {today}
tags: []
---

"""

FRONTMATTER_RE = re.compile(r"^---\n(.*?)\n---\n", re.DOTALL)


def detect_stack(path: Path) -> str:
    """根据目录和文件名推断 stack。"""
    rel = path.relative_to(VAULT)
    parts = rel.parts

    # 先看目录
    if len(parts) > 1:
        parent = parts[0]
        if parent in DIR_STACK:
            return DIR_STACK[parent]

    # 再看文件名
    fname = path.name.lower()
    for pattern, stack in FILE_STACK.items():
        if re.search(pattern, fname):
            return stack

    # 默认归 c（C 是基础）
    return "c"


def has_frontmatter(content: str) -> bool:
    """检查是否已有 frontmatter。"""
    return bool(FRONTMATTER_RE.match(content))


def add_frontmatter(path: Path, stack: str, dry_run: bool = False) -> bool:
    """给笔记添加 frontmatter（如果没有的话）。返回 True 表示修改了。"""
    content = path.read_text(encoding="utf-8")
    if has_frontmatter(content):
        return False  # 已有，跳过

    new_content = DEFAULT_FRONTMATTER.format(stack=stack, today=TODAY) + content
    if not dry_run:
        path.write_text(new_content, encoding="utf-8")
    return True


def migrate(dry_run: bool = False, limit: int | None = None) -> dict:
    """主迁移函数。返回统计字典。"""
    stats = {
        "total": 0,
        "modified": 0,
        "skipped": 0,
        "by_stack": {},
    }

    # 跳过 _index, _templates, _meta, _dashboard 等元目录
    skip_dirs = {"_index", "_templates", "_meta", "_dashboard", "_scripts"}

    md_files = []
    for path in VAULT.rglob("*.md"):
        rel = path.relative_to(VAULT)
        if any(part in skip_dirs for part in rel.parts):
            continue
        md_files.append(path)

    stats["total"] = len(md_files)
    if limit:
        md_files = md_files[:limit]

    for path in md_files:
        stack = detect_stack(path)
        stats["by_stack"][stack] = stats["by_stack"].get(stack, 0) + 1

        if add_frontmatter(path, stack, dry_run):
            stats["modified"] += 1
        else:
            stats["skipped"] += 1

    return stats


def main():
    dry_run = "--dry-run" in sys.argv
    limit = None
    if "--limit" in sys.argv:
        idx = sys.argv.index("--limit")
        if idx + 1 < len(sys.argv):
            limit = int(sys.argv[idx + 1])

    mode = "[DRY-RUN] " if dry_run else ""
    print(f"{mode}学习轨道笔记迁移开始")
    print(f"Vault: {VAULT}")
    print(f"Limit: {limit}")
    print()

    stats = migrate(dry_run=dry_run, limit=limit)

    print(f"总扫描: {stats['total']}")
    print(f"已修改: {stats['modified']}")
    print(f"跳过（已有 frontmatter）: {stats['skipped']}")
    print()
    print("按 stack 分布:")
    for stack, count in sorted(stats["by_stack"].items(), key=lambda x: -x[1]):
        print(f"  {stack}: {count}")


if __name__ == "__main__":
    main()