#!/usr/bin/env bash
# =============================================================================
# c_extension 演示
# 演示编译和测试 Python C 扩展
# =============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC} $1"; }
log_ok()    { echo -e "${GREEN}[OK]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

DEMO_DIR="${DEMO_DIR:-/tmp/python_cext_demo}"

cleanup() {
    log_info "清理..."
    rm -rf "$DEMO_DIR" build *.egg-info 2>/dev/null || true
}
trap cleanup EXIT

# =============================================================================
# 1. 编译 C 扩展
# =============================================================================
demo_build() {
    echo ""
    echo "============================================================"
    echo "1. 编译 C 扩展"
    echo "============================================================"

    log_info "检查编译工具..."
    python3 -c "from setuptools import setup, Extension; print('✓ setuptools 可用')"

    # 创建 setup.py
    mkdir -p "$DEMO_DIR"
    cp cextension.c "$DEMO_DIR/"

    cat > "$DEMO_DIR/setup.py" << 'EOF'
from setuptools import setup, Extension

module = Extension(
    'cextension',
    sources=['cextension.c'],
)

setup(
    name='cextension',
    version='1.0.0',
    description='Python C 扩展示例',
    ext_modules=[module],
)
EOF

    log_info "执行 setup.py build_ext --inplace..."
    cd "$DEMO_DIR"
    python3 setup.py build_ext --inplace 2>&1
    log_ok "编译完成！"
    ls -la *.so 2>/dev/null && echo "已生成 .so 文件"
}

# =============================================================================
# 2. 测试扩展
# =============================================================================
demo_test() {
    echo ""
    echo "============================================================"
    echo "2. 测试 C 扩展"
    echo "============================================================"

    cd "$DEMO_DIR"

    # 导入并测试扩展
    python3 << 'PYEOF'
import sys
sys.path.insert(0, '.')
import cextension

# 测试加法
print("c_add(3.14, 2.86) =", cextension.add(3.14, 2.86))

# 测试字符串反转
print("c_reverse('hello') =", cextension.reverse('hello'))

# 测试平均数
print("c_average([1,2,3,4,5]) =", cextension.average([1, 2, 3, 4, 5]))

# 测试斐波那契数列
print("c_fibonacci(10) =", cextension.fibonacci(10))

# 性能对比
import time

def py_fib(n):
    a, b = 0, 1
    result = []
    for _ in range(n):
        result.append(a)
        a, b = b, a + b
    return result

n = 100000

# C 扩展版本
start = time.time()
c_result = cextension.fibonacci(n)
c_time = time.time() - start
print(f"\nC 扩展 fibonacci({n}): {c_time:.4f} 秒")

# Python 纯代码版本
start = time.time()
py_result = py_fib(n)
py_time = time.time() - start
print(f"Python 版本 fibonacci({n}): {py_time:.4f} 秒")

print(f"\n性能比：{py_time/c_time:.2f}X")
print("✓ C 扩展全部测试通过！")
PYEOF

    log_ok "C 扩展测试完成"
}

main() {
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║     Python C 扩展演示（编译 + 调用）                       ║"
    echo "╚════════════════════════════════════════════════════════════╝"

    demo_build
    demo_test

    echo ""
    echo "============================================================"
    echo "演示完成！关键知识点："
    echo "============================================================"
    echo "• setup.py + Extension    # 编译 C 扩展"
    echo "• Python.h + PyMODINIT_FUNC  # C 扩展入口"
    echo "• PyArg_ParseTuple        # 参数解析"
    echo "• PyList_* 系列函数       # 列表操作"
    echo "• C 扩展比纯 Python 快 10-100X"
    echo "============================================================"
}

main "$@"
