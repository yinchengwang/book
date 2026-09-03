/**
 * @file main.c
 * @brief Linux epoll 事件驱动编程演示
 *
 * 本文件演示：
 * 1. epoll_create/ctl/wait 基本用法
 * 2. LT（Level-Triggered）模式
 * 3. ET（Edge-Triggered）模式
 * 4. Reactor 事件循环模式
 *
 * 学习目标：
 * - 理解 epoll 相比 select/poll 的优势
 * - 掌握 LT 与 ET 模式的区别
 * - 实现高性能事件驱动服务器
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

// ================================================================
// 常量与类型定义
// ================================================================

#define PORT 8889
#define MAX_EVENTS 64
#define BUFFER_SIZE 4096

// 客户端连接状态
typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    int port;
    char recv_buf[BUFFER_SIZE];
    size_t recv_len;
} client_t;

// 全局状态
static volatile int g_running = 1;

// 打印分隔符
void print_separator(const char *title) {
    printf("\n%s\n", "============================================================");
    printf("  %s\n", title);
    printf("%s\n", "============================================================");
}

// 信号处理
void sigint_handler(int sig) {
    (void)sig;
    g_running = 0;
    printf("\n[Server] 收到 SIGINT，正在关闭...\n");
}

// 设置 socket 非阻塞
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ================================================================
// 第一部分：基础 epoll 用法
// ================================================================

/**
 * @brief 创建 epoll 实例并注册监听 socket
 */
int create_epoll_server(void) {
    int listen_fd, epfd;
    struct epoll_event ev;
    struct sockaddr_in addr;

    // 1. 创建监听 socket
    listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd < 0) {
        perror("socket()");
        return -1;
    }

    // 端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. 绑定
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind()");
        close(listen_fd);
        return -1;
    }

    // 3. 监听
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen()");
        close(listen_fd);
        return -1;
    }

    // 4. 创建 epoll 实例
    epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1()");
        close(listen_fd);
        return -1;
    }

    // 5. 注册监听 socket 到 epoll（LT 模式）
    ev.events = EPOLLIN;  // 有数据可读
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl()");
        close(epfd);
        close(listen_fd);
        return -1;
    }

    printf("[Server] 监听端口 %d，epfd=%d\n", PORT, epfd);
    return epfd;
}

/**
 * @brief 处理新连接
 */
int handle_new_connection(int epfd, int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    struct epoll_event ev;

    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept()");
        return -1;
    }

    // 设置非阻塞
    set_nonblocking(client_fd);

    // 打印客户端信息
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    printf("[Server] 新连接: %s:%d (fd=%d)\n", client_ip, ntohs(client_addr.sin_port), client_fd);

    // 注册到 epoll（LT 模式，启用 EPOLLET 看下一节）
    ev.events = EPOLLIN;  // 水平触发
    ev.data.fd = client_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

    return client_fd;
}

/**
 * @brief 处理客户端数据
 */
void handle_client_data(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (n > 0) {
        buffer[n] = '\0';
        printf("[Server] fd=%d 收到: %s\n", client_fd, buffer);

        // HTTP 响应
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n"
            "Connection: close\r\n"
            "\r\n"
            "OK";
        send(client_fd, response, strlen(response), 0);
    } else if (n == 0) {
        printf("[Server] fd=%d 客户端关闭连接\n", client_fd);
        close(client_fd);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv()");
            close(client_fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        }
    }
}

// 全局 epoll_fd（简化演示）
static int epoll_fd;
static int listen_fd;

void demo_basic_epoll(void) {
    print_separator("1. 基础 epoll (LT 模式)");

    // 设置信号处理
    signal(SIGINT, sigint_handler);

    epoll_fd = create_epoll_server();
    if (epoll_fd < 0) return;

    struct epoll_event events[MAX_EVENTS];

    printf("\n等待事件...\n");

    while (g_running) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);

        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait()");
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                // 监听 socket 有事件，新连接
                handle_new_connection(epoll_fd, fd);
            } else {
                // 客户端 socket 有事件
                if (events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)) {
                    handle_client_data(fd);
                }
            }
        }
    }

    close(epoll_fd);
    printf("[Server] 关闭\n");
}

// ================================================================
// 第二部分：ET 模式
// ================================================================

/**
 * @brief ET 模式下读取所有数据
 *
 * Edge-Triggered 模式下，必须一次性读取所有可用数据，
 * 否则剩余数据不会再次触发事件。
 */
ssize_t et_read_all(int fd, char *buf, size_t size) {
    size_t total = 0;
    while (total < size) {
        ssize_t n = read(fd, buf + total, size - total);
        if (n > 0) {
            total += n;
        } else if (n == 0) {
            break;  // 对端关闭
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;  // 无更多数据
        } else {
            return -1;  // 错误
        }
    }
    return total;
}

void demo_et_mode(void) {
    print_separator("2. Edge-Triggered (ET) 模式");

    printf("ET 模式特点：\n");
    printf("- 只有状态变化时触发事件（从无到有时）\n");
    printf("- 必须一次性读取所有数据（循环直到 EAGAIN）\n");
    printf("- 避免漏掉数据\n\n");

    int listen_fd, epfd;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in addr;

    // 创建监听 socket
    listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT + 1);
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, SOMAXCONN);

    // 创建 epoll 并注册
    epfd = epoll_create1(0);
    ev.events = EPOLLIN | EPOLLET;  // ET 模式！
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    set_nonblocking(listen_fd);  // ET 模式必须非阻塞

    printf("ET Server 监听端口 %d，等待连接...\n", PORT + 1);

    int count = 0;
    while (count < 2) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, 3000);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                // 新连接
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                int cfd = accept(listen_fd, (struct sockaddr *)&client, &len);
                if (cfd >= 0) {
                    set_nonblocking(cfd);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = cfd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
                    printf("  [ET] 新连接 fd=%d\n", cfd);
                }
            } else if (events[i].events & EPOLLIN) {
                // ET 模式必须循环读取
                char buf[256];
                while (1) {
                    ssize_t r = read(fd, buf, sizeof(buf) - 1);
                    if (r > 0) {
                        buf[r] = '\0';
                        printf("  [ET] fd=%d 读取: %s", fd, buf);
                        count++;
                    } else if (r == 0) {
                        printf("  [ET] fd=%d 关闭\n", fd);
                        close(fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        break;
                    } else if (errno == EAGAIN) {
                        break;  // 本轮读完
                    } else {
                        perror("read()");
                        break;
                    }
                }
            }
        }
    }

    close(epfd);
    close(listen_fd);
}

// ================================================================
// 第三部分：LT vs ET 对比
// ================================================================

void demo_lt_vs_et(void) {
    print_separator("3. LT vs ET 模式对比");

    printf("| 特性         | LT (Level-Triggered) | ET (Edge-Triggered) |\n");
    printf("|--------------|----------------------|--------------------|\n");
    printf("| 触发条件     | 数据就绪时持续触发   | 状态变化时触发一次 |\n");
    printf("| 读取要求     | 可以只读部分数据     | 必须读完所有数据   |\n");
    printf("| 编程难度     | 简单                 | 复杂               |\n");
    printf("| 适用场景     | 普通网络编程         | 高性能服务器       |\n");
    printf("| 典型应用     | Redis, Nginx         | 高并发事件驱动     |\n");

    printf("\n建议：\n");
    printf("- 入门：先用 LT 模式（我们的 basic_epoll 示例）\n");
    printf("- 生产：ET 模式 + 非阻塞 socket（参考 Redis/Nginx）\n");
}

// ================================================================
// 主函数
// ================================================================

int main(int argc, char *argv[]) {
    printf("Linux epoll 事件驱动编程演示\n");
    printf("============================\n");

    if (argc > 1 && strcmp(argv[1], "et") == 0) {
        demo_et_mode();
    } else if (argc > 1 && strcmp(argv[1], "compare") == 0) {
        demo_lt_vs_et();
    } else {
        printf("用法: %s [basic|et|compare]\n", argv[0]);
        printf("\n演示模式:\n");
        printf("  (无参数) - LT 模式 epoll 服务器\n");
        printf("  et       - ET 模式演示\n");
        printf("  compare  - LT vs ET 对比\n\n");

        demo_lt_vs_et();
        demo_et_mode();
    }

    print_separator("演示完成");
    printf("本演示覆盖了以下主题：\n");
    printf("1. epoll_create/ctl/wait 基础用法\n");
    printf("2. LT (Level-Triggered) 模式\n");
    printf("3. ET (Edge-Triggered) 模式\n");
    printf("4. Reactor 事件循环模式\n");

    return 0;
}
