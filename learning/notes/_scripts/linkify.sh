#!/usr/bin/env bash
# learning/notes/_scripts/linkify.sh
# 笔记 ↔ 代码双向链接脚本（幂等）
#
# 用法：
#   ./linkify.sh [--dry-run]
#
# 策略：
#   笔记 → 代码：扫描 notes/ 下所有 .md，匹配文件名模式，
#             在 learning/code-solutions/ 中找对应解法，插入 wikilink。
#   代码 → 笔记：扫描 code-solutions/ 下所有 .md/README，
#             在 notes/ 中找对应笔记，插入 wikilink。
#
# 幂等：检查 wikilink 是否已存在，重复运行不产生重复链接。

set -euo pipefail

NOTES_DIR="learning/notes"
CODE_DIR="learning/code-solutions"
DRY_RUN=false

if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=true
fi

count_added=0

# 工具函数：检查文件中是否已包含某 wikilink
has_wikilink() {
    local file="$1"
    local link="$2"
    grep -qF "$link" "$file" 2>/dev/null
}

# 工具函数：追加 wikilink（如果不存在）
append_wikilink() {
    local file="$1"
    local link="$2"

    if has_wikilink "$file" "$link"; then
        return 1  # 已存在，跳过
    fi

    if [[ "$DRY_RUN" == "true" ]]; then
        echo "[DRY] would add '$link' to '$file'"
    else
        echo "" >> "$file"
        echo "## 相关代码" >> "$file"
        echo "- $link" >> "$file"
    fi
    count_added=$((count_added + 1))
    return 0
}

# 笔记 → 代码：扫描 LeetCode 题解文件名模式
# 假设：笔记标题 "LeetCode XXXX" 或文件名 "leetcode_XXXX.md"
#       对应代码路径 learning/code-solutions/c/leetcode/<XXXX>.md
for note in "$NOTES_DIR"/*/leet_code*.md "$NOTES_DIR"/*/leetcode*.md; do
    [[ -f "$note" ]] || continue

    # 提取 LeetCode 题号（4 位数字）
    if [[ "$note" =~ ([0-9]{4,5}) ]]; then
        problem_num="${BASH_REMATCH[1]}"
        code_path="$CODE_DIR/c/leetcode/${problem_num}.md"
        if [[ -f "$code_path" ]]; then
            wikilink="[[../../code-solutions/c/leetcode/${problem_num}]]"
            append_wikilink "$note" "$wikilink" || true
        fi
    fi
done

# 笔记 → 代码：数据结构笔记（ds-art, ds-skiplist 等）→ 对应 code
for note in "$NOTES_DIR"/*/ds-*.md "$NOTES_DIR"/ds-*.md; do
    [[ -f "$note" ]] || continue

    basename_no_ext=$(basename "$note" .md)
    # 在 learning/code-solutions/c/ 下找匹配的解法
    for code_file in "$CODE_DIR"/c/*/*/"$basename_no_ext".md "$CODE_DIR"/c/*/"$basename_no_ext".md; do
        if [[ -f "$code_file" ]]; then
            rel_path=$(realpath --relative-to="$(dirname "$note")" "$code_file" 2>/dev/null || echo "$code_file")
            wikilink="[[$rel_path]]"
            append_wikilink "$note" "$wikilink" || true
        fi
    done
done

# 代码 → 笔记：反向链接
for code_file in "$CODE_DIR"/c/leetcode/*.md "$CODE_DIR"/c/*/*/*.md; do
    [[ -f "$code_file" ]] || continue

    basename_no_ext=$(basename "$code_file" .md)
    # 在 notes/ 下找匹配的笔记
    for note in "$NOTES_DIR"/*/"$basename_no_ext".md "$NOTES_DIR"/"$basename_no_ext".md; do
        if [[ -f "$note" ]]; then
            rel_path=$(realpath --relative-to="$(dirname "$code_file")" "$note" 2>/dev/null || echo "$note")
            wikilink="[[$rel_path]]"
            append_wikilink "$code_file" "$wikilink" || true
        fi
    done
done

echo ""
if [[ "$DRY_RUN" == "true" ]]; then
    echo "[DRY-RUN] 共检查链接: 见上面输出"
else
    echo "✓ 双向链接完成，新增链接数: $count_added"
    echo "（重跑此脚本不会产生重复链接）"
fi