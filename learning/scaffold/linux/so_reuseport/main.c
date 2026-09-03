/**
 * SO_REUSEPORT 负载均衡演示
 *
 * 演示如何利用 SO_REUSEPORT 选项实现多 socket 绑定同一端口，
 * 让内核自动进行负载均衡分配连接。
 *
 * 编译: gcc -std=c11 -Wall -O2 -o so_reuseport main.c -lpthread
 * 运行: ./so_reuseport
 * 测试: nc 127.0.0.1 8888 (多个终端同时连接)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8888
#define NUM_LISTENERS 4        // 监听 socket 数量
#define BUFFER_SIZE 256

// 原子计数器，记录每个 listener 接受的连接数
static volatile int accept_counts[NUM_LISTENERS];
static volatile bool running = true;

/**
 * 创建并配置一个带有 SO_REUSEPORT 的监听 socket
 * 
 * @param index listener 索引，用于标识
 * @return 成功返回 socket fd，失败返回 -1
 */
int create_reuseport_socket(int index) {
    int sockfd;
    int opt = 1;
    struct sockaddr_in addr;

    // 1. 创建 TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket 创建失败");
        return -1;
    }

    // 2. 设置 SO_REUSEADDR（复用地址，与 SO_REUSEPORT 配合）
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("SO_REUSEADDR 设置失败");
        close(sockfd);
        return -1;
    }

    // 3. 设置 SO_REUSEPORT（核心选项：允许多 socket 绑定同一端口）
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("SO_REUSEPORT 设置失败");
        close(sockfd);
        return -1;
    }

    // 4. 绑定地址
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind 失败");
        close(sockfd);
        return -1;
    }

    // 5. 开始监听
    if (listen(sockfd, 128) < 0) {
        perror("listen 失败");
        close(sockfd);
        return -1;
    }

    printf("[Listener %d] 监听 socket fd=%d, 端口=%d\n", index, sockfd, PORT);
    return sockfd;
}

/**
 * listener 线程：接受连接并记录统计
 */
void *listener_thread(void *arg) {
    int index = *(int *)arg;
    int sockfd = create_reuseport_socket(index);
    if (sockfd < 0) {
        free(arg);
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    printf("[Listener %d] 已就绪，等待连接...\n", index);
    fflush(stdout);

    while (running) {
        // 设置超时，避免长时间阻塞导致无法响应 shutdown 信号
        struct timeval tv = {1, 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // 超时，继续等待
            }
            if (!running) break;
            perror("accept 失败");
            continue;
        }

        // 记录接受的连接
        accept_counts[index]++;

        // 读取并回显数据
        ssize_t n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            printf("[Listener %d] 收到: %s", index, buffer);

            // 回复确认消息
            snprintf(buffer, BUFFER_SIZE, "Hello from Listener %d, count=%d\n",
                     index, accept_counts[index]);
            send(client_fd, buffer, strlen(buffer), 0);
        }

        close(client_fd);
    }

    close(sockfd);
    free(arg);
    printf("[Listener %d] 已关闭，共接受 %d 个连接\n", index, accept_counts[index]);
    return NULL;
}

/**
 * 打印负载均衡统计信息
 */
void print_stats(void) {
    int total = 0;
    printf("\n========== 负载均衡统计 ==========\n");
    for (int i = 0; i < NUM_LISTENERS; i++) {
        printf("Listener %d: %d 个连接\n", i, accept_counts[i]);
        total += accept_counts[i];
    }
    printf("总计: %d 个连接\n", total);
    printf("==================================\n\n");
}

int main(void) {
    pthread_t threads[NUM_LISTENERS];

    printf("========== SO_REUSEPORT 负载均衡演示 ==========\n");
    printf("创建 %d 个监听 socket，绑定同一端口 %d\n", NUM_LISTENERS, PORT);
    printf("请在另一终端运行: nc 127.0.0.1 %d\n\n", PORT);

    // 创建多个 listener 线程
    for (int i = 0; i < NUM_LISTENERS; i++) {
        int *index = malloc(sizeof(int));
        *index = i;
        if (pthread_create(&threads[i], NULL, listener_thread, index) != 0) {
            perror("pthread_create 失败");
            free(index);
            running = false;
            break;
        }
    }

    // 主线程等待 30 秒，期间可观察负载分布
    printf("服务器运行中 (30 秒)...\n");
    sleep(30);

    // 停止所有 listener
    printf("\n停止服务器...\n");
    running = false;

    // 等待线程结束
    for (int i = 0; i < NUM_LISTENERS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 打印统计
    print_stats();

    printf("服务器已关闭\n");
    return 0;
}
