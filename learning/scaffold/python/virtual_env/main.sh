#!/usr/bin/env bash
# =============================================================================
# virtual_env 演示脚本
# 演示 Python venv/virtualenv + pip + 环境隔离
# =============================================================================

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'  # No Color

log_info()  { echo -e "${BLUE}[INFO]${NC} $1"; }
log_ok()    { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# 演示目录
DEMO_DIR="${DEMO_DIR:-/tmp/python_venv_demo}"
PROJECT_DIR="$DEMO_DIR/myproject"

cleanup() {
    log_info "清理演示环境..."
    rm -rf "$DEMO_DIR"
    log_ok "清理完成"
}

# 陷阱：退出时清理
trap cleanup EXIT

# =============================================================================
# 1. venv 模块演示（Python 3 内置）
# =============================================================================
demo_venv() {
    echo ""
    echo "============================================================"
    echo "1. Python venv 虚拟环境演示"
    echo "============================================================"

    mkdir -p "$PROJECT_DIR"
    cd "$PROJECT_DIR"

    # 1.1 创建虚拟环境（使用 venv 模块）
    log_info "使用 python3 -m venv 创建虚拟环境..."
    python3 -m venv venv

    # 1.2 激活虚拟环境
    log_info "激活虚拟环境（source venv/bin/activate）..."
    source venv/bin/activate

    # 验证：检查 Python 路径
    log_info "当前 Python 路径：$(which python3)"
    log_info "当前 pip 路径：$(which pip3)"

    # 1.3 安装包到虚拟环境
    log_info "安装 requests 库到虚拟环境..."
    pip install --quiet requests

    # 1.4 验证安装位置
    log_info "查看已安装的包："
    pip list | grep -i requests

    # 1.5 退出虚拟环境
    deactivate
    log_ok "已退出虚拟环境"

    # 1.6 验证系统 Python 不包含 requests
    log_info "系统 Python 不包含 requests："
    python3 -c "import requests" 2>&1 || log_ok "确认：系统 Python 无 requests"

    log_ok "venv 演示完成"
}

# =============================================================================
# 2. virtualenv 演示（更灵活的工具）
# =============================================================================
demo_virtualenv() {
    echo ""
    echo "============================================================"
    echo "2. virtualenv 工具演示"
    echo "============================================================"

    # 检查 virtualenv 是否安装
    if ! command -v virtualenv &> /dev/null; then
        log_warn "virtualenv 未安装，跳过演示"
        log_info "安装命令：pip install virtualenv"
        return 0
    fi

    cd "$DEMO_DIR"
    mkdir -p virtualenv_demo
    cd virtualenv_demo

    # 2.1 创建指定 Python 版本的虚拟环境
    log_info "使用 virtualenv 创建虚拟环境..."
    virtualenv -p python3 venv_custom

    # 2.2 激活并安装包
    source venv_custom/bin/activate
    pip install --quiet colorama

    # 2.3 查看环境详情
    log_info "虚拟环境信息："
    echo "  VIRTUAL_ENV: $VIRTUAL_ENV"
    echo "  Python: $(which python3)"
    echo "  路径长度: ${#VIRTUAL_ENV}"

    deactivate
    log_ok "virtualenv 演示完成"
}

# =============================================================================
# 3. 环境隔离原理
# =============================================================================
demo_isolation() {
    echo ""
    echo "============================================================"
    echo "3. 环境隔离原理演示"
    echo "============================================================"

    cd "$PROJECT_DIR"
    source venv/bin/activate

    # 3.1 site-packages 路径隔离
    log_info "虚拟环境 site-packages 路径："
    python3 -c "import site; print('  ' + site.getsitepackages()[0])"

    log_info "用户 site-packages 路径："
    python3 -c "import site; print('  ' + site.getusersitepackages())"

    # 3.2 环境变量隔离
    log_info "VIRTUAL_ENV 环境变量：${VIRTUAL_ENV:-未设置}"

    # 3.3 pip list 显示隔离的包
    log_info "虚拟环境中的包数量："
    pip list | wc -l | xargs -I {} echo "  {} 个包"

    deactivate
    log_ok "环境隔离演示完成"
}

# =============================================================================
# 主函数
# =============================================================================
main() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║     Python 虚拟环境（venv/virtualenv）演示                  ║"
    echo "║     演示内容：环境创建、激活、隔离原理                      ║"
    echo "╚════════════════════════════════════════════════════════════╝"

    demo_venv
    demo_virtualenv
    demo_isolation

    echo ""
    echo "============================================================"
    echo "演示完成！关键知识点："
    echo "============================================================"
    echo "• python3 -m venv venv     # 创建虚拟环境"
    echo "• source venv/bin/activate # 激活虚拟环境"
    echo "• deactivate              # 退出虚拟环境"
    echo "• pip install xxx         # 仅安装到当前环境"
    echo "• 虚拟环境隔离 site-packages 目录"
    echo "============================================================"
}

main "$@"
