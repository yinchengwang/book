#!/usr/bin/env bash
# engineering/scripts/coverage/run.sh
# 工程轨道 coverage 主入口
# 用法：./engineering/scripts/coverage/run.sh [--build-dir <dir>] [--source-dir <dir>] [--output-dir <dir>]

set -euo pipefail

# 默认参数
BUILD_DIR="${BUILD_DIR:-build-cov}"
SOURCE_DIR="${SOURCE_DIR:-engineering}"
OUTPUT_DIR="${OUTPUT_DIR:-coverage-html}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINEERING_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() { echo -e "${GREEN}[coverage]${NC} $*"; }
warn() { echo -e "${YELLOW}[coverage]${NC} $*"; }
err() { echo -e "${RED}[coverage]${NC} $*" >&2; }

# 检查工具链
USE_LCOV=0
if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then
    USE_LCOV=1
    log "Step 0: 检测到 lcov — 走 HTML 流程"
else
    warn "lcov/genhtml 未安装 — 走 gcov-only 流程（Windows fallback）"
fi

# Step 1: 配置 CMake（启用 coverage）
log "Step 1: 配置 CMake（启用 coverage）"
cmake -B "${BUILD_DIR}" -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_COVERAGE=ON

# Step 2: 编译
log "Step 2: 编译"
cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || echo 4)"

# Step 3: 根据工具链选择流程
if [[ ${USE_LCOV} -eq 1 ]]; then
    log "Step 3: lcov + genhtml（使用 lcov-aggregate.sh）"
    bash "$(dirname "${BASH_SOURCE[0]}")/lcov-aggregate.sh"
else
    log "Step 3: gcov + aggregate-gcov.py（Windows fallback）"
    cd "${ENGINEERING_DIR}"
    find . -name "*.gcda" -exec gcov {} \; >/dev/null 2>&1 || true
    cd "${ENGINEERING_DIR}"
    mkdir -p coverage-gcov
    find . -name "*.gcov" -exec mv {} coverage-gcov/ \; 2>/dev/null || true
    python3 scripts/coverage/aggregate-gcov.py \
        --gcov-dir coverage-gcov \
        --output docs/coverage-baseline.json || warn "aggregate-gcov 失败"
fi

# Step 3: 跑测试（best-effort，失败不阻断 coverage）
log "Step 3: 跑测试（best-effort）"
if (cd "${BUILD_DIR}" && ctest --output-on-failure -j4); then
    log "测试通过"
else
    warn "测试失败但继续生成 coverage 报告"
fi

# Step 4: lcov capture
log "Step 4: lcov capture"
RAW_INFO="coverage.raw.info"
lcov --capture \
    --directory "${BUILD_DIR}" \
    --output-file "${RAW_INFO}" \
    --rc lcov_branch_coverage=1 \
    --ignore-errors mismatch,empty,unused \
    2>&1 | tail -20

# Step 5: 过滤（只保留 engineering/src/）
log "Step 5: 过滤 coverage 数据（仅 engineering/src/）"
FILTERED_INFO="coverage.info"
ENGINEERING_ABS="$(cd "${SOURCE_DIR}" && pwd)"
lcov --extract "${RAW_INFO}" \
    "${ENGINEERING_ABS}/*" \
    --output-file "${FILTERED_INFO}" \
    --rc lcov_branch_coverage=1 \
    --ignore-errors empty,unused \
    2>&1 | tail -10

# Step 6: genhtml
log "Step 6: 生成 HTML 报告 → ${OUTPUT_DIR}/"
genhtml "${FILTERED_INFO}" \
    --output-directory "${OUTPUT_DIR}" \
    --branch-coverage=1 \
    --legend \
    --ignore-errors source 2>&1 | tail -10

# Step 7: 输出摘要
log "Step 7: 覆盖率摘要"
echo ""
echo "=========================="
echo "  覆盖率摘要（按模块）"
echo "=========================="
lcov --list "${FILTERED_INFO}" 2>&1 | head -50
echo ""
log "HTML 报告: ${OUTPUT_DIR}/index.html"
log "原始数据: ${RAW_INFO}"
log "过滤数据: ${FILTERED_INFO}"
log "✓ 完成"