/*
 * tcp_fastopen.c — TCP Fast Open (TFO) 演示程序
 *
 * 编译: gcc -std=c11 -o tcp_fastopen tcp_fastopen.c
 * 运行: sudo ./tcp_fastopen
 *
 * 功能:
 *   1. 检查 TFO 状态
 *   2. 启用 TFO
 *   3. 演示 TFO Cookie 机制
 *   4. 用 curl/nc 测试 TFO 连接
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ------------------------------------------------------------
 * 辅助函数：打印信息
 * ------------------------------------------------------------ */
static void print_section(const char *title)
{
    printf("\n");
    printf("========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

/* ------------------------------------------------------------
 * 1. 检查 TFO 状态
 *
 * /proc/sys/net/ipv4/tcp_fastopen 是一个 3 位掩码:
 *   bit 0 (1): 客户端启用 TFO
 *   bit 1 (2): 服务端启用 TFO
 *   bit 2 (4): 禁用 TFO 旁路黑名单
 *
 * 值 3 表示客户端+服务端都启用
 * ------------------------------------------------------------ */
static void check_tfo_status(void)
{
    print_section("1. 检查 TFO 状态");
    printf("检查文件: /proc/sys/net/ipv4/tcp_fastopen\n\n");

    FILE *fp = fopen("/proc/sys/net/ipv4/tcp_fastopen", "r");
    if (fp == NULL) {
        perror("  [错误] 无法打开 tcp_fastopen 文件");
        return;
    }

    int value;
    if (fscanf(fp, "%d", &value) == 1) {
        printf("  当前 TFO 值为: %d\n", value);
        printf("  位掩码解释:\n");
        printf("    bit0 (1) = 客户端启用:  %s\n", (value & 1) ? "是" : "否");
        printf("    bit1 (2) = 服务端启用:  %s\n", (value & 2) ? "是" : "否");
        printf("    bit2 (4) = 禁用黑名单:  %s\n", (value & 4) ? "是" : "否");

        if (value == 3) {
            printf("\n  [OK] TFO 已完全启用 (客户端+服务端)\n");
        } else if (value == 0) {
            printf("\n  [X] TFO 未启用，需要手动开启\n");
            printf("  启用命令: sudo sysctl -w net.ipv4.tcp_fastopen=3\n");
        } else {
            printf("\n  [!] TFO 部分启用，建议设置为 3\n");
        }
    }
    fclose(fp);
}

/* ------------------------------------------------------------
 * 2. 启用 TFO
 *
 * sysctl -w net.ipv4.tcp_fastopen=3
 * ------------------------------------------------------------ */
static void enable_tfo(void)
{
    print_section("2. 启用 TFO");

    printf("尝试写入 /proc/sys/net/ipv4/tcp_fastopen = 3\n\n");

    FILE *fp = fopen("/proc/sys/net/ipv4/tcp_fastopen", "w");
    if (fp == NULL) {
        perror("  [错误] 无法写入 tcp_fastopen (需要 root 权限)");
        printf("  提示: 使用 sudo 运行本程序\n");
        return;
    }

    fprintf(fp, "3");
    fclose(fp);

    printf("  [OK] TFO 已启用\n");
    printf("  永久生效: 将 'net.ipv4.tcp_fastopen = 3' 加入 /etc/sysctl.conf\n");
}

/* ------------------------------------------------------------
 * 3. 解释三次握手优化
 *
 * 传统三次握手:
 *   客户端                 服务端
 *     |----- SYN --------->|
 *     |<---- SYN-ACK ------|
 *     |----- ACK --------->|
 *     |====== 数据 =======>|
 *
 * TFO 第一次握手即可带数据:
 *   客户端                 服务端
 *     |-- SYN + 数据 ----->|
 *     |<---- SYN-ACK ------|
 *     |----- ACK --------->|
 *     |====== 响应数据 ===>|
 * ------------------------------------------------------------ */
static void explain_handshake_optimization(void)
{
    print_section("3. 三次握手优化原理");

    printf("【传统 TCP 连接】\n");
    printf("  客户端                              服务端\n");
    printf("    |------- SYN ------------------->|  (第1次RTT)\n");
    printf("    |<------ SYN-ACK ---------------|  (第2次RTT)\n");
    printf("    |------- ACK ------------------>|  (第3次RTT)\n");
    printf("    |======= HTTP请求数据 ==========>|  (第4次RTT)\n");
    printf("  总耗时: 至少 2 个 RTT 才能开始传数据\n\n");

    printf("【TCP Fast Open 连接】\n");
    printf("  客户端                              服务端\n");
    printf("    |------- SYN + Cookie + 数据 --->|  (第1次RTT)\n");
    printf("    |<------ SYN-ACK + 响应数据 -----|  (第2次RTT)\n");
    printf("    |------- ACK ------------------>|\n");
    printf("  总耗时: 只需 1 个 RTT 即可获取响应\n\n");

    printf("【性能收益】\n");
    printf("  - 延迟节省: 减少 1 个 RTT（约 10-100ms）\n");
    printf("  - 高频短连接场景收益最大（如 HTTP API 调用）\n");
    printf("  - 适用场景: REST API、数据库短连接、DNS 查询\n");
}

/* ------------------------------------------------------------
 * 4. TFO Cookie 机制详解
 *
 * Cookie 是服务端生成的安全令牌，防止放大攻击:
 *   1. 客户端发送 SYN 时附带空的 TFO cookie 请求
 *   2. 服务端用密钥加密客户端信息，生成 Cookie
 *   3. 后续客户端 SYN 包携带此 Cookie
 *   4. 服务端验证 Cookie 有效性，验证通过则接受数据
 * ------------------------------------------------------------ */
static void explain_tfo_cookie(void)
{
    print_section("4. TFO Cookie 机制");

    printf("【Cookie 生成】\n");
    printf("  服务端: Cookie = HMAC(密钥, 客户端IP + 端口 + 时间戳)\n");
    printf("  客户端: Cookie 缓存在本地，下次连接时带上\n\n");

    printf("【Cookie 生命周期】\n");
    printf("  - 有效期: 通常 2 天\n");
    printf("  - 每个 Cookie 对应特定 (IP, Port) 对\n");
    printf("  - 服务端重启后 Cookie 失效，需重新协商\n\n");

    printf("【安全保护】\n");
    printf("  - Cookie 无法伪造（需要服务端密钥）\n");
    printf("  - 服务端验证后才接受 SYN+Data，防止放大攻击\n");
    printf("  - 限制同一 IP 的并发 TFO 连接数\n\n");

    printf("【首次连接 vs 后续连接】\n");
    printf("  首次连接:\n");
    printf("    SYN + (空Cookie) + Data  →  服务端返回 SYN-ACK + Cookie\n");
    printf("    客户端保存 Cookie\n\n");
    printf("  后续连接:\n");
    printf("    SYN + (有效Cookie) + Data  →  服务端验证通过，立即处理数据\n");
}

/* ------------------------------------------------------------
 * 5. 用 curl 测试 TFO
 * ------------------------------------------------------------ */
static void demo_curl_tfo(void)
{
    print_section("5. 使用 curl 测试 TFO");

    printf("【检查 curl 是否支持 TFO】\n");
    printf("  curl --version | grep -i fastopen\n\n");

    printf("【TFO 客户端测试命令】\n");
    printf("  # 首次请求 (需要协商 Cookie，会有额外 RTT)\n");
    printf("  curl -v --tcp-fastopen http://example.com/\n\n");

    printf("  # 后续请求 (复用 Cookie，节省 1 个 RTT)\n");
    printf("  curl -v --tcp-fastopen http://example.com/\n\n");

    printf("【TFO 服务端测试命令】\n");
    printf("  # 启动支持 TFO 的服务端 (示例端口 8080)\n");
    printf("  sysctl -w net.ipv4.tcp_fastopen=3\n");
    printf("  nc -l -p 8080 &\n");
    printf("  # 客户端使用 TFO 连接\n");
    printf("  curl --tcp-fastopen http://localhost:8080/\n\n");

    printf("【验证 TFO 是否生效】\n");
    printf("  # 查看 TFO 连接统计\n");
    printf("  cat /proc/net/netstat | grep TcpExtStats\n");
    printf("  # 或查看具体计数器\n");
    printf("  grep TCPOFO /proc/net/netstat\n");
}

/* ------------------------------------------------------------
 * 主函数
 * ------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║       TCP Fast Open (TFO) 演示程序        ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  TCP Fast Open 允许在三次握手的第一次     ║\n");
    printf("║  SYN 包中携带数据，从而节省 1 个 RTT      ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    /* 默认执行所有步骤 */
    check_tfo_status();
    enable_tfo();
    explain_handshake_optimization();
    explain_tfo_cookie();
    demo_curl_tfo();

    print_section("总结");
    printf("TCP Fast Open 是一种 TCP 协议扩展，通过以下方式提升性能:\n");
    printf("  1. 在 SYN 包中携带初始数据，无需等待握手完成\n");
    printf("  2. 使用 Cookie 机制确保安全性\n");
    printf("  3. 适合高频短连接场景\n\n");
    printf("注意事项:\n");
    printf("  - 需要内核和应用程序同时支持\n");
    printf("  - 需要 root 权限修改 sysctl 参数\n");
    printf("  - 不适合首次连接（需要协商 Cookie）\n\n");

    return 0;
}
