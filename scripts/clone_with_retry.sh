#!/bin/bash
# 带重试逻辑的 git clone 脚本
# 用法: ./clone_with_retry.sh <url> <path> <max_retries>
# 超时时间: 10分钟, 低速限制: 1KB/s, 低速时间: 60s

set -e

URL="$1"
PATH="$2"
MAX_RETRIES="${3:-10}"
TIMEOUT=600000  # 10分钟

# 确保父目录存在
mkdir -p "$(dirname "$PATH")"

# 如果目录已存在且有 .git 目录，跳过
if [ -d "$PATH/.git" ]; then
    echo "⏭️  $PATH 已存在，跳过"
    exit 0
fi

echo "📦 开始克隆 $URL -> $PATH"

for i in $(seq 1 $MAX_RETRIES); do
    echo "🔄 第 $i/$MAX_RETRIES 次尝试..."

    # 执行克隆，设置超时和低速检测
    if GIT_TERMINAL_PROMPT=0 git -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=60 \
        clone --depth 1 "$URL" "$PATH" 2>&1; then
        echo "✅ 克隆成功: $PATH"
        exit 0
    else
        echo "⚠️  第 $i 次尝试失败"
        # 如果是最后一次尝试，清理失败的目录
        if [ $i -eq $MAX_RETRIES ]; then
            rm -rf "$PATH"
            echo "❌ 达到最大重试次数 ($MAX_RETRIES)，克隆失败: $URL"
            exit 1
        fi
        # 等待 5 秒后重试
        sleep 5
    fi
done
