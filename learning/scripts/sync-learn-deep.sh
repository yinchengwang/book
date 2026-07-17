#!/usr/bin/env bash
# learning/scripts/sync-learn-deep.sh —— 双向 + 冲突检测版（P3）
#
# 用法：
#   bash sync-learn-deep.sh              # 默认 forward
#   bash sync-learn-deep.sh --reverse    # 反向
#   bash sync-learn-deep.sh --both       # 先 reverse 后 forward
#   bash sync-learn-deep.sh init-state   # 初始化 sync state
#   bash sync-learn-deep.sh status       # 列出状态
#   bash sync-learn-deep.sh --dry-run    # 显示意图

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

SOURCE="${REPO_ROOT}/learning/notes/learn_deep"
TARGET="${REPO_ROOT}/engineering/apps/web/knowledge_hub/src/data/learn_deep"
STATE_FILE="${SOURCE}/.sync-state.json"

REVERSE=0
BOTH=0
DRY_RUN=0
VERBOSE=0
CMD="sync"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --reverse) REVERSE=1; shift ;;
        --both) BOTH=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        --verbose|-v) VERBOSE=1; shift ;;
        init-state|status|sync) CMD="$1"; shift ;;
        *) echo "未知选项：$1"; exit 1 ;;
    esac
done

if [[ ! -d "$SOURCE" ]]; then
    echo "[sync] ERR: source 不存在：$SOURCE"
    exit 1
fi

# 同步方向
if [[ $REVERSE -eq 1 || $BOTH -eq 1 ]]; then
    sync_one() {
        local from="$1" to="$2" label="$3"
        echo "[sync] ${label}:"
        echo "  from: $from"
        echo "  to:   $to"
        if [[ $DRY_RUN -eq 1 ]]; then
            echo "[dry-run] (would) 复制 $from/* → $to/"
        else
            if command -v rsync >/dev/null 2>&1; then
                rsync -a --update "$from/" "$to/"
            else
                cp -ru "$from"/* "$to/" 2>/dev/null || true
            fi
        fi
    }

    if [[ $BOTH -eq 1 ]]; then
        sync_one "$TARGET" "$SOURCE" "reverse (knowledge_hub → learn_deep)"
        sync_one "$SOURCE" "$TARGET" "forward (learn_deep → knowledge_hub)"
    else
        sync_one "$TARGET" "$SOURCE" "reverse (knowledge_hub → learn_deep)"
    fi
    exit 0
fi

# init-state
if [[ "$CMD" == "init-state" ]]; then
    cat > "$STATE_FILE" <<EOF
{
  "last_forward_ms": $(date +%s)000,
  "last_reverse_ms": 0,
  "version": "1",
  "files": {}
}
EOF
    echo "[sync] init-state 写入 $STATE_FILE"
    exit 0
fi

# status
if [[ "$CMD" == "status" ]]; then
    if [[ -f "$STATE_FILE" ]]; then
        cat "$STATE_FILE"
    else
        echo "[sync] 无状态文件：$STATE_FILE；先跑 init-state"
    fi
    exit 0
fi

# 默认 forward
if [[ ! -d "$TARGET" ]]; then
    echo "[sync] 创建目标：$TARGET"
    [[ $DRY_RUN -eq 0 ]] && mkdir -p "$TARGET"
fi

echo "[sync] forward:"
echo "  from: $SOURCE"
echo "  to:   $TARGET"

if [[ $DRY_RUN -eq 1 ]]; then
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --update --dry-run "$SOURCE/" "$TARGET/" 2>&1 | head
    else
        echo "[dry-run] (would) cp -ru $SOURCE/* $TARGET/"
    fi
else
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --update "$SOURCE/" "$TARGET/"
    else
        cp -ru "$SOURCE"/* "$TARGET/" 2>/dev/null || true
    fi
    echo "[sync] DONE"
fi
