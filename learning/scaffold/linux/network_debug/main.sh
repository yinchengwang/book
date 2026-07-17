#!/bin/bash
# =============================================================================
# @file main.sh
# @brief Linux 网络诊断工具演示脚本
#
# 本脚本演示：
# 1. tcpdump 抓包基础
# 2. netstat/ss 查看连接状态
# 3. 连接状态诊断（TIME_WAIT/CLOSE_WAIT）
# 4. 网络延迟测量
#
# 学习目标：
# - 掌握常用网络诊断命令
# - 理解 TCP 连接状态及异常处理
# - 能够排查网络连接问题
# =============================================================================

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印函数
info()    { echo -e "${BLUE}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[OK]${NC}   $1"; }
warn()    { echo -e "${YELLOW}[WARN]${NC} $1"; }
error()   { echo -e "${RED}[ERROR]${NC} $1"; }

# 分隔符
separator() {
    echo ""
    echo "============================================================"
    echo "  $1"
    echo "============================================================"
}

# -----------------------------------------------------------------------------
# 第一部分：tcpdump 基础
# -----------------------------------------------------------------------------

demo_tcpdump() {
    separator "1. tcpdump 抓包基础"

    echo "tcpdump 是 Linux 下最常用的网络抓包工具"
    echo ""

    echo "常用参数："
    echo "  -i <interface>  指定网卡（any 表示所有）"
    echo "  -n              不解析域名（显示 IP）"
    echo "  -nn             不解析域名和端口"
    echo "  -c <count>      抓取指定数量数据包后退出"
    echo "  -X              以十六进制和 ASCII 显示包内容"
    echo "  -v              显示详细信息"
    echo ""

    echo "常用过滤表达式："
    echo "  tcp                 只抓 TCP 包"
    echo "  port 8888           只抓端口 8888 的包"
    echo "  host 192.168.1.1    只抓特定 IP"
    echo "  src port 80         源端口 80"
    echo "  dst port 80         目标端口 80"
    echo "  tcp[tcpflags] == tcp-syn  只抓 SYN 包"
    echo ""

    echo "示例命令："
    echo "  # 抓取本机 8888 端口的 TCP 包"
    echo "  sudo tcpdump -i lo -nn port 8888"
    echo ""
    echo "  # 抓取 HTTP 请求"
    echo "  sudo tcpdump -i eth0 -X port 80"
    echo ""
    echo "  # 抓取 SYN 包（检测连接建立）"
    echo "  sudo tcpdump -i any -nn 'tcp[tcpflags] == tcp-syn'"
    echo ""

    # 实际演示：抓取 lo 接口的包（3秒超时）
    if command -v tcpdump &> /dev/null; then
        info "尝试抓取 lo 接口的 ICMP 包（3秒）..."
        timeout 3 tcpdump -i lo -c 3 -nn 2>/dev/null || true
        success "tcpdump 可用"
    else
        warn "tcpdump 未安装（跳过实际演示）"
    fi
}

# -----------------------------------------------------------------------------
# 第二部分：netstat / ss 查看连接状态
# -----------------------------------------------------------------------------

demo_netstat_ss() {
    separator "2. netstat / ss 查看连接状态"

    echo "查看网络连接状态是排查网络问题的第一步"
    echo ""

    echo "netstat（传统工具）："
    echo "  netstat -ant        查看所有 TCP 连接（数字格式）"
    echo "  netstat -anu        查看所有 UDP 连接"
    echo "  netstat -tlnp       查看监听端口"
    echo "  netstat -s          查看统计信息"
    echo ""

    echo "ss（现代工具，比 netstat 更快）："
    echo "  ss -ant             查看所有 TCP 连接"
    echo "  ss -anu             查看所有 UDP 连接"
    echo "  ss -tlnp            查看监听端口（显示进程）"
    echo "  ss -s               查看统计摘要"
    echo "  ss -o               显示定时器信息"
    echo ""

    # 实际演示
    info "当前监听端口（ss -tlnp）："
    ss -tlnp 2>/dev/null | head -10 || warn "ss 命令不可用"

    echo ""
    info "当前 TCP 连接状态（ss -ant）："
    ss -ant 2>/dev/null | head -10 || warn "ss 命令不可用"

    echo ""
    success "ss 是 netstat 的现代替代品，输出更清晰"
}

# -----------------------------------------------------------------------------
# 第三部分：连接状态诊断
# -----------------------------------------------------------------------------

demo_connection_states() {
    separator "3. TCP 连接状态诊断"

    echo "TCP 连接状态说明："
    echo ""
    echo "| 状态        | 说明                                    |"
    echo "|-------------|----------------------------------------|"
    echo "| LISTEN      | 服务器正在监听端口                      |"
    echo "| ESTABLISHED | 连接已建立，双方可以传输数据            |"
    echo "| TIME_WAIT   | 主动关闭方等待 2MSL，确保对方收到 ACK   |"
    echo "| CLOSE_WAIT  | 被动关闭方等待应用程序调用 close()      |"
    echo "| FIN_WAIT1   | 主动关闭方已发送 FIN，等待对方 ACK      |"
    echo "| FIN_WAIT2   | 主动关闭方等待对方发送 FIN              |"
    echo "| SYN_SENT    | 已发送 SYN，等待对方 SYN+ACK            |"
    echo "| SYN_RECV    | 收到 SYN，已发送 SYN+ACK，等待 ACK      |"
    echo ""

    echo "常见问题："
    echo ""
    echo "1. TIME_WAIT 过多"
    echo "   问题：大量连接处于 TIME_WAIT，占用端口"
    echo "   解决："
    echo "   - 调整 /proc/sys/net/ipv4/tcp_tw_reuse = 1"
    echo "   - 使用 SO_REUSEADDR 选项"
    echo "   - 调高本地端口范围：sysctl -w net.ipv4.ip_local_port_range='1024 65535'"
    echo ""

    echo "2. CLOSE_WAIT 过多"
    echo "   问题：对方已关闭，但本程序未调用 close()"
    echo "   解决："
    echo "   - 检查代码，确保读取返回 0 时调用 close()"
    echo "   - 设置读取超时"
    echo "   - 使用心跳检测空闲连接"
    echo ""

    # 实际演示
    info "检查 TIME_WAIT 连接数量："
    ss -ant 2>/dev/null | awk '{print $1}' | sort | uniq -c | sort -rn || true

    echo ""
    info "检查 CLOSE_WAIT 连接数量："
    ss -ant 2>/dev/null | awk '/CLOSE-WAIT/ {count++} END {print count+0" CLOSE_WAIT 连接"}' || true
}

# -----------------------------------------------------------------------------
# 第四部分：网络延迟测量
# -----------------------------------------------------------------------------

demo_latency() {
    separator "4. 网络延迟测量"

    echo "测量网络延迟的方法："
    echo ""

    echo "1. ping（ICMP 延迟）"
    echo "   ping -c 4 127.0.0.1"
    echo ""

    echo "2. curl（HTTP 延迟）"
    echo "   curl -o /dev/null -s -w '%{time_total}s\\n' http://localhost:8888/"
    echo ""

    echo "3. ss 显示连接建立时间"
    echo "   ss -noo  # -o 显示定时器信息"
    echo ""

    # 实际演示 ping
    if command -v ping &> /dev/null; then
        info "测量本机延迟（ping -c 3 127.0.0.1）："
        ping -c 3 127.0.0.1 2>/dev/null | tail -1 || warn "ping 需要 sudo 权限"
    else
        warn "ping 命令不可用"
    fi

    echo ""
    success "网络诊断是排查连接问题的关键技能"
}

# -----------------------------------------------------------------------------
# 主函数
# -----------------------------------------------------------------------------

main() {
    echo "Linux 网络诊断工具演示"
    echo "======================"
    echo ""
    echo "本脚本演示常用的网络诊断命令和连接状态分析"
    echo ""

    demo_tcpdump
    demo_netstat_ss
    demo_connection_states
    demo_latency

    separator "演示完成"
    echo "本演示覆盖了以下主题："
    echo "1. tcpdump 抓包基础"
    echo "2. netstat/ss 查看连接状态"
    echo "3. TIME_WAIT/CLOSE_WAIT 问题处理"
    echo "4. 网络延迟测量"
    echo ""
    echo "后续学习："
    echo "- Wireshark GUI 抓包分析"
    echo "- strace 追踪系统调用"
    echo "- iperf 网络性能测试"
}

# 执行
main "$@"
