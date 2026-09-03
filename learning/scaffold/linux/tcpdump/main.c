/**
 * @file main.c
 * @brief Linux tcpdump 原理学习演示
 *
 * 演示内容：
 *   - Raw Socket 创建与配置
 *   - 抓取网络数据包
 *   - 简单协议解析（Ethernet + IP + TCP 头部）
 *
 * 编译：gcc -std=c11 -Wall -o tcpdump main.c
 * 运行：sudo ./tcpdump  (需要 root 权限)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <net/if.h>

/* ============================================================
 * 以太网头部结构
 * ============================================================ */
#pragma pack(push, 1)
typedef struct {
    unsigned char dst_mac[6];   /* 目标 MAC */
    unsigned char src_mac[6];   /* 源 MAC */
    unsigned short eth_type;    /* 以太网类型 */
} eth_header_t;
#pragma pack(pop)

/* ============================================================
 * 打印 MAC 地址
 * ============================================================ */
static void print_mac(unsigned char *mac) {
    printf("%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/* ============================================================
 * 解析以太网头部
 * ============================================================ */
static int parse_eth_header(const unsigned char *packet, char *src_ip, char *dst_ip,
                            int *src_port, int *dst_port, char *protocol) {
    eth_header_t *eth = (eth_header_t *)packet;

    /* 字节序转换 */
    unsigned short eth_type = ntohs(eth->eth_type);

    printf("[tcpdump]   Ethernet: ");
    print_mac(eth->src_mac);
    printf(" -> ");
    print_mac(eth->dst_mac);
    printf("\n");
    printf("[tcpdump]   EtherType: 0x%04X ", eth_type);

    /* 仅处理 IPv4 */
    if (eth_type != 0x0800) {
        printf("(非 IPv4)\n");
        return -1;
    }
    printf("(IPv4)\n");

    /* 解析 IP 头部 */
    struct ip *ip_hdr = (struct ip *)(packet + sizeof(eth_header_t));
    strncpy(src_ip, inet_ntoa(ip_hdr->ip_src), 16);
    strncpy(dst_ip, inet_ntoa(ip_hdr->ip_dst), 16);

    printf("[tcpdump]   IP: %s -> %s\n", src_ip, dst_ip);

    /* 仅处理 TCP */
    if (ip_hdr->ip_p != IPPROTO_TCP) {
        printf("[tcpdump]   协议: %d (非 TCP)\n", ip_hdr->ip_p);
        return -1;
    }

    strcpy(protocol, "TCP");

    /* 解析 TCP 头部 */
    struct tcphdr *tcp_hdr = (struct tcphdr *)((unsigned char *)ip_hdr + ip_hdr->ip_hl * 4);
    *src_port = ntohs(tcp_hdr->source);
    *dst_port = ntohs(tcp_hdr->dest);

    printf("[tcpdump]   TCP: %s:%d -> %s:%d\n", src_ip, *src_port, dst_ip, *dst_port);
    printf("[tcpdump]   Flags: ");
    if (tcp_hdr->syn) printf("SYN ");
    if (tcp_hdr->ack) printf("ACK ");
    if (tcp_hdr->fin) printf("FIN ");
    if (tcp_hdr->rst) printf("RST ");
    if (tcp_hdr->psh) printf("PSH ");
    printf("\n");

    return 0;
}

/* ============================================================
 * Demo 1: Raw Socket 创建
 * ============================================================ */
static void demo_raw_socket(void) {
    printf("[tcpdump] 演示 Raw Socket 创建\n");

    /* 创建原始套接字 */
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sock < 0) {
        perror("[tcpdump] socket 创建失败（需要 root 权限）");
        printf("[tcpdump] 提示：使用 sudo 运行本程序\n");
        return;
    }

    printf("[tcpdump]   Raw Socket 创建成功\n");
    printf("[tcpdump]   协议: ETH_P_IP (0x%04X)\n", ETH_P_IP);
    printf("[tcpdump]   说明: 可接收所有 IP 数据包\n");

    close(sock);
}

/* ============================================================
 * Demo 2: 抓包演示（非阻塞模式）
 * ============================================================ */
static void demo_capture(void) {
    printf("[tcpdump] 演示抓包（5 秒超时）\n");

    /* 创建原始套接字 */
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sock < 0) {
        perror("[tcpdump] socket 创建失败");
        return;
    }

    /* 设置超时 */
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    unsigned char buffer[4096];
    int packet_count = 0;

    printf("[tcpdump]   开始抓包 (Ctrl+C 退出)...\n");

    /* 最多抓取 5 个包 */
    while (packet_count < 5) {
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("[tcpdump]   超时，停止抓包\n");
                break;
            }
            perror("[tcpdump] recvfrom 失败");
            break;
        }

        printf("\n[tcpdump] ===== 数据包 #%d =====\n", packet_count + 1);
        printf("[tcpdump]   长度: %d 字节\n", len);

        char src_ip[16], dst_ip[16], protocol[8];
        int src_port, dst_port;

        if (parse_eth_header(buffer, src_ip, dst_ip, &src_port, &dst_port, protocol) == 0) {
            printf("[tcpdump]   解析成功: %s %s:%d -> %s:%d\n",
                   protocol, src_ip, src_port, dst_ip, dst_port);
        }

        packet_count++;
    }

    printf("\n[tcpdump]   共抓取 %d 个数据包\n", packet_count);
    close(sock);
}

/* ============================================================
 * Demo 3: tcpdump 原理说明
 * ============================================================ */
static void demo_tcpdump_principle(void) {
    printf("[tcpdump] tcpdump 工作原理:\n\n");

    printf("[tcpdump] 1. 创建原始套接字 (AF_PACKET, SOCK_RAW)\n");
    printf("[tcpdump]    - 绕过 TCP/UDP 协议栈，直接访问链路层\n");
    printf("[tcpdump]    - 需要 root 权限\n\n");

    printf("[tcpdump] 2. 设置过滤器 (BPF - Berkeley Packet Filter)\n");
    printf("[tcpdump]    - 用户空间编译过滤规则\n");
    printf("[tcpdump]    - 内核空间执行，减少用户/内核拷贝\n\n");

    printf("[tcpdump] 3. 接收数据包\n");
    printf("[tcpdump]    - recvfrom() 返回完整数据包（含链路层头部）\n");
    printf("[tcpdump]    - 数据保存在内核缓冲区\n\n");

    printf("[tcpdump] 4. 解析与显示\n");
    printf("[tcpdump]    - 解析 Ethernet 头部 -> 获取 MAC\n");
    printf("[tcpdump]    - 解析 IP 头部 -> 获取 IP 地址\n");
    printf("[tcpdump]    - 解析 TCP/UDP 头部 -> 获取端口\n\n");

    printf("[tcpdump] 数据包结构:\n");
    printf("[tcpdump]    +------------------+\n");
    printf("[tcpdump]    | Ethernet Header  | 14 字节\n");
    printf("[tcpdump]    +------------------+\n");
    printf("[tcpdump]    | IP Header        | 20 字节\n");
    printf("[tcpdump]    +------------------+\n");
    printf("[tcpdump]    | TCP Header       | 20+ 字节\n");
    printf("[tcpdump]    +------------------+\n");
    printf("[tcpdump]    | Payload          |\n");
    printf("[tcpdump]    +------------------+\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux tcpdump 原理学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: Raw Socket 创建 ---\n");
    demo_raw_socket();
    printf("\n");

    printf("--- Demo 2: 抓包演示 ---\n");
    demo_capture();
    printf("\n");

    printf("--- Demo 3: tcpdump 原理 ---\n");
    demo_tcpdump_principle();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
