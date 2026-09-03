#!/usr/bin/env bash
# =============================================================================
# packaging 演示脚本
# 演示 setup.py / pyproject.toml + 打包分发
# =============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC} $1"; }
log_ok()    { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }

DEMO_DIR="${DEMO_DIR:-/tmp/python_packaging_demo}"

cleanup() {
    log_info "清理演示环境..."
    rm -rf "$DEMO_DIR"
    log_ok "清理完成"
}
trap cleanup EXIT

# =============================================================================
# 1. pyproject.toml 方式（现代推荐）
# =============================================================================
demo_pyproject() {
    echo ""
    echo "============================================================"
    echo "1. pyproject.toml 方式（PEP 517/518 现代标准）"
    echo "============================================================"

    mkdir -p "$DEMO_DIR/mypackage"
    cd "$DEMO_DIR/mypackage"

    # 创建包结构
    cat > mypackage/__init__.py << 'EOF'
"""mypackage - 示例包"""

__version__ = "0.1.0"

def add(a, b):
    """加法函数"""
    return a + b
EOF

    cat > mypackage/utils.py << 'EOF'
"""工具函数"""

def greet(name: str) -> str:
    """问候函数"""
    return f"Hello, {name}!"
EOF

    # 创建 pyproject.toml（现代方式）
    cat > pyproject.toml << 'EOF'
[build-system]
requires = ["setuptools>=45", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "mypackage"
version = "0.1.0"
description = "示例 Python 包"
readme = "README.md"
requires-python = ">=3.8"
license = {text = "MIT"}
authors = [
    {name = "Your Name", email = "you@example.com"}
]
classifiers = [
    "Programming Language :: Python :: 3",
    "License :: OSI Approved :: MIT License",
]

[project.optional-dependencies]
dev = ["pytest", "black", "mypy"]

[tool.setuptools.packages.find]
where = ["."]
EOF

    log_info "pyproject.toml 创建完成"
    cat pyproject.toml
    log_ok "pyproject.toml 演示完成"
}

# =============================================================================
# 2. setup.py 方式（传统兼容）
# =============================================================================
demo_setup_py() {
    echo ""
    echo "============================================================"
    echo "2. setup.py 方式（传统兼容）"
    echo "============================================================"

    cd "$DEMO_DIR/mypackage"

    # 创建 setup.py
    cat > setup.py << 'EOF'
"""传统 setup.py 方式"""
from setuptools import setup, find_packages

setup(
    name="mypackage-legacy",
    version="0.1.0",
    description="示例包（setup.py 方式）",
    packages=find_packages(),
    python_requires=">=3.8",
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
    ],
)
EOF

    log_info "setup.py 创建完成"
    cat setup.py
    log_ok "setup.py 演示完成"
}

# =============================================================================
# 3. 打包验证
# =============================================================================
demo_build() {
    echo ""
    echo "============================================================"
    echo "3. 打包验证"
    echo "============================================================"

    cd "$DEMO_DIR/mypackage"

    # 检查 setuptools 是否支持 pyproject.toml
    log_info "检查打包工具..."
    python3 -c "import setuptools; print(f'setuptools version: {setuptools.__version__}')"

    # 尝试构建（如果 build 可用）
    if python3 -m pip show build &>/dev/null; then
        log_info "构建 wheel 和 sdist..."
        python3 -m build --no-isolation 2>&1 | head -10
    else
        log_info "build 包未安装，跳过实际构建"
        log_info "安装命令: pip install build"
    fi

    log_ok "打包验证完成"
}

main() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║     Python 包打包分发演示（setup.py / pyproject.toml）      ║"
    echo "╚════════════════════════════════════════════════════════════╝"

    demo_pyproject
    demo_setup_py
    demo_build

    echo ""
    echo "============================================================"
    echo "演示完成！关键知识点："
    echo "============================================================"
    echo "• pyproject.toml     # 现代打包标准（PEP 517）"
    echo "• setup.py           # 传统方式，兼容旧项目"
    echo "• setuptools         # 打包后端"
    echo "• build              # pip install build"
    echo "• python -m build    # 构建 wheel/sdist"
    echo "============================================================"
}

main "$@"
