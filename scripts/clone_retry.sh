#!/bin/bash
# 带重试的克隆脚本
# 用法: bash clone_retry.sh <url> <path> [max_retries]
set -e

URL="$1"
PATH="$2"
MAX_RETRIES="${3:-10}"
TIMEOUT=600000

# 确保父目录存在
mkdir -p "$(dirname "$PATH")"

# 如果已存在，跳过
if [ -d "$PATH/.git" ]; then
    echo "SKIP: $PATH exists"
    exit 0
fi

echo "CLONE: $URL -> $PATH"

for i in $(seq 1 $MAX_RETRIES); do
    if GIT_TERMINAL_PROMPT=0 git -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=60 \
        clone --depth 1 "$URL" "$PATH" 2>&1; then
        echo "OK: $PATH"
        exit 0
    fi
    echo "RETRY $i/$MAX_RETRIES: $PATH"
    sleep 3
done

echo "FAIL: $PATH"
exit 1
