#!/bin/bash
# run_all_tests.sh - 编译并运行所有 mystl 测试

set -e

echo "========================================="
echo "  mystl 手写 STL 全面测试套件"
echo "========================================="

cd "$(dirname "$0")"
STL_DIR=$(pwd)

# 编译器设置
CXX="g++"
CXXFLAGS="-std=c++17 -I ../include"

# 测试文件列表
TESTS=(
    "test_type_traits"
    "test_allocator"
    "test_iterator"
    "test_functional"
    "test_memory"
    "test_utility"
    "test_vector"
    "test_array"
    "test_list"
    "test_forward_list"
    "test_deque"
    "test_stack"
    "test_queue"
    "test_priority_queue"
    "test_set"
    "test_multiset"
    "test_map"
    "test_multimap"
    "test_unordered_set"
    "test_unordered_map"
    "test_unordered_multiset"
    "test_unordered_multimap"
    "test_algorithm"
    "test_numeric"
)

# 统计
TOTAL_TESTS=0
TOTAL_PASSED=0
TOTAL_FAILED=0
FAILED_TESTS=""

# 编译并运行每个测试
for test_name in "${TESTS[@]}"; do
    test_file="${test_name}.cpp"
    test_bin="${test_name}"

    if [ -f "$test_file" ]; then
        echo ""
        echo "编译: $test_name..."
        if $CXX $CXXFLAGS -o "$test_bin" "$test_file" 2>&1; then
            echo "运行: $test_name..."
            if output=$(./"$test_bin" 2>&1); then
                # 解析输出统计
                passed=$(echo "$output" | grep "通过:" | sed 's/.*通过: \([0-9]*\).*/\1/')
                failed=$(echo "$output" | grep "失败:" | sed 's/.*失败: \([0-9]*\).*/\1/')
                total=$(echo "$output" | grep "总计:" | sed 's/.*总计: \([0-9]*\).*/\1/')

                if [ -n "$passed" ]; then
                    TOTAL_PASSED=$((TOTAL_PASSED + passed))
                fi
                if [ -n "$failed" ]; then
                    TOTAL_FAILED=$((TOTAL_FAILED + failed))
                    if [ "$failed" -gt 0 ]; then
                        FAILED_TESTS="$FAILED_TESTS $test_name"
                    fi
                fi
                if [ -n "$total" ]; then
                    TOTAL_TESTS=$((TOTAL_TESTS + total))
                fi

                echo "✓ $test_name: $passed/$total 通过"
            else
                echo "✗ $test_name: 运行失败"
                FAILED_TESTS="$FAILED_TESTS $test_name"
            fi
            rm -f "$test_bin"
        else
            echo "✗ $test_name: 编译失败"
            FAILED_TESTS="$FAILED_TESTS $test_name"
        fi
    else
        echo "跳过: $test_name (文件不存在)"
    fi
done

# 打印总结
echo ""
echo "========================================="
echo "  测试总结"
echo "========================================="
echo "总测试数: $TOTAL_TESTS"
echo "通过数:   $TOTAL_PASSED"
echo "失败数:   $TOTAL_FAILED"

if [ "$TOTAL_FAILED" -gt 0 ]; then
    echo ""
    echo "失败的测试:"
    for test in $FAILED_TESTS; do
        echo "  - $test"
    done
    exit 1
else
    echo ""
    echo "✓ 所有测试通过!"
    exit 0
fi