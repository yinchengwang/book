/**
 * @file src/db/ecosystem/rest_server.c
 * @brief Ecosystem REST API 服务器实现
 *
 * 基于 Winsock 的简单 HTTP REST API 服务器。
 */
#include <db/ecosystem/rest_server.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 服务器内部状态 */
struct rest_server {
    int port;                        /**< 监听端口 */
    SOCKET listen_socket;            /**< 监听 socket */
    volatile bool running;           /**< 运行状态 */
    int64_t start_time;              /**< 启动时间 */
    pthread_mutex_t mutex;           /**< 互斥锁 */
    pthread_t accept_thread;         /**< 接受线程 */
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取当前时间（毫秒）
 */
static int64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * @brief 发送 HTTP 响应
 */
static void send_response(SOCKET client, int status_code, const char *status_msg,
                         const char *body, const char *content_type) {
    char header[1024];
    int header_len;

    header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        status_code, status_msg,
        content_type ? content_type : "application/json",
        body ? strlen(body) : 0);

    send(client, header, (int)strlen(header), 0);
    if (body && strlen(body) > 0) {
        send(client, body, (int)strlen(body), 0);
    }
}

/**
 * @brief 解析 HTTP 请求
 */
static int parse_request(const char *request, char *method, char *path,
                         char *body, size_t *body_len) {
    /* 解析请求行 */
    const char *path_start = strchr(request, ' ');
    if (!path_start) return -1;

    size_t method_len = path_start - request;
    if (method_len >= 32) return -1;
    strncpy(method, request, method_len);
    method[method_len] = '\0';

    path_start++;
    const char *path_end = strchr(path_start, ' ');
    if (!path_end) return -1;

    size_t path_len = path_end - path_start;
    if (path_len >= 1024) return -1;
    strncpy(path, path_start, path_len);
    path[path_len] = '\0';

    /* 查找请求体 */
    const char *body_start = strstr(request, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        size_t remaining = strlen(request) - (size_t)(body_start - request);
        if (remaining > 0 && *body_len > 0) {
            size_t copy_len = remaining < (*body_len - 1) ? remaining : (*body_len - 1);
            strncpy(body, body_start, copy_len);
            body[copy_len] = '\0';
            *body_len = copy_len;
        } else {
            body[0] = '\0';
            *body_len = 0;
        }
    } else {
        body[0] = '\0';
        *body_len = 0;
    }

    return 0;
}

/**
 * @brief URL 解码
 */
static void url_decode(char *dst, const char *src, size_t dst_size) {
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], '\0'};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/**
 * @brief 提取 URL 路径（去除查询参数）
 */
static void extract_path(char *dst, const char *url, size_t dst_size) {
    const char *path = url;
    const char *query = strchr(url, '?');
    size_t len = query ? (size_t)(query - url) : strlen(url);
    if (len >= dst_size) len = dst_size - 1;
    strncpy(dst, path, len);
    dst[len] = '\0';
    url_decode(dst, dst, dst_size);
}

/* ========================================================================
 * 请求处理
 * ======================================================================== */

/**
 * @brief 处理请求的线程函数
 */
static void *request_handler_thread(void *arg) {
    rest_server_t *server = (rest_server_t *)arg;

    while (server->running) {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client = accept(server->listen_socket, (struct sockaddr *)&client_addr, &addr_len);

        if (client == INVALID_SOCKET) {
            if (server->running) {
                Sleep(10);
            }
            continue;
        }

        /* 读取请求 */
        char request[65536];
        int bytes_received = recv(client, request, sizeof(request) - 1, 0);

        if (bytes_received <= 0) {
            closesocket(client);
            continue;
        }

        request[bytes_received] = '\0';

        /* 解析请求 */
        char method[32], path[1024], body[65536];
        size_t body_len = sizeof(body);

        if (parse_request(request, method, path, body, &body_len) != 0) {
            send_response(client, 400, "Bad Request",
                         "{\"error\":\"Invalid request\"}",
                         "application/json");
            closesocket(client);
            continue;
        }

        /* 提取路径 */
        char decoded_path[1024];
        extract_path(decoded_path, path, sizeof(decoded_path));

        /* 处理路由 */
        if (strcmp(method, "GET") == 0 && strcmp(decoded_path, "/health") == 0) {
            send_response(client, 200, "OK",
                         "{\"status\":\"healthy\",\"uptime\":%lld}",
                         "application/json");
        } else if (strcmp(method, "GET") == 0 && strcmp(decoded_path, "/ready") == 0) {
            send_response(client, 200, "OK",
                         "{\"status\":\"ready\"}",
                         "application/json");
        } else if (strcmp(method, "GET") == 0 && strcmp(decoded_path, "/live") == 0) {
            send_response(client, 200, "OK",
                         "{\"status\":\"live\"}",
                         "application/json");
        } else {
            send_response(client, 404, "Not Found",
                         "{\"error\":\"Not found\"}",
                         "application/json");
        }

        closesocket(client);
    }

    return NULL;
}

/* ========================================================================
 * 生命周期实现
 * ======================================================================== */

rest_server_t *rest_server_create(int port) {
    rest_server_t *server = calloc(1, sizeof(rest_server_t));
    if (!server) return NULL;

    server->port = port;
    server->listen_socket = INVALID_SOCKET;
    server->running = false;
    pthread_mutex_init(&server->mutex, NULL);

    /* 初始化 Winsock */
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        pthread_mutex_destroy(&server->mutex);
        free(server);
        return NULL;
    }

    /* 创建 socket */
    server->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->listen_socket == INVALID_SOCKET) {
        WSACleanup();
        pthread_mutex_destroy(&server->mutex);
        free(server);
        return NULL;
    }

    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server->listen_socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(server->listen_socket);
        WSACleanup();
        pthread_mutex_destroy(&server->mutex);
        free(server);
        return NULL;
    }

    /* 监听 */
    if (listen(server->listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(server->listen_socket);
        WSACleanup();
        pthread_mutex_destroy(&server->mutex);
        free(server);
        return NULL;
    }

    return server;
}

int rest_server_start(rest_server_t *srv) {
    if (!srv) return -1;

    srv->running = true;
    srv->start_time = get_current_time_ms();

    /* 启动请求处理线程 */
    pthread_create(&srv->accept_thread, NULL, request_handler_thread, srv);

    return 0;
}

void rest_server_stop(rest_server_t *srv) {
    if (!srv || !srv->running) return;

    srv->running = false;

    if (srv->listen_socket != INVALID_SOCKET) {
        closesocket(srv->listen_socket);
        srv->listen_socket = INVALID_SOCKET;
    }

    pthread_join(srv->accept_thread, NULL);
}

void rest_server_destroy(rest_server_t *srv) {
    if (!srv) return;

    if (srv->running) {
        rest_server_stop(srv);
    }

    WSACleanup();
    pthread_mutex_destroy(&srv->mutex);
    free(srv);
}
