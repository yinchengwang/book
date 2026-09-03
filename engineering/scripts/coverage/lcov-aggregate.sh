#!/usr/bin/env bash
# engineering/scripts/coverage/lcov-aggregate.sh
# lcov + genhtml 全流程（CI Ubuntu 用）
#
# 与 gcov-only fallback 不同：本脚本依赖 lcov 工具链
# 用法：
#   bash lcov-aggregate.sh [--src-dir <engineering/src>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINEERING_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

SRC_DIR="engineering/src"
BUILD_DIR="${ENGINEERING_DIR}/build-cov"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --src-dir) SRC_DIR="$2"; shift 2 ;;
        *) echo "未知选项 $1"; exit 1 ;;
    esac
done

# 检查 lcov
if ! command -v lcov >/dev/null 2>&1; then
    echo "ERROR: lcov 未安装（apt install lcov）"
    exit 1
fi

cd "${ENGINEERING_DIR}"

# Step 1: capture
echo "Step 1: lcov capture"
lcov --capture \
    --directory "${BUILD_DIR}" \
    --output-file coverage.info \
    --rc lcov_branch_coverage=1 \
    --ignore-errors mismatch,empty,unused 2>&1 | tail -5

# Step 2: filter 仅工程层
echo "Step 2: lcov extract (${SRC_DIR})"
lcov --extract coverage.info \
    "${PWD}/${SRC_DIR}/*" \
    --output-file coverage.filtered.info \
    --rc lcov_branch_coverage=1 \
    --ignore-errors empty,unused 2>&1 | tail -5

# Step 3: HTML
echo "Step 3: genhtml"
mkdir -p coverage-html
genhtml coverage.filtered.info \
    --output-directory coverage-html \
    --branch-coverage=1 \
    --legend \
    --ignore-errors source 2>&1 | tail -5

# Step 4: JSON
echo "Step 4: aggregate"
python3 scripts/coverage/aggregate.py coverage.filtered.info > coverage-current.json || true

# Step 5: 报告
echo ""
echo "===== Coverage Summary ====="
lcov --list coverage.filtered.info 2>&1 | head -30
echo ""
echo "HTML:   $(pwd)/coverage-html/index.html"
echo "Raw:    $(pwd)/coverage.info"
echo "Filter: $(pwd)/coverage.filtered.info"
echo "JSON:   $(pwd)/coverage-current.json"
