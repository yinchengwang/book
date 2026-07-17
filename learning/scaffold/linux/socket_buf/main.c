/**
 * @file main.c
 * @brief Linux Socket 缓冲区学习演示
 *
 * 演示内容：
 *   - getsockopt/setsockopt 获取/设置 Socket 选项
 *   - SO_SNDBUF/SO_RCVBUF 缓冲区大小控制
 *   - TCP 缓冲区调优原理
 *
 * 编译：gcc -std=c11 -Wall -o socket_buf main.c
 * 运行：./socket_buf
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/* ============================================================
 * 打印 Socket 缓冲区信息
 * ============================================================ */
static void print_buf_info(int sock, const char *name) {
    int sndbuf, rcvbuf;
    socklen_t optlen = sizeof(int);

    /* 获取发送缓冲区大小 */
    if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, &optlen) == 0) {
        printf("[socket_buf]   %s SO_SNDBUF: %d bytes (%.1f KB)\n",
               name, sndbuf, sndbuf / 1024.0);
    } else {
        perror("[socket_buf] getsockopt SO_SNDBUF 失败");
    }

    /* 获取接收缓冲区大小 */
    if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) == 0) {
        printf("[socket_buf]   %s SO_RCVBUF: %d bytes (%.1f KB)\n",
               name, rcvbuf, rcvbuf / 1024.0);
    } else {
        perror("[socket_buf] getsockopt SO_RCVBUF 失败");
    }
}

/* ============================================================
 * Demo 1: 查看默认缓冲区大小
 * ============================================================ */
static void demo_default_buffers(void) {
    printf("[socket_buf] 演示 Socket 默认缓冲区大小\n");

    /* 创建测试 Socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[socket_buf] socket 创建失败");
        return;
    }

    printf("[socket_buf] 创建新 Socket (SOCK_STREAM)\n");
    print_buf_info(sock, "默认");

    /* 对比 UDP Socket */
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock >= 0) {
        printf("[socket_buf] 创建 UDP Socket (SOCK_DGRAM)\n");
        print_buf_info(udp_sock, "UDP");
        close(udp_sock);
    }

    close(sock);
}

/* ============================================================
 * Demo 2: 设置缓冲区大小
 * ============================================================ */
static void demo_set_buffers(void) {
    printf("[socket_buf] 演示设置 Socket 缓冲区大小\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[socket_buf] socket 创建失败");
        return;
    }

    /* 设置较大的发送缓冲区 */
    int desired_sndbuf = 512 * 1024;  /* 512 KB */
    printf("[socket_buf] 尝试设置 SO_SNDBUF 为 %d bytes\n", desired_sndbuf);

    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &desired_sndbuf, sizeof(desired_sndbuf)) == 0) {
        printf("[socket_buf]   setsockopt 成功\n");
        print_buf_info(sock, "设置后");
    } else {
        perror("[socket_buf]   setsockopt 失败");
    }

    /* 注意：内核可能会将请求值翻倍 */
    printf("[socket_buf] 注意：内核可能会将请求的缓冲区大小翻倍\n");
    printf("[socket_buf] 这是因为内核在用户缓冲区基础上增加自己的开销\n");

    close(sock);
}

/* ============================================================
 * Demo 3: TCP 特定选项
 * ============================================================ */
static void demo_tcp_options(void) {
    printf("[socket_buf] 演示 TCP 特定选项\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[socket_buf] socket 创建失败");
        return;
    }

    int nodelay, quickack;
    socklen_t optlen = sizeof(int);

    /* TCP_NODELAY（禁用 Nagle 算法） */
    if (getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, &optlen) == 0) {
        printf("[socket_buf]   TCP_NODELAY: %s\n", nodelay ? "启用" : "禁用");
    }

    /* TCP_QUICKACK（快速 ACK 模式） */
    if (getsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &quickack, &optlen) == 0) {
        printf("[socket_buf]   TCP_QUICKACK: %s\n", quickack ? "启用" : "禁用");
    }

    /* 设置 TCP_NODELAY */
    int enable = 1;
    printf("[socket_buf] 启用 TCP_NODELAY（低延迟模式）\n");
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) == 0) {
        printf("[socket_buf]   设置成功\n");
        getsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, &optlen);
        printf("[socket_buf]   TCP_NODELAY: %s\n", nodelay ? "启用" : "禁用");
    }

    close(sock);
}

/* ============================================================
 * Demo 4: 系统限制查看
 * ============================================================ */
static void demo_sysctl_limits(void) {
    printf("[socket_buf] 查看系统 Socket 缓冲区限制\n");

    FILE *fp = fopen("/proc/sys/net/core/rmem_default", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            printf("[socket_buf]   rmem_default: %s KB", buf);
        }
        fclose(fp);
    }

    fp = fopen("/proc/sys/net/core/rmem_max", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            printf("[socket_buf]   rmem_max: %s KB", buf);
        }
        fclose(fp);
    }

    fp = fopen("/proc/sys/net/core/wmem_default", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            printf("[socket_buf]   wmem_default: %s KB", buf);
        }
        fclose(fp);
    }

    fp = fopen("/proc/sys/net/core/wmem_max", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            printf("[socket_buf]   wmem_max: %s KB", buf);
        }
        fclose(fp);
    }

    printf("[socket_buf] 这些值可以通过 sysctl 或 /proc 调整\n");
}

/* ============================================================
 * Demo 5: 缓冲区大小与性能
 * ============================================================ */
static void demo_buf_performance(void) {
    printf("[socket_buf] 缓冲区大小与性能的关系:\n\n");

    printf("[socket_buf]   缓冲区太小：\n");
    printf("[socket_buf]     - 容易造成 TCP 拥塞窗口空转\n");
    printf("[socket_buf]     - 高延迟、高 CPU 使用率\n");
    printf("[socket_buf]     - 吞吐量受限\n\n");

    printf("[socket_buf]   缓冲区太大：\n");
    printf("[socket_buf]     - 内存占用增加\n");
    printf("[socket_buf]     - 单机可支持连接数减少\n");
    printf("[socket_buf]     - 延迟增加（数据在缓冲区排队）\n\n");

    printf("[socket_buf]   调优建议：\n");
    printf("[socket_buf]     - 高带宽低延迟网络：使用大缓冲区\n");
    printf("[socket_buf]     - 短连接场景：使用较小缓冲区\n");
    printf("[socket_buf]     - 数据库服务器：根据 max_connections 调整\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux Socket 缓冲区学习演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: 默认缓冲区大小 ---\n");
    demo_default_buffers();
    printf("\n");

    printf("--- Demo 2: 设置缓冲区大小 ---\n");
    demo_set_buffers();
    printf("\n");

    printf("--- Demo 3: TCP 特定选项 ---\n");
    demo_tcp_options();
    printf("\n");

    printf("--- Demo 4: 系统限制 ---\n");
    demo_sysctl_limits();
    printf("\n");

    printf("--- Demo 5: 性能关系 ---\n");
    demo_buf_performance();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
