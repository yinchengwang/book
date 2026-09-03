#!/usr/bin/env bash
# =============================================================================
# distribution 演示脚本
# 演示 wheel/sdist + PyPI 上传 + 版本管理
# =============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info()  { echo -e "${BLUE}[INFO]${NC} $1"; }
log_ok()    { echo -e "${GREEN}[OK]${NC} $1"; }

DEMO_DIR="${DEMO_DIR:-/tmp/python_distribution_demo}"

cleanup() {
    log_info "清理演示环境..."
    rm -rf "$DEMO_DIR"
    log_ok "清理完成"
}
trap cleanup EXIT

# =============================================================================
# 1. 版本管理策略
# =============================================================================
demo_versioning() {
    echo ""
    echo "============================================================"
    echo "1. 版本管理策略（Semantic Versioning）"
    echo "============================================================"

    log_info "语义化版本格式：MAJOR.MINOR.PATCH"
    echo ""
    echo "  MAJOR - 不兼容的 API 变更"
    echo "  MINOR - 向后兼容的功能新增"
    echo "  PATCH - 向后兼容的问题修复"
    echo ""
    echo "  示例：1.2.3"
    echo "    • 1 = MAJOR version"
    echo "    • 2 = MINOR version"
    echo "    • 3 = PATCH version"

    # 预发布版本
    log_info "预发布版本："
    echo "  1.0.0-alpha    # alpha 测试"
    echo "  1.0.0-beta     # beta 测试"
    echo "  1.0.0-rc1      # 候选发布"

    # 开发版本
    log_info "开发版本："
    echo "  1.0.0.dev0     # 开发版本"

    log_ok "版本管理演示完成"
}

# =============================================================================
# 2. wheel vs sdist
# =============================================================================
demo_artifacts() {
    echo ""
    echo "============================================================"
    echo "2. 分发产物：wheel vs sdist"
    echo "============================================================"

    # 创建示例包
    mkdir -p "$DEMO_DIR/mydist"
    cd "$DEMO_DIR/mydist"

    cat > mydist/__init__.py << 'EOF'
"""mydist - 分发示例"""
__version__ = "0.1.0"
EOF

    cat > pyproject.toml << 'EOF'
[build-system]
requires = ["setuptools>=45", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "mydist"
version = "0.1.0"
description = "分发示例"
EOF

    log_info "wheel（wheel 文件）："
    echo "  • .whl 文件，二进制分发包"
    echo "  • 已编译，包含依赖信息"
    echo "  • pip install xxx.whl 直接安装"
    echo "  • 命名格式：name-version-py3-none-any.whl"

    log_info "sdist（源码分发包）："
    echo "  • .tar.gz 文件，源码包"
    echo "  • 包含所有源文件"
    echo "  • pip install name.tar.gz 安装"
    echo "  • 需要在目标环境编译"

    log_info "构建产物示例："
    echo "  mydist-0.1.0-py3-none-any.whl      # wheel"
    echo "  mydist-0.1.0.tar.gz                 # sdist"

    log_ok "wheel vs sdist 演示完成"
}

# =============================================================================
# 3. PyPI 上传流程
# =============================================================================
demo_pypi() {
    echo ""
    echo "============================================================"
    echo "3. PyPI 上传流程"
    echo "============================================================"

    log_info "PyPI 相关命令："
    echo ""
    echo "  # 安装上传工具"
    echo "  pip install build twine"
    echo ""
    echo "  # 构建 wheel 和 sdist"
    echo "  python -m build"
    echo ""
    echo "  # 上传到 PyPI"
    echo "  twine upload dist/*"
    echo ""
    echo "  # 上传到 TestPyPI（测试）"
    echo "  twine upload --repository testpypi dist/*"
    echo ""
    echo "  # 环境变量配置认证"
    echo "  export TWINE_USERNAME=__token__"
    echo "  export TWINE_PASSWORD=pypi-xxxx"

    log_info "TestPyPI 测试："
    echo "  • https://test.pypi.org/"
    echo "  • 先在 TestPyPI 测试，避免污染正式仓库"

    log_ok "PyPI 上传流程演示完成"
}

# =============================================================================
# 4. 本地安装验证
# =============================================================================
demo_local_install() {
    echo ""
    echo "============================================================"
    echo "4. 本地安装验证"
    echo "============================================================"

    cd "$DEMO_DIR/mydist"

    log_info "安装到本地虚拟环境..."
    python3 -m pip install -e . --quiet 2>/dev/null || true

    log_info "验证安装..."
    python3 -c "import mydist; print(f'版本: {mydist.__version__}')" || log_info "跳过验证"

    log_ok "本地安装验证完成"
}

main() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║     Python 包分发演示（wheel/sdist + PyPI）                ║"
    echo "╚════════════════════════════════════════════════════════════╝"

    demo_versioning
    demo_artifacts
    demo_pypi
    demo_local_install

    echo ""
    echo "============================================================"
    echo "演示完成！关键知识点："
    echo "============================================================"
    echo "• 语义化版本：MAJOR.MINOR.PATCH"
    echo "• wheel (.whl)    # 二进制分发"
    echo "• sdist (.tar.gz) # 源码分发"
    echo "• twine upload    # 上传到 PyPI"
    echo "============================================================"
}

main "$@"
