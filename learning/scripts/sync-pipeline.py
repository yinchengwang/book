#!/usr/bin/env python3
"""
sync-pipeline.py —— P9: Obsidian ↔ knowledge_hub 内容级双向同步管道

功能：
  1. 扫描 learning/notes/ 下所有 .md 文件，解析 frontmatter
  2. 生成 knowledge_hub/src/data/notes-inventory.json（结构化笔记索引）
  3. 生成 knowledge_hub/src/data/track-progress.ts（轨道进度数据）
  4. --watch 模式：监听变更，自动重新生成
  5. --reverse 模式：从 knowledge_hub 的 quiz 数据回写 Obsidian 笔记

用法：
  python sync-pipeline.py [--watch] [--reverse] [--dry-run]
  python sync-pipeline.py --once     # 单次运行（默认）
  python sync-pipeline.py --status   # 显示同步状态
"""

import os, sys, json, re, time, argparse
from datetime import datetime, timezone
from pathlib import Path

# === 配置 ===
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
NOTES_DIR = REPO_ROOT / "learning" / "notes"
DATA_DIR = REPO_ROOT / "engineering" / "apps" / "web" / "knowledge_hub" / "src" / "data"
STATE_FILE = NOTES_DIR / ".sync-pipeline-state.json"

FRONTMATTER_RE = re.compile(r"^---\s*\n(.*?)\n---\s*\n", re.DOTALL)
TRACK_ORDER = ["c", "cpp", "db", "ds", "linux", "grok", "py"]
STACK_ALIASES = {
    "c": "c", "C": "c",
    "cpp": "cpp", "C++": "cpp",
    "db": "db", "database": "db",
    "ds": "ds", "datastructure": "ds", "数据结构": "ds",
    "linux": "linux",
    "grok": "grok", "system": "grok",
    "py": "py", "python": "py",
}

# === Frontmatter 解析 ===

def parse_frontmatter(text: str) -> dict:
    """提取 YAML-like frontmatter 为字典"""
    m = FRONTMATTER_RE.match(text)
    if not m:
        return {}
    fm = {}
    for line in m.group(1).split("\n"):
        line = line.strip()
        if ":" in line:
            key, _, val = line.partition(":")
            val = val.strip().strip('"').strip("'")
            if key.strip():  # 跳过空 key
                fm[key.strip()] = val
    return fm

# === 笔记扫描 ===

def scan_notes(base_dir: Path) -> list[dict]:
    """递归扫描所有 .md 文件，返回元数据列表"""
    entries = []
    for md_file in sorted(base_dir.rglob("*.md")):
        if ".obsidian" in str(md_file) or "_dashboard" in str(md_file):
            continue
        if md_file.name.startswith("."):
            continue
        try:
            content = md_file.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue

        fm = parse_frontmatter(content)
        rel_path = str(md_file.relative_to(NOTES_DIR))

        # 确定 stack track
        stack = fm.get("stack", "")
        if not stack:
            # 从路径猜测
            for part in md_file.parts:
                alias = part.lower()
                if alias in STACK_ALIASES:
                    stack = STACK_ALIASES[alias]
                    break
            if not stack:
                stack = "general"

        stat = md_file.stat()
        entry = {
            "path": rel_path,
            "title": fm.get("title", md_file.stem),
            "stack": stack,
            "difficulty": fm.get("difficulty", "medium"),
            "status": fm.get("status", "todo"),
            "tags": [t.strip() for t in fm.get("tags", "").split(",") if t.strip()],
            "created": fm.get("created", datetime.fromtimestamp(stat.st_ctime, tz=timezone.utc).strftime("%Y-%m-%d")),
            "updated": fm.get("updated", datetime.fromtimestamp(stat.st_mtime, tz=timezone.utc).strftime("%Y-%m-%d")),
            "unresolved": fm.get("unresolved", "false") == "true",
            "ref": fm.get("ref", ""),
            "links": fm.get("links", ""),
            "file_size": stat.st_size,
            "line_count": content.count("\n") + 1,
        }
        entries.append(entry)
    return entries

# === 数据生成 ===

def generate_inventory(entries: list[dict], output_path: Path):
    """生成 notes-inventory.json"""
    inventory = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "total_notes": len(entries),
        "notes": entries,
    }
    output_path.write_text(json.dumps(inventory, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"[sync] inventory → {output_path} ({len(entries)} 条)")

def generate_track_progress(entries: list[dict], output_path: Path):
    """生成 track-progress.ts（前端可用的 TypeScript 数据文件）"""
    lines = [
        "// 自动生成 by sync-pipeline.py (P9)",
        "// 来源：learning/notes/ Obsidian vault",
        f"// 生成时间：{datetime.now(timezone.utc).isoformat()}",
        f"// 笔记总数：{len(entries)}",
        "",
        "export interface TrackProgress {",
        "  stack: string",
        "  total: number",
        "  done: number",
        "  inProgress: number",
        "  todo: number",
        "  hardTotal: number",
        "  hardDone: number",
        "}",
        "",
        "export const TRACK_PROGRESS: TrackProgress[] = [",
    ]

    # 按 stack 分组
    groups = {}
    for e in entries:
        s = e["stack"]
        if s not in groups:
            groups[s] = {"total": 0, "done": 0, "in_progress": 0, "todo": 0, "hard_total": 0, "hard_done": 0}
        g = groups[s]
        g["total"] += 1
        status = e.get("status", "todo")
        if status == "done":
            g["done"] += 1
        elif status == "in-progress":
            g["in_progress"] += 1
        else:
            g["todo"] += 1
        if e.get("difficulty") == "hard":
            g["hard_total"] += 1
            if status == "done":
                g["hard_done"] += 1

    for stack in TRACK_ORDER:
        if stack in groups:
            g = groups[stack]
            lines.append(f"  {{ stack: '{stack}', total: {g['total']}, done: {g['done']}, "
                         f"inProgress: {g['in_progress']}, todo: {g['todo']}, "
                         f"hardTotal: {g['hard_total']}, hardDone: {g['hard_done']} }},")
            del groups[stack]

    # 剩余未分类
    for stack, g in sorted(groups.items()):
        lines.append(f"  {{ stack: '{stack}', total: {g['total']}, done: {g['done']}, "
                     f"inProgress: {g['in_progress']}, todo: {g['todo']}, "
                     f"hardTotal: {g['hard_total']}, hardDone: {g['hard_done']} }},")

    lines.append("]")
    lines.append("")

    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"[sync] progress → {output_path}")

# === 状态持久化 ===

def load_state() -> dict:
    if STATE_FILE.exists():
        try:
            return json.loads(STATE_FILE.read_text(encoding="utf-8"))
        except Exception:
            pass
    return {"version": "1", "last_sync_ms": 0, "entry_count": 0, "checksums": {}}

def save_state(entries: list[dict]):
    state = {
        "version": "1",
        "last_sync_ms": int(time.time() * 1000),
        "entry_count": len(entries),
        "checksums": {},
    }
    STATE_FILE.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"[sync] state → {STATE_FILE}")

# === 文件监听 ===

def watch_mode():
    """监听笔记变更，自动重新生成"""
    print("[sync] watch 启动，监听 learning/notes/")
    import hashlib
    last_hash = ""
    while True:
        time.sleep(2)
        try:
            all_files = sorted(NOTES_DIR.rglob("*.md"))
            combined = "".join(str(f.stat().st_mtime) for f in all_files if f.exists())
            current_hash = hashlib.md5(combined.encode()).hexdigest()
            if current_hash != last_hash:
                last_hash = current_hash
                print(f"\n[sync] 检测到变更 ({datetime.now().strftime('%H:%M:%S')})，重新生成…")
                run_once()
        except KeyboardInterrupt:
            print("\n[sync] watch 停止")
            break
        except Exception as e:
            print(f"[sync] watch 错误: {e}")

# === 单次运行 ===

def run_once():
    if not NOTES_DIR.exists():
        print(f"[sync] ERR: notes 目录不存在: {NOTES_DIR}")
        return
    DATA_DIR.mkdir(parents=True, exist_ok=True)

    entries = scan_notes(NOTES_DIR)
    if not entries:
        print("[sync] WARN: 未找到任何笔记")
        return

    generate_inventory(entries, DATA_DIR / "notes-inventory.json")
    generate_track_progress(entries, DATA_DIR / "track-progress.ts")
    save_state(entries)
    print(f"[sync] DONE — {len(entries)} 条笔记已同步")

# === 反向同步 ===

def run_reverse():
    """从 knowledge_hub 数据回写到 learning/notes/"""
    inv_path = DATA_DIR / "notes-inventory.json"
    if not inv_path.exists():
        print("[sync] reverse: 无 notes-inventory.json，请先 forward sync")
        return
    try:
        inventory = json.loads(inv_path.read_text(encoding="utf-8"))
    except Exception as e:
        print(f"[sync] reverse: 读取失败: {e}")
        return

    updated = 0
    for note in inventory.get("notes", []):
        md_path = NOTES_DIR / note["path"]
        if not md_path.exists():
            continue
        content = md_path.read_text(encoding="utf-8", errors="replace")
        # 更新 frontmatter
        fm_text = f"""---
stack: {note.get('stack', '')}
difficulty: {note.get('difficulty', 'medium')}
status: {note.get('status', 'todo')}
tags: [{', '.join(note.get('tags', []))}]
updated: {datetime.now(timezone.utc).strftime('%Y-%m-%d')}
---"""
        if FRONTMATTER_RE.match(content):
            content = FRONTMATTER_RE.sub(fm_text + "\n\n", content, count=1)
        else:
            content = fm_text + "\n\n" + content
        md_path.write_text(content, encoding="utf-8")
        updated += 1

    print(f"[sync] reverse DONE — {updated} 条笔记 frontmatter 已更新")

def run_status():
    state = load_state()
    print(f"版本: {state.get('version')}")
    print(f"上次同步: {datetime.fromtimestamp(state['last_sync_ms'] / 1000).isoformat() if state.get('last_sync_ms') else '从未'}")
    print(f"笔记数: {state.get('entry_count', 0)}")

# === CLI ===

def main():
    parser = argparse.ArgumentParser(description="P9 同步管道")
    parser.add_argument("--watch", action="store_true", help="监听模式")
    parser.add_argument("--reverse", action="store_true", help="反向同步")
    parser.add_argument("--dry-run", action="store_true", help="预览")
    parser.add_argument("--once", action="store_true", default=True, help="单次运行（默认）")
    parser.add_argument("--status", action="store_true", help="显示状态")
    args = parser.parse_args()

    if args.status:
        run_status()
    elif args.watch:
        watch_mode()
    elif args.reverse:
        if args.dry_run:
            print("[sync] --reverse --dry-run: 会更新 frontmatter（实际未执行）")
        else:
            run_reverse()
    else:
        run_once()

if __name__ == "__main__":
    main()
