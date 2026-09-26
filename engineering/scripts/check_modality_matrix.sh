#!/usr/bin/env bash
# check_modality_matrix.sh - 模态组合构建矩阵验收（spec §6.2 DoD-2）
#
# 跑 3 个非平凡组合（REL=OFF/KV=OFF 全关无意义跳过）：
#   REL=ON  KV=ON   - 默认构建
#   REL=ON  KV=OFF  - 仅 SQL 路径
#   REL=OFF KV=ON   - 仅 KV 路径
# 每个组合：
#   1) cmake -B build-matrix-<tag> -S . -DBUILD_TESTING=ON \
#            -DMMDB_ENABLE_RELATIONAL=$rel -DMMDB_ENABLE_KV=$kv
#   2) cmake --build build-matrix-<tag> --parallel 4
#   3) REL=ON 时跑 SQL 测试套（DriverSmoke|E2ECapability|CanonicalParser|
#                                SqlIntegration|CliSmoke）
#      REL=OFF 时跳过 SQL 测试（路径不存在）
#
# 输出末尾打印 MATRIX-OK 表示三个组合全部通过。

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT/engineering"

# Windows + Git Bash 路径转换：cygpath 一下
if command -v cygpath >/dev/null 2>&1; then
    REPO_ROOT_WIN="$(cygpath -w "$REPO_ROOT")"
else
    REPO_ROOT_WIN="$REPO_ROOT"
fi

SQL_TESTS_RE='DriverSmoke|E2ECapability|CanonicalParser|SqlIntegration|CliSmoke'

PASS_COUNT=0
FAIL_COUNT=0
FAIL_TAGS=""

for rel in ON OFF; do
    for kv in ON OFF; do
        if [ "$rel" = "OFF" ] && [ "$kv" = "OFF" ]; then
            echo "[skip] RELATIONAL=OFF KV=OFF (无意义空配置)"
            continue
        fi
        tag="r${rel}-k${kv}"
        dir="build-matrix-${tag}"
        echo ""
        echo "=========================================="
        echo "  MATRIX: RELATIONAL=$rel KV=$kv  -> $dir"
        echo "=========================================="

        # 1) configure
        echo "[$tag] cmake -B $dir ..."
        if ! cmake -B "$dir" -S . \
            -DBUILD_TESTING=ON \
            -DMMDB_ENABLE_RELATIONAL="$rel" \
            -DMMDB_ENABLE_KV="$kv" \
            > /dev/null 2>&1; then
            echo "[$tag] CONFIGURE FAIL"
            FAIL_COUNT=$((FAIL_COUNT + 1))
            FAIL_TAGS="$FAIL_TAGS $tag"
            continue
        fi

        # 2) build
        echo "[$tag] cmake --build $dir --parallel 4 ..."
        if ! cmake --build "$dir" --parallel 4 > /dev/null 2>&1; then
            echo "[$tag] BUILD FAIL"
            FAIL_COUNT=$((FAIL_COUNT + 1))
            FAIL_TAGS="$FAIL_TAGS $tag"
            continue
        fi

        # 3) ctest (REL=ON 时跑 SQL 测试，REL=OFF 时跳过)
        if [ "$rel" = "ON" ]; then
            echo "[$tag] ctest --test-dir $dir/test -R '$SQL_TESTS_RE' --timeout 60 ..."
            # CMake lays CTestTestfile.cmake under <build>/test/, so ctest must
            # point at the test subdir, not the build root.
            if ctest --test-dir "$dir/test" --timeout 60 \
                -R "$SQL_TESTS_RE" --output-on-failure 2>&1 \
                | tail -5; then
                PASS_COUNT=$((PASS_COUNT + 1))
            else
                echo "[$tag] CTEST FAIL"
                FAIL_COUNT=$((FAIL_COUNT + 1))
                FAIL_TAGS="$FAIL_TAGS $tag"
            fi
        else
            # REL=OFF：CLI/SQL 不存在，仅验证可链接即可
            echo "[$tag] REL=OFF，跳过 SQL ctest（路径不存在属预期）"
            PASS_COUNT=$((PASS_COUNT + 1))
        fi
    done
done

echo ""
echo "=========================================="
echo "  Matrix Summary: pass=$PASS_COUNT fail=$FAIL_COUNT"
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "MATRIX-OK"
    exit 0
else
    echo "MATRIX-FAIL: $FAIL_TAGS"
    exit 1
fi