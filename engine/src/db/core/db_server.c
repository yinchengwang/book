/**
 * @file db_server.c
 * @brief 数据库服务器实现 - 简化版 PostgreSQL wire 协议
 */

#include "db/db_server.h"
#include "db/log.h"
#include "db/errors.h"
#include "db/guc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define close closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

/* ============================================================
 * 静态变量
 * ============================================================ */

static int g_server_socket = -1;
static bool g_running = false;
static const char *g_data_dir = NULL;

/* ============================================================
 * Socket 初始化
 * ============================================================ */

#ifdef _WIN32
static int socket_init(void) {
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data);
}
#else
static int socket_init(void) { return 0; }
#endif

/* ============================================================
 * PostgreSQL Wire 协议处理（简化版）
 * ============================================================ */

/**
 * @brief 发送 ReadyForQuery 消息
 */
static void send_ready(int fd) {
    char msg[] = {'I', 0, 0, 0, 5};  /* ReadyForQuery + 'I' */
    send(fd, msg, 5, 0);
}

/**
 * @brief 发送简单查询响应
 */
static void send_simple_query_response(int fd, const char *result) {
    /* RowDescription */
    char rowdesc[24] = {
        'T', 0, 0, 0, 20,  /* type + length */
        0, 1,               /* num fields */
        0, 0, 0, 0,         /* field name */
        0, 0, 0, 0,         /* table OID */
        0, 0,               /* column ID */
        0, 0, 0, 0xFF,      /* data type OID (text) */
        0, 0, 0, 0x10,      /* data size */
        0, 0                 /* format (text) */
    };
    send(fd, rowdesc, 21, 0);

    /* DataRow */
    char datarow[11] = {
        'D', 0, 0, 0, 11,   /* type + length */
        0, 1,               /* num columns */
        0, 0, 0, 0, 0,     /* column length = 0 (NULL) */
        0, 0, 0, 0, 5       /* "hello" */
    };
    send(fd, datarow, 14, 0);

    send_ready(fd);
}

/**
 * @brief 发送错误响应
 */
static void send_error_response(int fd, const char *error) {
    char msg[256];
    size_t error_len = strlen(error);
    size_t total_len = error_len + 8;

    if (total_len > sizeof(msg)) {
        total_len = sizeof(msg);
        error_len = total_len - 8 - 1;  /* 留 1 字节终止符 */
    }

    msg[0] = 'E';  /* ErrorResponse */
    msg[1] = 0;
    msg[2] = 0;
    msg[3] = (char)(total_len & 0xFF);
    msg[4] = 'S';  /* severity */
    msg[5] = 0;
    msg[6] = 'C';
    msg[7] = '0';
    msg[8] = '0';
    msg[9] = '0';
    msg[10] = 'M';  /* message */
    int i = 11;
    strncpy(&msg[i], error, error_len);
    i += (int)error_len;
    msg[i++] = 0;  /* end of message */
    msg[i++] = 0;  /* end of messages */

    send(fd, msg, i, 0);
    send_ready(fd);
}

/**
 * @brief 处理 StartupMessage
 */
static int handle_startup(int fd, const char *buf, int len) {
    /* 跳过长度和协议版本 */
    const char *p = buf + 8;

    char database[64] = {0};
    char user[64] = {0};

    while (p < buf + len && *p != 0) {
        char *key = (char *)p;
        p += strlen(key) + 1;
        if (p >= buf + len) break;

        char *value = (char *)p;
        p += strlen(value) + 1;

        if (strcmp(key, "database") == 0) {
            strncpy(database, value, sizeof(database) - 1);
        } else if (strcmp(key, "user") == 0) {
            strncpy(user, value, sizeof(user) - 1);
        }
    }

    LOG_INFO("连接: user=%s database=%s", user, database);

    /* 发送 AuthenticationOk */
    char auth_ok[9] = {
        'R', 0, 0, 0, 8,  /* type + length */
        0, 0, 0, 0         /* AuthenticationOk */
    };
    send(fd, auth_ok, 8, 0);

    send_ready(fd);
    return 0;
}

/**
 * @brief 处理查询
 */
static int handle_query(int fd, const char *sql, int len) {
    char *query = malloc(len + 1);
    memcpy(query, sql, len);
    query[len] = 0;

    LOG_INFO("执行 SQL: %s", query);

    /* 这里可以调用实际的 SQL 执行器 */
    /* 目前返回空结果集 */
    send_ready(fd);

    free(query);
    return 0;
}

/**
 * @brief 处理单个连接
 */
void server_handle_connection(int fd) {
    char buf[BUFFER_SIZE];
    int buf_len = 0;

    LOG_INFO("处理连接 (fd=%d)", fd);

    while (g_running) {
        int n = recv(fd, buf + buf_len, BUFFER_SIZE - buf_len - 1, 0);
        if (n <= 0) {
            break;
        }
        buf_len += n;
        buf[buf_len] = 0;

        /* 处理接收到的消息 */
        char *p = buf;
        while (p < buf + buf_len) {
            char type = p[0];

            if (type == 'Q') {  /* Query */
                int len = *(int *)(p + 1);
                handle_query(fd, p + 5, len - 5);
                p += 4 + len;
            } else if (type == 'X') {  /* Terminate */
                LOG_INFO("客户端断开连接");
                close(fd);
                return;
            } else if (p[0] == 0 && p[1] == 0) {  /* StartupMessage */
                int len = *(int *)(p + 1);
                handle_startup(fd, p, len);
                p += 4 + len;
            } else {
                /* 未知消息类型，忽略 */
                break;
            }
        }

        /* 移动缓冲区剩余数据 */
        if (p < buf + buf_len) {
            memmove(buf, p, buf + buf_len - p);
            buf_len = buf + buf_len - p;
        } else {
            buf_len = 0;
        }
    }

    close(fd);
}

/* ============================================================
 * 服务器主循环
 * ============================================================ */

static void *accept_thread(void *arg) {
    (void)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    LOG_INFO("服务器监听线程启动");

    while (g_running) {
        int client_fd = accept(g_server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (g_running) {
                LOG_ERROR("accept 失败: %d", client_fd);
            }
            break;
        }

        LOG_INFO("收到连接: %s:%d",
                 inet_ntoa(client_addr.sin_addr),
                 ntohs(client_addr.sin_port));

        /* 在新线程中处理连接 */
#ifdef _WIN32
        HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)server_handle_connection,
                                     (LPVOID)client_fd, 0, NULL);
        if (thread) CloseHandle(thread);
#else
        pthread_t thread;
        pthread_create(&thread, NULL, server_handle_connection, (void *)(long)client_fd);
        pthread_detach(thread);
#endif
    }

    return NULL;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

int server_start(int port, const char *data_dir) {
    struct sockaddr_in addr;

    if (socket_init() != 0) {
        LOG_ERROR("Socket 初始化失败");
        return -1;
    }

    g_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_socket < 0) {
        LOG_ERROR("创建 socket 失败");
        return -1;
    }

    int opt = 1;
    setsockopt(g_server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(g_server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("绑定端口 %d 失败", port);
        close(g_server_socket);
        return -1;
    }

    if (listen(g_server_socket, 10) < 0) {
        LOG_ERROR("监听失败");
        close(g_server_socket);
        return -1;
    }

    g_running = true;
    g_data_dir = data_dir;

    LOG_INFO("服务器启动: port=%d data_dir=%s", port, data_dir ? data_dir : "(null)");

#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)accept_thread, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, accept_thread, NULL);
    pthread_detach(thread);
#endif

    return 0;
}

void server_stop(void) {
    LOG_INFO("服务器停止...");
    g_running = false;

    if (g_server_socket >= 0) {
        close(g_server_socket);
        g_server_socket = -1;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    LOG_INFO("服务器已停止");
}

int server_execute_sql(const char *sql, char *output, size_t output_len) {
    (void)sql;
    (void)output;
    (void)output_len;
    LOG_INFO("执行 SQL (stub): %s", sql);
    return 0;
}

/* ============================================================
 * 主函数
 * ============================================================ */

#ifdef DB_SERVER_MAIN
int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    const char *data_dir = NULL;
    const char *config_file = NULL;

    /* 解析参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("  -D DIR      数据目录\n");
            printf("  -p PORT     端口 (默认 %d)\n", DEFAULT_PORT);
            printf("  -c FILE     配置文件\n");
            return 0;
        }
    }

    /* 加载配置 */
    if (config_file) {
        guc_init(data_dir);
        guc_load_file(config_file);
    } else if (data_dir) {
        guc_init(data_dir);
        char conf[256];
        snprintf(conf, sizeof(conf), "%s/postgresql.conf", data_dir);
        guc_load_file(conf);
    }

    /* 启动服务器 */
    if (server_start(port, data_dir) != 0) {
        fprintf(stderr, "服务器启动失败\n");
        return 1;
    }

    printf("数据库服务器已启动，按 Ctrl+C 停止...\n");

    /* 等待信号 */
#ifdef _WIN32
    while (g_running) {
        Sleep(1000);
    }
#else
    while (g_running) {
        sleep(1);
    }
#endif

    server_stop();
    guc_shutdown();
    return 0;
}
#endif
