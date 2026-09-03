/**
 * TCP 拥塞控制演示程序
 * 演示查询和设置 TCP 拥塞控制算法，理解 cwnd（拥塞窗口）概念
 *
 * 编译: gcc -std=c11 -o tcp_congestion main.c
 * 运行: ./tcp_congestion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_LEN 256

/**
 * 读取 /proc 文件内容
 */
static int read_proc_file(const char *path, char *buf, size_t size)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    if (fgets(buf, (int)size, fp) == NULL) {
        fclose(fp);
        return -1;
    }

    // 去掉换行符
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    fclose(fp);
    return 0;
}

/**
 * 查询当前拥塞控制算法
 * 文件: /proc/sys/net/ipv4/tcp_congestion_control
 */
static void query_current_algorithm(void)
{
    char buf[64];
    const char *path = "/proc/sys/net/ipv4/tcp_congestion_control";

    printf("=== 查询当前拥塞控制算法 ===\n");
    printf("文件: %s\n\n", path);

    if (read_proc_file(path, buf, sizeof(buf)) == 0) {
        printf("当前算法: %s\n\n", buf);
    } else {
        printf("无法读取，当前系统可能不支持\n\n");
    }
}

/**
 * 查询可用的拥塞控制算法列表
 * 文件: /proc/sys/net/ipv4/tcp_available_congestion_control
 */
static void query_available_algorithms(void)
{
    char buf[256];
    const char *path = "/proc/sys/net/ipv4/tcp_available_congestion_control";

    printf("=== 查询可用拥塞控制算法 ===\n");
    printf("文件: %s\n\n", path);

    if (read_proc_file(path, buf, sizeof(buf)) == 0) {
        printf("可用算法: %s\n", buf);
        printf("  (空格分隔的算法列表)\n\n");
    } else {
        printf("无法读取\n\n");
    }
}

/**
 * 解释 CUBIC vs BBR 算法
 */
static void explain_algorithms(void)
{
    printf("=== CUBIC vs BBR 算法对比 ===\n\n");

    printf("【CUBIC (TCP Cubic)】\n");
    printf("  - Linux 默认拥塞控制算法\n");
    printf("  - 基于丢包驱动: 检测到丢包时认为发生拥塞\n");
    printf("  - 窗口增长: 使用三次函数曲线，在高带宽时较激进\n");
    printf("  - 适用场景: 传统数据中心、低延迟网络\n");
    printf("  - 缺点: 在高 BDP (Bandwidth-Delay Product) 网络表现差\n\n");

    printf("【BBR (Bottleneck Bandwidth and Round-trip propagation time)】\n");
    printf("  - Google 2016 年提出，基于模型驱动\n");
    printf("  - 核心思想: 同时测量带宽和 RTT，寻找最佳工作点\n");
    printf("  - 不依赖丢包: 即使没有丢包也会调整窗口\n");
    printf("  - 适用场景: 跨洲际传输、高延迟网络、卫星链路\n");
    printf("  - 优点: 在高 BDP 网络表现优异，减少不必要的重传\n\n");

    printf("【关键差异】\n");
    printf("  - CUBIC: 丢包 → 减小窗口 → 缓慢恢复\n");
    printf("  - BBR: 持续探测带宽和 RTT，保持最佳平衡\n\n");
}

/**
 * 解释拥塞窗口 (cwnd) 概念
 */
static void explain_cwnd(void)
{
    printf("=== 拥塞窗口 (cwnd) 概念 ===\n\n");

    printf("【基本概念】\n");
    printf("  - cwnd (congestion window): 发送方在未收到 ACK 前可发送的数据量\n");
    printf("  - 目的: 防止发送方注入过多数据导致网络拥塞\n");
    printf("  - 单位: 字节或 MSS (Maximum Segment Size)\n\n");

    printf("【窗口限制】\n");
    printf("  - 实际发送量 = min(cwnd, rwnd)\n");
    printf("  - cwnd: 拥塞窗口 (发送方限制)\n");
    printf("  - rwnd: 接收窗口 (接收方限制)\n\n");

    printf("【工作流程】\n");
    printf("  1. 慢启动: cwnd 从小值开始，指数增长\n");
    printf("  2. 拥塞避免: 达到阈值后，线性增长\n");
    printf("  3. 拥塞检测: 丢包时减小 cwnd，重新开始\n\n");

    printf("【查看 cwnd】\n");
    printf("  使用 ss 命令查看连接状态:\n");
    printf("    ss -i -t                              # 查看所有 TCP 连接\n");
    printf("    ss -ti dst 192.168.1.100              # 查看目标地址的连接\n");
    printf("    ss -tn                               # 只显示 IP (不含主机名解析)\n");
    printf("  输出中的 'cwnd' 列显示当前拥塞窗口大小\n\n");

    printf("【相关命令】\n");
    printf("  查看网络栈信息:\n");
    printf("    ss -s                                 # TCP 连接统计\n");
    printf("    nstat -as                             # 网络统计\n\n");
}

/**
 * 设置拥塞控制算法（需要 root 权限）
 */
static void demonstrate_set_algorithm(void)
{
    printf("=== 设置拥塞控制算法 ===\n\n");

    printf("【方法 1: sysctl 命令】\n");
    printf("  sysctl -w net.ipv4.tcp_congestion_control=bbr\n\n");

    printf("【方法 2: 永久设置 (需要 root)】\n");
    printf("  echo 'net.ipv4.tcp_congestion_control=bbr' >> /etc/sysctl.conf\n");
    printf("  sysctl -p\n\n");

    printf("【方法 3: 对单个连接设置】\n");
    printf("  使用 iproute2 工具:\n");
    printf("    ip link set dev eth0 txqueuelen 10000\n");
    printf("  通过路由表为特定流量选择算法:\n");
    printf("    ip rule add from 192.168.1.0/24 cong_ctl bbr\n\n");

    printf("【查看当前设置】\n");
    printf("  sysctl net.ipv4.tcp_congestion_control\n\n");
}

/**
 * 使用 ss 命令展示连接状态
 */
static void demonstrate_ss_command(void)
{
    printf("=== 使用 ss 命令查看连接状态 ===\n\n");

    printf("【常用 ss 命令】\n\n");

    printf("1. 查看所有 TCP 连接:\n");
    printf("   $ ss -tn\n");
    printf("   State      Recv-Q   Send-Q   Local Address:Port   Peer Address:Port\n");
    printf("   ESTAB      0        0        192.168.1.100:22      10.0.0.50:54321\n\n");

    printf("2. 查看详细连接信息 (含 cwnd):\n");
    printf("   $ ss -ti\n");
    printf("   Netid  State   Recv-Q  Send-Q   Local Address:Port   Peer Address:Port\n");
    printf("   tcp    ESTAB   0       0        192.168.1.100:5432    10.0.0.50:54321\n");
    printf("           cubic wscale:7,7 rtt:0.5ms ato:0.05ms cwnd:10\n\n");

    printf("3. 查看特定进程的连接:\n");
    printf("   $ ss -tp | grep postgres\n\n");

    printf("4. TCP 状态统计:\n");
    printf("   $ ss -s\n");
    printf("   Total: 156 (kernel 160)\n");
    printf("   TCP:   12 (estab 5, closed 1, orphan 0, timewait 2)\n\n");

    printf("【ss 输出字段说明】\n");
    printf("  - cwnd: 拥塞窗口大小 (MSS 个数)\n");
    printf("  - rtt: 往返时间 (round-trip time)\n");
    printf("  - rttvar: RTT 变化\n");
    printf("  - pacing_rate: 速率控制\n");
    printf("  - bbr: BBR 算法特有的 pacing_rate 和 cwnd\n\n");
}

/**
 * 主函数
 */
int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("    TCP 拥塞控制演示程序\n");
    printf("========================================\n");
    printf("\n");

    query_current_algorithm();
    query_available_algorithms();
    explain_algorithms();
    explain_cwnd();
    demonstrate_set_algorithm();
    demonstrate_ss_command();

    printf("========================================\n");
    printf("    演示完成\n");
    printf("========================================\n");
    printf("\n");

    return 0;
}
