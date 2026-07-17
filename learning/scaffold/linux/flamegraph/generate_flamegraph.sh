#!/bin/bash
#
# @file generate_flamegraph.sh
# @brief 使用 perf + FlameGraph 生成火焰图
#
# 使用方法:
#   1. 确保已安装 perf (Linux) 或 WSL
#   2. 确保已克隆 FlameGraph:
#      git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph
#   3. 运行本脚本:
#      ./generate_flamegraph.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ========== 前置检查 ==========

echo "========================================"
echo "  火焰图生成脚本"
echo "========================================"
echo ""

# 检查是否为 Linux/WSL
if [[ "$(uname)" != "Linux" ]]; then
    echo "[错误] 此脚本需要在 Linux 或 WSL 环境下运行"
    echo ""
    echo "Windows 用户选项:"
    echo "  1. 在 WSL (Windows Subsystem for Linux) 中运行"
    echo "  2. 使用 Docker 容器"
    exit 1
fi

# 检查 perf 是否存在
if ! command -v perf &> /dev/null; then
    echo "[错误] perf 未安装或不在 PATH 中"
    echo ""
    echo "安装 perf (Ubuntu/Debian):"
    echo "  sudo apt-get install linux-tools-common linux-tools-generic"
    echo ""
    echo "安装 perf (WSL):"
    echo "  1. 确认 WSL 内核支持 perf (需要 WSL2 + 内核模块)"
    echo "  2. 或使用 Docker 方案"
    exit 1
fi

# 检查 FlameGraph 路径
FLAMEGRAPH_DIR="$HOME/FlameGraph"
if [[ ! -d "$FLAMEGRAPH_DIR" ]]; then
    echo "[警告] FlameGraph 未找到于 $FLAMEGRAPH_DIR"
    echo ""
    echo "请先安装 FlameGraph:"
    echo "  git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph"
    echo ""
    echo "跳过此检查，继续生成 perf 数据..."
    FLAMEGRAPH_DIR=""
fi

# ========== 编译程序 ==========

echo "[1/4] 编译程序..."
echo "----------------------------------------"

if [[ ! -f "Makefile" ]]; then
    echo "编译: g++ -O2 -g -o main main.cpp -lm"
    g++ -O2 -g -o main main.cpp -lm
else
    echo "编译: make"
    make
fi

if [[ ! -f "./main" ]]; then
    echo "[错误] 编译失败，未生成 main 可执行文件"
    exit 1
fi

echo "编译成功"
echo ""

# ========== perf 采样 ==========

echo "[2/4] 运行 perf 采样..."
echo "----------------------------------------"
echo "采样命令: perf record -F 99 -g ./main"
echo "采样中 (约 5-10 秒)..."
echo ""

# 清理旧的采样数据
rm -f perf.data perf.data.old

# 运行 perf 采样
# -F 99: 99Hz 采样频率
# -g: 收集调用栈
# --call-graph dwarf: 使用 DWARF 调用图 (更精确)
perf record -F 99 -g --call-graph dwarf ./main

if [[ ! -f "perf.data" ]]; then
    echo "[错误] perf 采样失败"
    exit 1
fi

echo ""
echo "采样完成"
echo ""

# ========== 生成火焰图 ==========

echo "[3/4] 生成火焰图..."
echo "----------------------------------------"

if [[ -n "$FLAMEGRAPH_DIR" && -f "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" ]]; then
    echo "使用 FlameGraph 工具生成 SVG..."

    # perf script: 将采样数据转换为文本格式
    # stackcollapse-perf.pl: 折叠调用栈
    # flamegraph.pl: 生成 SVG 火焰图
    perf script | \
        "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" | \
        "$FLAMEGRAPH_DIR/flamegraph.pl" > flamegraph.svg

    if [[ -f "flamegraph.svg" ]]; then
        echo "火焰图已生成: flamegraph.svg"
    else
        echo "[错误] 火焰图生成失败"
        exit 1
    fi
else
    echo "[提示] FlameGraph 未安装，跳过 SVG 生成"
    echo ""
    echo "可以手动生成火焰图:"
    echo "  1. 安装 FlameGraph:"
    echo "     git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph"
    echo ""
    echo "  2. 运行以下命令:"
    echo "     perf script | ~/FlameGraph/stackcollapse-perf.pl | ~/FlameGraph/flamegraph.pl > flamegraph.svg"
    echo ""
    echo "  3. 用浏览器打开 flamegraph.svg 查看"
fi

echo ""

# ========== 完成 ==========

echo "[4/4] 完成"
echo "========================================"
echo ""
echo "查看火焰图的方法:"
echo "  1. 在浏览器中直接打开: file://$PWD/flamegraph.svg"
echo "  2. 或启动本地服务器:"
echo "     python3 -m http.server 8000"
echo "     然后访问 http://localhost:8000/flamegraph.svg"
echo ""
echo "火焰图解读:"
echo "  - 横向宽度 = 该函数消耗的 CPU 时间比例"
echo "  - 纵向堆叠 = 调用栈 (从下往上看)"
echo "  - 颜色: 暖色(红/橙)=CPU密集, 冷色(蓝/绿)=IO等待"
echo ""
echo "========================================"
