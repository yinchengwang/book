/**
 * @file main.c
 * @brief 网络诊断接口演示
 *
 * 演示 /proc/net/snmp、/proc/net/tcp 等接口
 * 展示网络统计和连接状态分析
 *
 * 编译：gcc -std=c11 -o net_diag main.c
 * 运行：./net_diag
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>

/* SNMP 统计结构 */
typedef struct {
    /* IP 层 */
    unsigned long ip_forwarding;
    unsigned long ip_in_receives;
    unsigned long ip_in_hdr_errors;
    unsigned long ip_out_requests;

    /* TCP 层 */
    unsigned long tcp_active_opens;
    unsigned long tcp_passive_opens;
    unsigned long tcp_in_segs;
    unsigned long tcp_out_segs;
    unsigned long tcp_retrans_segs;
    unsigned long tcp_in_errs;
    unsigned long tcp_curr_estab;

    /* UDP 层 */
    unsigned long udp_in_datagrams;
    unsigned long udp_out_datagrams;
    unsigned long udp_no_ports;
    unsigned long udp_in_errors;
} SnmpStats;

/* 解析 /proc/net/snmp */
static int parse_proc_net_snmp(SnmpStats *stats) {
    FILE *fp = fopen("/proc/net/snmp", "r");
    if (!fp) {
        perror("fopen /proc/net/snmp");
        return -1;
    }

    memset(stats, 0, sizeof(*stats));
    char line[1024];

    /* Ip: 行解析 */
    while (fgets(line, sizeof(line), fp)) {
        /* 跳过标题行 */
        if (strncmp(line, "Ip:", 3) != 0) continue;

        char *tok = strtok(line + 3, " \t\n");
        int col = 0;
        while (tok) {
            unsigned long val = strtoul(tok, NULL, 10);
            switch (col) {
                case 0: stats->ip_forwarding = val; break;
                case 2: stats->ip_in_receives = val; break;
                case 3: stats->ip_in_hdr_errors = val; break;
                case 8: stats->ip_out_requests = val; break;
            }
            col++;
            tok = strtok(NULL, " \t\n");
        }
        break;
    }

    /* Tcp: 行解析 */
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Tcp:", 4) != 0) continue;

        char *tok = strtok(line + 4, " \t\n");
        int col = 0;
        while (tok) {
            unsigned long val = strtoul(tok, NULL, 10);
            switch (col) {
                case 4: stats->tcp_active_opens = val; break;
                case 5: stats->tcp_passive_opens = val; break;
                case 6: stats->tcp_in_segs = val; break;
                case 10: stats->tcp_out_segs = val; break;
                case 11: stats->tcp_retrans_segs = val; break;
                case 13: stats->tcp_in_errs = val; break;
                case 9: stats->tcp_curr_estab = val; break;
            }
            col++;
            tok = strtok(NULL, " \t\n");
        }
        break;
    }

    /* Udp: 行解析 */
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Udp:", 4) != 0) continue;

        char *tok = strtok(line + 4, " \t\n");
        int col = 0;
        while (tok) {
            unsigned long val = strtoul(tok, NULL, 10);
            switch (col) {
                case 0: stats->udp_in_datagrams = val; break;
                case 3: stats->udp_out_datagrams = val; break;
                case 1: stats->udp_no_ports = val; break;
                case 2: stats->udp_in_errors = val; break;
            }
            col++;
            tok = strtok(NULL, " \t\n");
        }
        break;
    }

    fclose(fp);
    return 0;
}

/* 打印 SNMP 统计 */
static void print_snmp_stats(const SnmpStats *s) {
    printf("\n=== TCP 连接统计 ===\n");
    printf("活跃连接数:       %lu\n", s->tcp_curr_estab);
    printf("主动打开:         %lu\n", s->tcp_active_opens);
    printf("被动打开:         %lu\n", s->tcp_passive_opens);
    printf("收到段:           %lu\n", s->tcp_in_segs);
    printf("发出段:           %lu\n", s->tcp_out_segs);
    printf("重传段:           %lu\n", s->tcp_retrans_segs);
    printf("输入错误:         %lu\n", s->tcp_in_errs);

    printf("\n=== UDP 统计 ===\n");
    printf("收到数据报:       %lu\n", s->udp_in_datagrams);
    printf("发出数据报:       %lu\n", s->udp_out_datagrams);
    printf("无端口:           %lu\n", s->udp_no_ports);
    printf("输入错误:         %lu\n", s->udp_in_errors);

    printf("\n=== IP 层统计 ===\n");
    printf("收到包:           %lu\n", s->ip_in_receives);
    printf("转发包:           %lu\n", s->ip_forwarding);
    printf("头部错误:         %lu\n", s->ip_in_hdr_errors);
    printf("发出包:           %lu\n", s->ip_out_requests);
}

/* 解析 /proc/net/tcp 显示连接 */
static void show_tcp_connections(void) {
    printf("\n=== TCP 连接表 (/proc/net/tcp 前10行) ===\n");
    FILE *fp = fopen("/proc/net/tcp", "r");
    if (!fp) return;

    char line[256];
    int count = 0;
    /* 跳过标题行 */
    fgets(line, sizeof(line), fp);

    printf("%-5s %-30s %-30s %-12s\n",
           "序号", "本地地址", "远程地址", "状态");
    printf("%-5s %-30s %-30s %-12s\n",
           "---", "------", "------", "---");

    while (fgets(line, sizeof(line), fp) && count < 10) {
        char local_addr[40], remote_addr[40];
        int state;
        unsigned int local_ip[4], remote_ip[4];
        int local_port, remote_port;

        if (sscanf(line, "%*d: %2x%2x%2x%2x:%4x %2x%2x%2x%2x:%4x %2x",
                   &local_ip[0], &local_ip[1], &local_ip[2], &local_ip[3],
                   &local_port,
                   &remote_ip[0], &remote_ip[1], &remote_ip[2], &remote_ip[3],
                   &remote_port, &state) == 11) {

            snprintf(local_addr, sizeof(local_addr), "%d.%d.%d.%d:%d",
                     local_ip[0], local_ip[1], local_ip[2], local_ip[3], local_port);
            snprintf(remote_addr, sizeof(remote_addr), "%d.%d.%d.%d:%d",
                     remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3], remote_port);

            const char *state_str;
            switch (state) {
                case 1: state_str = "ESTABLISHED"; break;
                case 2: state_str = "SYN_SENT"; break;
                case 3: state_str = "SYN_RECV"; break;
                case 4: state_str = "FIN_WAIT1"; break;
                case 5: state_str = "FIN_WAIT2"; break;
                case 6: state_str = "TIME_WAIT"; break;
                case 7: state_str = "CLOSE"; break;
                case 8: state_str = "CLOSE_WAIT"; break;
                case 9: state_str = "LAST_ACK"; break;
                case 10: state_str = "LISTEN"; break;
                case 11: state_str = "CLOSING"; break;
                default: state_str = "UNKNOWN"; break;
            }

            printf("%-5d %-30s %-30s %-12s\n",
                   count + 1, local_addr, remote_addr, state_str);
            count++;
        }
    }
    fclose(fp);

    if (count == 0) printf("(无活动连接)\n");
}

/* 枚举网络接口 */
static void show_network_interfaces(void) {
    printf("\n=== 网络接口列表 ===\n");
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }

    printf("%-15s %-20s\n", "接口", "地址");
    printf("%-15s %-20s\n", "----", "---");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET) {
            char host[INET_ADDRSTRLEN];
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &sa->sin_addr, host, sizeof(host));
            printf("%-15s %-20s\n", ifa->ifa_name, host);
        }
    }

    freeifaddrs(ifaddr);
}

int main(void) {
    printf("===========================================\n");
    printf("  网络诊断接口演示\n");
    printf("===========================================\n");

    /* 1. SNMP 统计 */
    printf("\n=== 1. /proc/net/snmp 统计 ===");
    SnmpStats stats;
    if (parse_proc_net_snmp(&stats) == 0) {
        print_snmp_stats(&stats);
    }

    /* 2. TCP 连接状态 */
    printf("\n=== 2. TCP 连接状态 ===");
    show_tcp_connections();

    /* 3. 网络接口 */
    printf("\n=== 3. 网络接口 ===");
    show_network_interfaces();

    /* 4. 网络监控命令速查 */
    printf("\n\n=== 4. 网络监控命令速查 ===\n");
    printf("  # 查看所有 TCP 连接\n");
    printf("  netstat -t -n -4\n");
    printf("\n  # 查看 UDP 连接\n");
    printf("  netstat -u -n\n");
    printf("\n  # 实时网络流量（需要 ifstat）\n");
    printf("  ifstat -i eth0 1\n");
    printf("\n  # 查看套接字统计\n");
    printf("  cat /proc/net/sockstat\n");
    printf("\n  # 网络连接采样（需要 ss）\n");
    printf("  ss -t -a | wc -l\n");
    printf("\n  # 抓包分析（需要 tcpdump）\n");
    printf("  sudo tcpdump -i eth0 port 5432\n");

    /* 5. 诊断启示 */
    printf("\n\n=== 5. 网络性能诊断启示 ===\n");
    printf("  重传率高 → 网络丢包或拥塞\n");
    printf("  TIME_WAIT 多 → 短连接过多\n");
    printf("  CLOSE_WAIT 多 → 应用未正确关闭连接\n");
    printf("  LISTEN 过多 → 服务端口多/潜在攻击\n");

    printf("\n===========================================\n");
    printf("  网络诊断演示完成\n");
    printf("===========================================\n");

    return 0;
}
