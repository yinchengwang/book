#!/usr/bin/env python3
"""
learning/notes/_scripts/sync_kanban.py
将笔记 frontmatter `ref` 字段与 knowledge_hub 的 kanban ID 对齐。

⚠️ 当前状态：knowledge_hub 的 kanban-data.js 在 S1 实施前位于根目录 apps/web/knowledge_hub/data/，
实施后预期路径为 engineering/apps/web/knowledge_hub/src/data/kanban-data.js。
实际未找到该文件（knowledge_hub 用了不同数据源），本脚本以 stub 形式提供。

用法：
    python3 sync_kanban.py [--dry-run]

输出：对齐报告（每个 ref 在 kanban-data.js 中的匹配状态）
"""
import io
import json
import sys
from pathlib import Path

if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8")


# 候选路径（按优先级查找）
KANBAN_CANDIDATES = [
    "engineering/apps/web/knowledge_hub/src/data/kanban-data.js",
    "engineering/apps/web/knowledge_hub/data/kanban-data.js",
]


def find_kanban_data() -> Path | None:
    """查找 kanban-data.js。"""
    for path in KANBAN_CANDIDATES:
        p = Path(path)
        if p.exists():
            return p
    return None


def main():
    dry_run = "--dry-run" in sys.argv
    print(f"{'[DRY-RUN] ' if dry_run else ''}kanban 同步开始")
    print()

    kanban = find_kanban_data()
    if kanban is None:
        print("⚠️  未找到 kanban-data.js。候选路径：")
        for p in KANBAN_CANDIDATES:
            print(f"   - {p}")
        print()
        print("可能原因：")
        print("  1. knowledge_hub 已废弃 kanban 数据（当前用 Taro + React 实时数据）")
        print("  2. kanban 数据迁移到其他路径")
        print()
        print("建议：")
        print("  - 暂时忽略此脚本")
        print("  - 等待 knowledge_hub 重新提供 kanban 数据导出")
        print("  - 或使用 apps/web/knowledge_hub 的 API 实时同步")
        sys.exit(0)

    print(f"找到 kanban-data.js: {kanban}")
    # 实际解析逻辑（待 kanban-data.js 存在后实现）
    print("(待实现)")


if __name__ == "__main__":
    main()