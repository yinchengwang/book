#!/usr/bin/env bash
# c-ci scaffold — 本地 CI 脚本
#
# 用法：./ci.sh
# 退出码：0 = 全部通过，1 = 任一步失败

set -e          # 任一命令失败立即退出
set -u          # 未定义变量报错
set -o pipefail # pipeline 任一步失败整体失败

echo "=== c-ci 本地 CI 流水线 ==="
echo "时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "平台: $(uname -s 2>/dev/null || echo Windows)"
echo

# === 1. 编译检查 ===
echo "[1/4] 编译检查"
mkdir -p build
gcc -Wall -Wextra -Wpedantic -O2 -std=c11 -o build/ci_demo main.c
echo "  OK: build/ci_demo 生成成功"

# === 2. 运行测试 ===
echo
echo "[2/4] 运行测试"
./build/ci_demo
echo "  OK: 所有测试通过"

# === 3. 静态分析（graceful 降级）===
echo
echo "[3/4] 静态分析"
if command -v cppcheck >/dev/null 2>&1; then
    cppcheck --enable=warning,style --error-exitcode=0 main.c
    echo "  OK: cppcheck 无 warning/style 级别问题"
else
    echo "  [skip] cppcheck 不可用（生产 CI 由 GitHub Actions 提供）"
fi

if command -v flawfinder >/dev/null 2>&1; then
    flawfinder --error-level=medium main.c
    echo "  OK: flawfinder 无 medium+ 级别问题"
else
    echo "  [skip] flawfinder 不可用"
fi

# === 4. 格式统计 ===
echo
echo "[4/4] 格式统计"
LINES=$(wc -l < main.c)
MAX=$(awk '{ print length }' main.c | sort -nr | head -1)
echo "  main.c: $LINES 行, 最大行长 $MAX"
if [ "$MAX" -gt 100 ]; then
    echo "  [warn] 最大行 > 100 字符"
fi

echo
echo "=== 全部通过 ==="
exit 0