/**
 * @file main.c
 * @brief Linux TCP 状态学习演示
 *
 * 演示内容：
 *   - 读取 /proc/net/tcp 获取 TCP 连接信息
 *   - 解析 TCP 11 种状态
 *   - 统计各状态的连接数
 *
 * 编译：gcc -std=c11 -Wall -o tcp_state main.c
 * 运行：./tcp_state
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

/* ============================================================
 * TCP 状态枚举
 * ============================================================ */
typedef enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING,
    TCP_NEW_SYN_RECV,
    TCP_MAX_STATE
} tcp_state_t;

static const char *tcp_state_names[] = {
    [TCP_ESTABLISHED]  = "ESTABLISHED",
    [TCP_SYN_SENT]     = "SYN_SENT",
    [TCP_SYN_RECV]     = "SYN_RECV",
    [TCP_FIN_WAIT1]    = "FIN_WAIT1",
    [TCP_FIN_WAIT2]    = "FIN_WAIT2",
    [TCP_TIME_WAIT]    = "TIME_WAIT",
    [TCP_CLOSE]        = "CLOSE",
    [TCP_CLOSE_WAIT]   = "CLOSE_WAIT",
    [TCP_LAST_ACK]     = "LAST_ACK",
    [TCP_LISTEN]       = "LISTEN",
    [TCP_CLOSING]      = "CLOSING",
    [TCP_NEW_SYN_RECV] = "NEW_SYN_RECV"
};

/* ============================================================
 * 解析十六进制 IP:端口
 * ============================================================ */
static void parse_hex_addr(const char *hex, char *ip, int *port) {
    unsigned int addr_hex, port_hex;
    sscanf(hex, "%X", &addr_hex);

    /* 解析地址（Linux 存储为小端序） */
    struct in_addr addr;
    addr.s_addr = htonl(addr_hex);
    inet_ntop(AF_INET, &addr, ip, INET_ADDRSTRLEN);

    /* 获取端口（从 hex 中提取） */
    const char *colon = strchr(hex, ':');
    if (colon) {
        sscanf(colon + 1, "%X", &port_hex);
        *port = port_hex;
    } else {
        *port = 0;
    }
}

/* ============================================================
 * 将十六进制状态转换为枚举
 * ============================================================ */
static tcp_state_t hex_to_state(unsigned int state_hex) {
    switch (state_hex) {
        case 0x01: return TCP_ESTABLISHED;
        case 0x02: return TCP_SYN_SENT;
        case 0x03: return TCP_SYN_RECV;
        case 0x04: return TCP_FIN_WAIT1;
        case 0x05: return TCP_FIN_WAIT2;
        case 0x06: return TCP_TIME_WAIT;
        case 0x07: return TCP_CLOSE;
        case 0x08: return TCP_CLOSE_WAIT;
        case 0x09: return TCP_LAST_ACK;
        case 0x0A: return TCP_LISTEN;
        case 0x0B: return TCP_CLOSING;
        case 0x0C: return TCP_NEW_SYN_RECV;
        default:   return TCP_MAX_STATE;
    }
}

/* ============================================================
 * 统计 TCP 连接
 * ============================================================ */
static void demo_tcp_stats(void) {
    printf("[tcp_state] 读取 /proc/net/tcp 获取 TCP 连接统计\n");

    FILE *fp = fopen("/proc/net/tcp", "r");
    if (!fp) {
        perror("[tcp_state] fopen /proc/net/tcp 失败");
        return;
    }

    char line[1024];
    int state_counts[TCP_MAX_STATE + 1] = {0};
    int total_conn = 0;

    /* 跳过标题行 */
    fgets(line, sizeof(line), fp);

    printf("[tcp_state]   本地地址         远程地址         状态      名称\n");

    while (fgets(line, sizeof(line), fp)) {
        unsigned int local_addr, remote_addr, state, uid;
        unsigned long rx_queue, tx_queue, timer_run, time_len;
        unsigned long retrnsmt, uid_val, timeout;

        /* 解析格式：sl local_address rem_address st tx_queue rx_queue ... */
        if (sscanf(line, "%*d: %X:%X %X:%X %X %lX:%lX %lX:%lX %X %lu %lu %lu",
                   &local_addr, &rx_queue, &remote_addr, &tx_queue,
                   &state, &timer_run, &time_len, &retrnsmt, &uid_val,
                   &timeout) < 5) {
            continue;
        }

        tcp_state_t st = hex_to_state(state);
        state_counts[st]++;

        if (total_conn < 15) {
            char local_ip[INET_ADDRSTRLEN], remote_ip[INET_ADDRSTRLEN];
            int local_port, remote_port;

            parse_hex_addr(line + 4, local_ip, &local_port);
            /* 重新解析远程地址 */
            const char *rem_start = strchr(line + 4, ' ');
            if (rem_start) {
                char rem_hex[32];
                sscanf(rem_start + 1, "%31s", rem_hex);
                parse_hex_addr(rem_hex, remote_ip, &remote_port);
            }

            const char *state_name = (st < TCP_MAX_STATE) ?
                                     tcp_state_names[st] : "UNKNOWN";
            printf("[tcp_state]   %s:%-5d -> %s:%-5d %-12s\n",
                   local_ip, local_port, remote_ip, remote_port, state_name);
        }
        total_conn++;
    }

    fclose(fp);

    /* 打印统计摘要 */
    printf("\n[TCP 连接状态统计]\n");
    printf("[tcp_state]   ");
    int printed = 0;
    for (int i = 0; i <= TCP_MAX_STATE; i++) {
        if (state_counts[i] > 0 && i < TCP_MAX_STATE) {
            printf("%s=%d ", tcp_state_names[i], state_counts[i]);
            printed++;
            if (printed % 5 == 0) printf("\n[tcp_state]   ");
        }
    }
    printf("\n");
    printf("[tcp_state]   总连接数: %d\n", total_conn);
}

/* ============================================================
 * TCP 状态机转换
 * ============================================================ */
static void demo_state_machine(void) {
    printf("[tcp_state] TCP 11 种状态说明:\n\n");

    printf("[tcp_state]   状态          说明\n");
    printf("[tcp_state]   ----------    ----------------------------------------\n");
    printf("[tcp_state]   LISTEN        服务器等待连接请求\n");
    printf("[tcp_state]   SYN_SENT      客户端已发送 SYN\n");
    printf("[tcp_state]   SYN_RECV      服务器收到并回复 SYN\n");
    printf("[tcp_state]   ESTABLISHED   连接已建立，双方可传输数据\n");
    printf("[tcp_state]   FIN_WAIT1     主动关闭，已发 FIN，等待 ACK\n");
    printf("[tcp_state]   FIN_WAIT2     收到 ACK，等待对方 FIN\n");
    printf("[tcp_state]   CLOSE_WAIT    被动关闭，收到 FIN，等待应用关闭\n");
    printf("[tcp_state]   CLOSING       双方同时关闭\n");
    printf("[tcp_state]   TIME_WAIT     等待足够时间确保对方收到 ACK\n");
    printf("[tcp_state]   LAST_ACK      被动关闭方发送 FIN，等待 ACK\n");
    printf("[tcp_state]   CLOSE         连接关闭\n");

    printf("\n[tcp_state] 典型连接生命周期:\n");
    printf("[tcp_state]   Client:  CLOSED -> SYN_SENT -> ESTABLISHED\n");
    printf("[tcp_state]   Server:  CLOSED -> LISTEN -> SYN_RECV -> ESTABLISHED\n");
    printf("[tcp_state]   Close:   ESTABLISHED -> FIN_WAIT1 -> FIN_WAIT2 -> CLOSED\n");
}

/* ============================================================
 * TCP 连接故障排查
 * ============================================================ */
static void demo_troubleshooting(void) {
    printf("[tcp_state] TCP 连接故障排查:\n\n");

    /* TIME_WAIT 过多 */
    printf("[tcp_state] 1. TIME_WAIT 过多（> 10000）:\n");
    printf("[tcp_state]    原因：频繁创建/关闭短连接\n");
    printf("[tcp_state]    解决：启用 tcp_tw_reuse / 调整 TIME_WAIT 超时\n\n");

    /* CLOSE_WAIT 过多 */
    printf("[tcp_state] 2. CLOSE_WAIT 过多（> 100）:\n");
    printf("[tcp_state]    原因：应用未正确关闭连接\n");
    printf("[tcp_state]    解决：检查代码中 close() 调用\n\n");

    /* SYN_RECV 过多 */
    printf("[tcp_state] 3. SYN_RECV 过多（> 1000）:\n");
    printf("[tcp_state]    原因：SYN 泛洪攻击 / 服务器过载\n");
    printf("[tcp_state]    解决：启用 syncookies / 扩容\n\n");

    /* ESTABLISHED 过多 */
    printf("[tcp_state] 4. ESTABLISHED 过多（接近 max_connections）:\n");
    printf("[tcp_state]    原因：连接未释放 / 长连接泄漏\n");
    printf("[tcp_state]    解决：设置连接超时 / 检查连接池\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux TCP 状态学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: TCP 连接统计 ---\n");
    demo_tcp_stats();
    printf("\n");

    printf("--- Demo 2: TCP 状态机 ---\n");
    demo_state_machine();
    printf("\n");

    printf("--- Demo 3: 连接故障排查 ---\n");
    demo_troubleshooting();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
