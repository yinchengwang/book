/*
 * Unix Domain Socket 示例程序
 * 演示 AF_UNIX + SOCK_STREAM / SOCK_DGRAM 模式
 *
 * 编译: gcc -std=c11 -Wall -O2 -o unix_socket main.c
 * 运行: ./unix_socket
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define SOCK_STREAM_PATH "/tmp/unix_stream.sock"
#define SOCK_DGRAM_PATH  "/tmp/unix_dgram.sock"
#define BUFFER_SIZE 256

/* 辅助函数：创建 Unix 域 socket 并绑定路径 */
static int create_and_bind(const char *path, int type) {
    int fd;
    struct sockaddr_un addr;

    // 移除旧 socket 文件（如果存在）
    unlink(path);

    fd = socket(AF_UNIX, type, 0);
    if (fd < 0) {
        perror("socket 创建失败");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind 失败");
        close(fd);
        return -1;
    }

    return fd;
}

/* 测试 SOCK_STREAM（面向连接）模式 */
static void test_stream_mode(void) {
    printf("\n========== SOCK_STREAM 测试 ==========\n");

    int server_fd = create_and_bind(SOCK_STREAM_PATH, SOCK_STREAM);
    if (server_fd < 0) return;

    // 服务器监听连接
    if (listen(server_fd, 5) < 0) {
        perror("listen 失败");
        close(server_fd);
        return;
    }
    printf("[服务器] 监听 %s\n", SOCK_STREAM_PATH);

    // fork 创建一个子进程作为客户端
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork 失败");
        close(server_fd);
        return;
    }

    if (pid == 0) {
        // 子进程：作为客户端连接服务器
        sleep(1);  // 等待服务器准备就绪

        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (client_fd < 0) {
            perror("[客户端] socket 创建失败");
            exit(1);
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCK_STREAM_PATH, sizeof(addr.sun_path) - 1);

        if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("[客户端] connect 失败");
            close(client_fd);
            exit(1);
        }

        // 发送数据
        const char *msg = "你好，Stream 服务器！";
        write(client_fd, msg, strlen(msg));
        printf("[客户端] 发送: %s\n", msg);

        // 接收响应
        char buf[BUFFER_SIZE];
        ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[客户端] 收到响应: %s\n", buf);
        }

        close(client_fd);
        exit(0);
    }

    // 父进程：接受连接并处理
    struct sockaddr_un client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int conn_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (conn_fd < 0) {
        perror("accept 失败");
        close(server_fd);
        return;
    }
    printf("[服务器] 收到连接\n");

    // 接收数据
    char buf[BUFFER_SIZE];
    ssize_t n = read(conn_fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("[服务器] 收到: %s\n", buf);
    }

    // 发送响应
    const char *resp = "Stream 数据已收到";
    write(conn_fd, resp, strlen(resp));
    printf("[服务器] 发送响应: %s\n", resp);

    close(conn_fd);
    close(server_fd);
    wait(NULL);  // 等待子进程结束

    printf("[Stream 特点] 面向连接、可靠、字节流、无消息边界\n");
}

/* 测试 SOCK_DGRAM（无连接）模式 */
static void test_dgram_mode(void) {
    printf("\n========== SOCK_DGRAM 测试 ==========\n");

    int server_fd = create_and_bind(SOCK_DGRAM_PATH, SOCK_DGRAM);
    if (server_fd < 0) return;
    printf("[服务器] 绑定 %s\n", SOCK_DGRAM_PATH);

    // fork 创建一个子进程作为客户端
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork 失败");
        close(server_fd);
        return;
    }

    if (pid == 0) {
        // 子进程：作为客户端发送数据报
        sleep(1);

        int client_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (client_fd < 0) {
            perror("[客户端] socket 创建失败");
            exit(1);
        }

        struct sockaddr_un server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sun_family = AF_UNIX;
        strncpy(server_addr.sun_path, SOCK_DGRAM_PATH, sizeof(server_addr.sun_path) - 1);

        // 发送数据报（无连接）
        const char *msg = "你好，DGRAM 服务器！";
        sendto(client_fd, msg, strlen(msg), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr));
        printf("[客户端] 发送数据报: %s\n", msg);

        close(client_fd);
        exit(0);
    }

    // 父进程：接收数据报
    char buf[BUFFER_SIZE];
    struct sockaddr_un src_addr;
    socklen_t addr_len = sizeof(src_addr);

    ssize_t n = recvfrom(server_fd, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr *)&src_addr, &addr_len);
    if (n > 0) {
        buf[n] = '\0';
        printf("[服务器] 收到数据报: %s\n", buf);
    }

    close(server_fd);
    wait(NULL);

    printf("[DGRAM 特点] 无连接、数据报、有边界、最大长度限制\n");
}

int main(void) {
    printf("Unix Domain Socket 示例\n");
    printf("========================\n");

    test_stream_mode();
    test_dgram_mode();

    // 清理 socket 文件
    unlink(SOCK_STREAM_PATH);
    unlink(SOCK_DGRAM_PATH);

    printf("\n测试完成！\n");
    return 0;
}
