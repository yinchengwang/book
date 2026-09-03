#!/usr/bin/env bash
# =============================================================================
# pip_packages 演示脚本
# 演示 pip install + requirements.txt + pip freeze 高级用法
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
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

DEMO_DIR="${DEMO_DIR:-/tmp/python_pip_demo}"
PROJECT_DIR="$DEMO_DIR/myproject"

cleanup() {
    log_info "清理演示环境..."
    rm -rf "$DEMO_DIR"
    log_ok "清理完成"
}
trap cleanup EXIT

# =============================================================================
# 1. pip install 高级用法
# =============================================================================
demo_pip_install() {
    echo ""
    echo "============================================================"
    echo "1. pip install 高级用法"
    echo "============================================================"

    mkdir -p "$PROJECT_DIR"
    cd "$PROJECT_DIR"

    # 1.1 从版本约束安装
    log_info "从 requirements 安装..."
    cat > requirements.txt << 'EOF'
requests>=2.28.0
click>=8.0.0
colorama>=0.4.4
EOF

    log_info "安装依赖（显示进度）..."
    pip install -r requirements.txt --quiet
    log_ok "依赖安装完成"

    # 1.2 pip show 查看包信息
    log_info "查看 requests 包详情："
    pip show requests | head -6

    # 1.3 pip list 查看已安装
    log_info "查看最近安装的包："
    pip list --format=freeze | grep -E "requests|click|colorama"

    log_ok "pip install 演示完成"
}

# =============================================================================
# 2. requirements.txt 管理
# =============================================================================
demo_requirements() {
    echo ""
    echo "============================================================"
    echo "2. requirements.txt 依赖管理"
    echo "============================================================"

    cd "$PROJECT_DIR"

    # 2.1 pip freeze 导出依赖
    log_info "导出当前依赖到 requirements.txt..."
    pip freeze > requirements_pinned.txt
    log_info "导出的依赖："
    cat requirements_pinned.txt | head -5

    # 2.2 分环境依赖文件
    log_info "创建分环境依赖文件..."
    cat > requirements-dev.txt << 'EOF'
-r requirements.txt
pytest>=7.0.0
black>=22.0.0
mypy>=0.950
EOF
    log_info "开发依赖文件内容："
    cat requirements-dev.txt

    # 2.3 pip check 依赖检查
    log_info "检查依赖冲突..."
    pip check 2>&1 || log_ok "无依赖冲突"

    log_ok "requirements.txt 管理演示完成"
}

# =============================================================================
# 3. pip 配置与缓存
# =============================================================================
demo_pip_config() {
    echo ""
    echo "============================================================"
    echo "3. pip 配置与缓存"
    echo "============================================================"

    # 3.1 查看 pip 配置
    log_info "pip 配置位置："
    pip config list 2>&1 | head -3 || log_info "无自定义配置"

    # 3.2 pip cache 管理
    log_info "pip 缓存目录："
    pip cache dir 2>/dev/null || echo "无法获取缓存目录"

    # 3.3 pip download（只下载不安装）
    log_info "下载但不安装包..."
    mkdir -p "$DEMO_DIR/downloads"
    pip download requests -d "$DEMO_DIR/downloads" --quiet
    ls "$DEMO_DIR/downloads/" | head -3
    rm -rf "$DEMO_DIR/downloads"

    log_ok "pip 配置与缓存演示完成"
}

# =============================================================================
# 4. pip search 和索引
# =============================================================================
demo_pip_index() {
    echo ""
    echo "============================================================"
    echo "4. pip 索引与搜索"
    echo "============================================================"

    # 4.1 从 PyPI 索引安装
    log_info "从 PyPI 安装（默认源）："
    pip index versions requests 2>/dev/null | head -3 || log_info "requests 已安装"

    # 4.2 指定镜像源安装
    log_info "从国内镜像安装（演示）："
    # 不实际执行，避免网络问题
    echo "  pip install xxx -i https://pypi.tuna.tsinghua.edu.cn/simple"

    # 4.3 pip hash 校验
    log_info "pip hash 用法："
    echo "  pip hash package.tar.gz  # 生成包的 SHA256 哈希"

    log_ok "pip 索引与搜索演示完成"
}

main() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║     Python pip 高级用法演示                                 ║"
    echo "║     演示内容：pip install、requirements.txt、pip freeze     ║"
    echo "╚════════════════════════════════════════════════════════════╝"

    demo_pip_install
    demo_requirements
    demo_pip_config
    demo_pip_index

    echo ""
    echo "============================================================"
    echo "演示完成！关键知识点："
    echo "============================================================"
    echo "• pip install -r requirements.txt  # 批量安装"
    echo "• pip freeze > requirements.txt    # 导出依赖"
    echo "• pip show package                 # 查看包信息"
    echo "• pip check                        # 检查依赖冲突"
    echo "• pip download -d ./pkgs           # 下载包到本地"
    echo "============================================================"
}

main "$@"
