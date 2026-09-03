/**
 * @file main.c
 * @brief Linux Socket 编程基础演示
 *
 * 本文件演示：
 * 1. TCP server/client 通信流程
 * 2. UDP datagram 收发
 * 3. socket 选项设置（SO_REUSEADDR）
 * 4. 非阻塞与超时设置
 *
 * 学习目标：
 * - 理解 socket/bind/listen/accept/connect 全流程
 * - 掌握 TCP 与 UDP 的区别
 * - 学会处理常见 socket 错误
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

// ================================================================
// 辅助宏与常量
// ================================================================

#define PORT 8888
#define BUFFER_SIZE 1024
#define TIMEOUT_MS  3000

// 打印带标题的分隔符
void print_separator(const char *title) {
    printf("\n%s\n", "============================================================");
    printf("  %s\n", title);
    printf("%s\n", "============================================================");
}

// 设置 socket 为非阻塞模式
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 设置 socket 超时
int set_timeout(int fd, int milliseconds) {
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

// 打印 socket 选项
void print_socket_options(int fd) {
    int optval;
    socklen_t optlen = sizeof(optval);

    printf("\n  --- Socket 选项 ---\n");

    if (getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen) == 0) {
        printf("  SO_REUSEADDR = %d\n", optval);
    }
    if (getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) == 0) {
        printf("  SO_KEEPALIVE = %d\n", optval);
    }
    if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, &optlen) == 0) {
        printf("  TCP_NODELAY = %d\n", optval);
    }
}

// ================================================================
// 第一部分：TCP Server
// ================================================================

/**
 * @brief 创建 TCP 服务器
 *
 * 流程：socket() -> bind() -> listen() -> accept() -> recv/send -> close()
 */
int create_tcp_server(void) {
    int server_fd;
    struct sockaddr_in addr;

    // 1. 创建 socket
    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) {
        perror("  [Server] socket() 失败");
        return -1;
    }
    printf("  [Server] socket() 创建成功，fd=%d\n", server_fd);

    // 2. 设置 SO_REUSEADDR（端口复用，快速重启）
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. 绑定地址
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("  [Server] bind() 失败");
        close(server_fd);
        return -1;
    }
    printf("  [Server] bind() 绑定端口 %d 成功\n", PORT);

    // 4. 监听
    if (listen(server_fd, 5) < 0) {
        perror("  [Server] listen() 失败");
        close(server_fd);
        return -1;
    }
    printf("  [Server] listen() 开始监听\n");

    print_socket_options(server_fd);

    return server_fd;
}

/**
 * @brief 处理单个客户端连接
 */
void handle_client(int client_fd, struct sockaddr_in *client_addr) {
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    printf("  [Server] 客户端连接: %s:%d\n", client_ip, ntohs(client_addr->sin_port));

    // 接收数据
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("  [Server] 收到: %s\n", buffer);

        // 回显
        const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello";
        send(client_fd, response, strlen(response), 0);
        printf("  [Server] 发送响应\n");
    } else if (n == 0) {
        printf("  [Server] 客户端关闭连接\n");
    } else {
        perror("  [Server] recv() 失败");
    }
}

void demo_tcp_server(void) {
    print_separator("1. TCP Server");

    int server_fd = create_tcp_server();
    if (server_fd < 0) return;

    printf("\n  等待客户端连接（5秒超时）...\n");

    // 使用 poll 检测新连接
    struct pollfd pfd = { .fd = server_fd, .events = POLLIN };
    int ret = poll(&pfd, 1, 5000);

    if (ret > 0) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (client_fd >= 0) {
            handle_client(client_fd, &client_addr);
            close(client_fd);
        }
    } else if (ret == 0) {
        printf("  [Server] 等待超时，无客户端连接\n");
    } else {
        perror("  [Server] poll() 失败");
    }

    close(server_fd);
    printf("  [Server] 关闭服务器\n");
}

// ================================================================
// 第二部分：TCP Client
// ================================================================

/**
 * @brief 创建 TCP 客户端并连接服务器
 */
int create_tcp_client(const char *server_ip, int port) {
    int client_fd;
    struct sockaddr_in server_addr;

    // 1. 创建 socket
    client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_fd < 0) {
        perror("  [Client] socket() 失败");
        return -1;
    }
    printf("  [Client] socket() 创建成功，fd=%d\n", client_fd);

    // 2. 设置连接超时
    set_timeout(client_fd, TIMEOUT_MS);

    // 3. 连接服务器
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("  [Client] inet_pton() 失败");
        close(client_fd);
        return -1;
    }

    printf("  [Client] 连接到 %s:%d...\n", server_ip, port);
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("  [Client] connect() 失败");
        close(client_fd);
        return -1;
    }
    printf("  [Client] 连接成功!\n");

    print_socket_options(client_fd);

    return client_fd;
}

void demo_tcp_client(void) {
    print_separator("2. TCP Client");

    // 注意：需要先运行 demo_tcp_server() 再运行此函数
    // 这里演示独立连接逻辑
    int client_fd = create_tcp_client("127.0.0.1", PORT);
    if (client_fd < 0) {
        printf("  [Client] 连接失败（服务器未运行）\n");
        return;
    }

    // 发送请求
    const char *request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    ssize_t sent = send(client_fd, request, strlen(request), 0);
    printf("  [Client] 发送 %zd 字节\n", sent);

    // 接收响应
    char buffer[BUFFER_SIZE];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("  [Client] 收到响应 (%zd 字节)\n", n);
    }

    close(client_fd);
    printf("  [Client] 连接已关闭\n");
}

// ================================================================
// 第三部分：UDP Demo
// ================================================================

void demo_udp(void) {
    print_separator("3. UDP Datagram");

    int udp_fd;
    struct sockaddr_in local_addr, remote_addr;

    // 1. 创建 UDP socket
    udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_fd < 0) {
        perror("  [UDP] socket() 失败");
        return;
    }
    printf("  [UDP] socket() 创建成功，fd=%d\n", udp_fd);

    // 2. 绑定本地地址
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(PORT + 1);  // 不同于 TCP 端口

    if (bind(udp_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("  [UDP] bind() 失败");
        close(udp_fd);
        return;
    }
    printf("  [UDP] bind() 绑定端口 %d 成功\n", PORT + 1);

    // 3. 发送数据
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    remote_addr.sin_port = htons(PORT);

    const char *msg = "UDP Hello from client!";
    ssize_t sent = sendto(udp_fd, msg, strlen(msg), 0,
                          (struct sockaddr *)&remote_addr, sizeof(remote_addr));
    printf("  [UDP] sendto() 发送 %zd 字节\n", sent);

    // 4. 接收数据（带超时）
    set_timeout(udp_fd, 1000);
    char buffer[BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    ssize_t n = recvfrom(udp_fd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr *)&from_addr, &from_len);
    if (n > 0) {
        buffer[n] = '\0';
        char from_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, from_ip, sizeof(from_ip));
        printf("  [UDP] recvfrom() 收到: %s (from %s:%d)\n",
               buffer, from_ip, ntohs(from_addr.sin_port));
    } else if (n == -1 && errno == EAGAIN) {
        printf("  [UDP] recvfrom() 超时（无数据）\n");
    }

    close(udp_fd);
    printf("  [UDP] 关闭\n");
}

// ================================================================
// 主函数
// ================================================================

int main(int argc, char *argv[]) {
    printf("Linux Socket 编程基础演示\n");
    printf("==========================\n");
    printf("用法: %s [server|client|udp]\n", argv[0]);

    if (argc < 2) {
        printf("\n演示所有功能（注意：需要先启动 server 再启动 client）\n");
        demo_tcp_server();
        demo_tcp_client();
        demo_udp();
    } else if (strcmp(argv[1], "server") == 0) {
        demo_tcp_server();
    } else if (strcmp(argv[1], "client") == 0) {
        demo_tcp_client();
    } else if (strcmp(argv[1], "udp") == 0) {
        demo_udp();
    } else {
        fprintf(stderr, "未知参数: %s\n", argv[1]);
        return 1;
    }

    print_separator("演示完成");
    printf("本演示覆盖了以下主题：\n");
    printf("1. TCP Server: socket -> bind -> listen -> accept -> recv/send\n");
    printf("2. TCP Client: socket -> connect -> send -> recv\n");
    printf("3. UDP: socket -> bind -> sendto/recvfrom\n");
    printf("4. Socket 选项: SO_REUSEADDR, SO_KEEPALIVE, TCP_NODELAY\n");
    printf("5. 超时设置: SO_RCVTIMEO\n");

    return 0;
}
