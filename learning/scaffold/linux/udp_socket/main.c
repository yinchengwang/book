/**
 * UDP 高性能套接字示例程序
 *
 * 演示 UDP 套接字的高级特性：
 * - SO_REUSEPORT 多进程负载均衡
 * - UDP GRO (Generic Receive Offload)
 * - 批量数据处理
 * - /proc/net/udp 统计信息读取
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>

#define BUFFER_SIZE     4096
#define BATCH_SIZE      64          /* 批量处理的最大数据包数量 */
#define SERVER_PORT     8888

/* 数据包结构体，用于批量处理 */
typedef struct {
    char    data[BUFFER_SIZE];
    int     len;
    struct  sockaddr_in src_addr;
    socklen_t addr_len;
} udp_packet_t;

/* UDP GRO 选项控制 */
typedef struct {
    int udp_gro_enabled;           /* UDP GRO 是否启用 */
    int batch_size;                 /* 批量处理大小 */
    int reuseport_enabled;          /* SO_REUSEPORT 是否启用 */
} udp_options_t;

/**
 * 创建 UDP 套接字
 * @param port 绑定端口，0 表示任意端口
 * @param opts UDP 选项
 * @return 套接字文件描述符，失败返回 -1
 */
int create_udp_socket(int port, udp_options_t *opts)
{
    int sockfd;
    int optval = 1;

    /* 创建 UDP 套接字 */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket 创建失败");
        return -1;
    }

    /* 设置 SO_REUSEADDR，允许地址复用 */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt SO_REUSEADDR 失败");
        close(sockfd);
        return -1;
    }

    /* 设置 SO_REUSEPORT，支持多进程/多线程负载均衡 */
    if (opts && opts->reuseport_enabled) {
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
            perror("setsockopt SO_REUSEPORT 失败（内核可能不支持）");
            /* 不算致命错误，继续执行 */
        } else {
            printf("[INFO] SO_REUSEPORT 已启用\n");
        }
    }

    /* 尝试启用 UDP GRO（需要内核支持） */
    if (opts && opts->udp_gro_enabled) {
        /* UDP GRO 在某些系统上通过 UDP_GRO 套接字选项启用 */
        int udp_gro = 1;
#ifdef UDP_GRO
        if (setsockopt(sockfd, IPPROTO_UDP, UDP_GRO, &udp_gro, sizeof(udp_gro)) < 0) {
            printf("[INFO] UDP GRO 启用失败（内核可能不支持）\n");
        } else {
            printf("[INFO] UDP GRO 已启用\n");
        }
#else
        printf("[INFO] 编译时不支持 UDP_GRO 选项\n");
#endif
    }

    /* 绑定地址 */
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind 失败");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/**
 * 批量接收 UDP 数据包
 * @param sockfd 套接字描述符
 * @param packets 接收缓冲区数组
 * @param max_packets 最大接收数量
 * @return 实际接收的数据包数量
 */
int batch_recvfrom(int sockfd, udp_packet_t *packets, int max_packets)
{
    int count = 0;

    /* 非阻塞接收循环 */
    for (int i = 0; i < max_packets; i++) {
        ssize_t n = recvfrom(sockfd, packets[i].data, BUFFER_SIZE - 1,
                             MSG_DONTWAIT,
                             (struct sockaddr *)&packets[i].src_addr,
                             &packets[i].addr_len);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* 没有更多数据 */
                break;
            }
            perror("recvfrom 错误");
            break;
        }

        packets[i].len = (int)n;
        packets[i].data[n] = '\0';
        count++;
    }

    return count;
}

/**
 * 读取 /proc/net/udp 统计信息
 */
void read_udp_stats(void)
{
    FILE *fp = fopen("/proc/net/udp", "r");
    if (!fp) {
        printf("[WARN] 无法打开 /proc/net/udp: %s\n", strerror(errno));
        return;
    }

    char line[512];
    printf("\n=== /proc/net/udp 统计信息 ===\n");

    /* 跳过标题行 */
    if (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }

    /* 读取数据行 */
    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < 5) {
        printf("%s", line);
        count++;
    }

    if (count == 0) {
        printf("（无可用数据）\n");
    }

    fclose(fp);
}

/**
 * 打印 UDP 缓冲区信息
 */
void print_udp_buffer_info(int sockfd)
{
    int recvbuf, sendbuf;

    socklen_t optlen = sizeof(recvbuf);

    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvbuf, &optlen) == 0) {
        printf("[INFO] SO_RCVBUF: %d bytes (%.1f KB)\n", recvbuf, recvbuf / 1024.0);
    }

    optlen = sizeof(sendbuf);
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, &optlen) == 0) {
        printf("[INFO] SO_SNDBUF: %d bytes (%.1f KB)\n", sendbuf, sendbuf / 1024.0);
    }
}

/**
 * 服务器模式：批量接收并处理数据
 */
void run_server(int port, udp_options_t *opts)
{
    int sockfd = create_udp_socket(port, opts);
    if (sockfd < 0) {
        exit(EXIT_FAILURE);
    }

    printf("[服务器] UDP 套接字已创建，监听端口 %d\n", port);
    printf("[服务器] 批量大小: %d\n", opts->batch_size);

    /* 设置非阻塞模式，支持批量接收 */
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    print_udp_buffer_info(sockfd);

    udp_packet_t packets[BATCH_SIZE];
    uint64_t total_packets = 0;

    printf("\n[服务器] 等待数据...\n");

    while (1) {
        int count = batch_recvfrom(sockfd, packets, opts->batch_size);

        if (count > 0) {
            total_packets += count;
            printf("\r[服务器] 收到 %d 个数据包 (总计: %lu)", count, total_packets);
            fflush(stdout);

            /* 模拟处理：简单回显 */
            for (int i = 0; i < count; i++) {
                char *client_ip = inet_ntoa(packets[i].src_addr.sin_addr);
                int client_port = ntohs(packets[i].src_addr.sin_port);
                printf("\n    [%s:%d] %.*s",
                       client_ip, client_port,
                       packets[i].len > 64 ? 64 : packets[i].len,
                       packets[i].data);
            }
        }

        /* 短暂休眠，避免 CPU 100% */
        usleep(1000);
    }

    close(sockfd);
}

/**
 * 客户端模式：发送测试数据
 */
void run_client(const char *server_ip, int port, int count)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket 创建失败");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    printf("[客户端] 发送 %d 个数据包到 %s:%d\n", count, server_ip, port);

    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "Hello from UDP client - PID: %d", getpid());

    for (int i = 0; i < count; i++) {
        ssize_t sent = sendto(sockfd, msg, strlen(msg), 0,
                               (struct sockaddr *)&servaddr, sizeof(servaddr));
        if (sent < 0) {
            perror("sendto 失败");
            break;
        }
        printf("[客户端] 已发送数据包 #%d\n", i + 1);
        usleep(10000);  /* 10ms 间隔 */
    }

    printf("[客户端] 发送完成\n");
    close(sockfd);
}

/**
 * 打印使用说明
 */
void print_usage(const char *prog)
{
    printf("用法:\n");
    printf("  %s server [port]     - 以服务器模式运行\n", prog);
    printf("  %s client ip port [n]- 以客户端模式运行，发送 n 个数据包\n", prog);
    printf("\n示例:\n");
    printf("  %s server 8888       - 启动服务器，监听 8888 端口\n", prog);
    printf("  %s client 127.0.0.1 8888 10 - 发送 10 个测试数据包\n", prog);
}

int main(int argc, char *argv[])
{
    /* 默认选项 */
    udp_options_t opts = {
        .udp_gro_enabled = 1,
        .batch_size = BATCH_SIZE,
        .reuseport_enabled = 1
    };

    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "server") == 0) {
        int port = (argc > 2) ? atoi(argv[2]) : SERVER_PORT;
        run_server(port, &opts);
    } else if (strcmp(argv[1], "client") == 0) {
        if (argc < 4) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        const char *ip = argv[2];
        int port = atoi(argv[3]);
        int count = (argc > 4) ? atoi(argv[4]) : 5;
        run_client(ip, port, count);
        read_udp_stats();
    } else if (strcmp(argv[1], "stats") == 0) {
        read_udp_stats();
    } else {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
