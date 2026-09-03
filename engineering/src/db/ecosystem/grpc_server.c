/**
 * @file src/db/ecosystem/grpc_server.c
 * @brief Ecosystem gRPC 服务器实现
 *
 * 基于 Winsock 的简单 gRPC 服务器实现。
 * 支持 Query 和 Execute 两个 RPC 方法。
 */
#include <db/ecosystem/grpc_server.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

/* gRPC 帧常量 */
#define GRPC_FRAME_HEADER_SIZE 5
#define GRPC_CONTENT_TYPE "application/grpc"
#define GRPC_STATUS_OK 0
#define GRPC_STATUS_BAD_REQUEST 3
#define GRPC_STATUS_INTERNAL 13

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** gRPC 服务器内部状态 */
struct grpc_server {
    int port;                          /**< 监听端口 */
    SOCKET listen_socket;              /**< 监听 socket */
    volatile bool running;             /**< 运行状态 */
    int64_t start_time;                /**< 启动时间 */
    pthread_mutex_t mutex;             /**< 互斥锁 */
    pthread_t accept_thread;           /**< 接受线程 */
    grpc_server_callbacks_t callbacks; /**< 回调函数 */
    void *user_data;                   /**< 用户数据 */
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
 * @brief 读取完整的 gRPC 帧
 */
static int read_grpc_frame(SOCKET client, char **body, size_t *body_len) {
    char header[GRPC_FRAME_HEADER_SIZE];
    int bytes_received;

    /* 读取帧头 */
    bytes_received = recv(client, header, GRPC_FRAME_HEADER_SIZE, 0);
    if (bytes_received != GRPC_FRAME_HEADER_SIZE) {
        return -1;
    }

    /* 解析帧长度 (3字节 big-endian) */
    size_t frame_length = ((unsigned char)header[1] << 16) |
                          ((unsigned char)header[2] << 8) |
                          ((unsigned char)header[3]);

    if (frame_length > 1024 * 1024) {  /* 限制 1MB */
        return -1;
    }

    /* 读取帧体 */
    char *frame_data = malloc(frame_length + 1);
    if (!frame_data) return -1;

    size_t total_received = 0;
    while (total_received < frame_length) {
        bytes_received = recv(client, frame_data + total_received,
                             (int)(frame_length - total_received), 0);
        if (bytes_received <= 0) {
            free(frame_data);
            return -1;
        }
        total_received += (size_t)bytes_received;
    }
    frame_data[frame_length] = '\0';

    *body = frame_data;
    *body_len = frame_length;
    return 0;
}

/**
 * @brief 发送 gRPC 响应帧
 */
static int send_grpc_response(SOCKET client, int status, const char *message,
                             const void *data, size_t data_len) {
    char header[GRPC_FRAME_HEADER_SIZE];
    char *response = NULL;
    size_t response_len;
    int total_len;

    /* 计算响应长度 */
    /* 格式: status(1) + message_length(4) + message + data */
    size_t msg_len = strlen(message);
    total_len = 1 + 4 + msg_len + data_len;

    response = malloc(total_len);
    if (!response) return -1;

    /* 打包响应 */
    size_t offset = 0;
    response[offset++] = (char)status;

    /* 消息长度 (4字节 big-endian) */
    response[offset++] = (char)((msg_len >> 24) & 0xFF);
    response[offset++] = (char)((msg_len >> 16) & 0xFF);
    response[offset++] = (char)((msg_len >> 8) & 0xFF);
    response[offset++] = (char)(msg_len & 0xFF);

    /* 消息内容 */
    memcpy(response + offset, message, msg_len);
    offset += msg_len;

    /* 数据内容 */
    if (data && data_len > 0) {
        memcpy(response + offset, data, data_len);
    }

    /* 发送帧头 */
    header[0] = 0;  /* 压缩标志 */
    header[1] = (char)((total_len >> 16) & 0xFF);
    header[2] = (char)((total_len >> 8) & 0xFF);
    header[3] = (char)(total_len & 0xFF);
    header[4] = 0;  /* 保留字节 */

    if (send(client, header, GRPC_FRAME_HEADER_SIZE, 0) != GRPC_FRAME_HEADER_SIZE) {
        free(response);
        return -1;
    }

    /* 发送响应体 */
    if (send(client, response, (int)total_len, 0) != (int)total_len) {
        free(response);
        return -1;
    }

    free(response);
    return 0;
}

/**
 * @brief 解析 Query 请求
 */
static int parse_query_request(const char *body, size_t body_len,
                               char *sql, size_t sql_size) {
    /* 简单的文本格式解析: "sql=xxx" */
    if (body_len >= 4 && strncmp(body, "sql=", 4) == 0) {
        size_t sql_len = body_len - 4;
        if (sql_len >= sql_size) sql_len = sql_size - 1;
        strncpy(sql, body + 4, sql_len);
        sql[sql_len] = '\0';
        return 0;
    }
    return -1;
}

/**
 * @brief 解析 Execute 请求
 */
static int parse_execute_request(const char *body, size_t body_len,
                                 char *sql, size_t sql_size) {
    return parse_query_request(body, body_len, sql, sql_size);
}

/**
 * @brief 处理请求的线程函数
 */
static void *request_handler_thread(void *arg) {
    grpc_server_t *server = (grpc_server_t *)arg;

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
        char *body = NULL;
        size_t body_len = 0;

        if (read_grpc_frame(client, &body, &body_len) != 0) {
            closesocket(client);
            continue;
        }

        /* 检查是否是 POST */
        if (body_len > 0 && body[0] == 0) {  /* POST 请求 */
            char sql[4096];
            memset(sql, 0, sizeof(sql));

            /* 解析路径和方法 */
            /* 简单实现：假设请求格式为 "POST /mmdb.MMDBService/Query" */
            char *path_start = strstr(body + 1, "POST /");
            if (path_start) {
                char *path_end = strstr(path_start, " HTTP");
                if (path_end) {
                    size_t path_len = (size_t)(path_end - path_start - 6);
                    if (path_len < sizeof(sql)) {
                        strncpy(sql, path_start + 6, path_len);
                        sql[path_len] = '\0';
                    }
                }
            }

            /* 查找请求体开始 (空行后) */
            char *body_start = strstr(body, "\r\n\r\n");
            if (body_start) {
                body_start += 4;
                size_t actual_body_len = body_len - (size_t)(body_start - body);

                if (strstr(sql, "Query") != NULL) {
                    /* 处理 Query 请求 */
                    if (server->callbacks.on_query) {
                        grpc_query_result_t result;
                        server->callbacks.on_query(body_start, actual_body_len,
                                                   server->user_data, &result);

                        send_grpc_response(client,
                                          result.status == 0 ? GRPC_STATUS_OK : GRPC_STATUS_INTERNAL,
                                          result.message ? result.message : "",
                                          result.data, result.data_len);
                    } else {
                        send_grpc_response(client, GRPC_STATUS_INTERNAL,
                                          "No query handler registered", NULL, 0);
                    }
                } else if (strstr(sql, "Execute") != NULL) {
                    /* 处理 Execute 请求 */
                    if (server->callbacks.on_execute) {
                        grpc_execute_result_t result;
                        server->callbacks.on_execute(body_start, actual_body_len,
                                                     server->user_data, &result);

                        char msg[256];
                        snprintf(msg, sizeof(msg), "%lld,%lld",
                                (long long)result.rows_affected,
                                (long long)result.last_insert_id);

                        send_grpc_response(client,
                                          result.status == 0 ? GRPC_STATUS_OK : GRPC_STATUS_INTERNAL,
                                          msg, NULL, 0);
                    } else {
                        send_grpc_response(client, GRPC_STATUS_INTERNAL,
                                          "No execute handler registered", NULL, 0);
                    }
                } else {
                    send_grpc_response(client, GRPC_STATUS_BAD_REQUEST,
                                      "Unknown method", NULL, 0);
                }
            } else {
                send_grpc_response(client, GRPC_STATUS_BAD_REQUEST,
                                  "Invalid request body", NULL, 0);
            }
        } else {
            send_grpc_response(client, GRPC_STATUS_BAD_REQUEST,
                              "Invalid request", NULL, 0);
        }

        free(body);
        closesocket(client);
    }

    return NULL;
}

/* ========================================================================
 * 生命周期实现
 * ======================================================================== */

grpc_server_t *grpc_server_create(int port) {
    grpc_server_t *server = calloc(1, sizeof(grpc_server_t));
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

int grpc_server_start(grpc_server_t *srv) {
    if (!srv) return -1;

    srv->running = true;
    srv->start_time = get_current_time_ms();

    /* 启动请求处理线程 */
    pthread_create(&srv->accept_thread, NULL, request_handler_thread, srv);

    return 0;
}

void grpc_server_stop(grpc_server_t *srv) {
    if (!srv || !srv->running) return;

    srv->running = false;

    if (srv->listen_socket != INVALID_SOCKET) {
        closesocket(srv->listen_socket);
        srv->listen_socket = INVALID_SOCKET;
    }

    pthread_join(srv->accept_thread, NULL);
}

void grpc_server_destroy(grpc_server_t *srv) {
    if (!srv) return;

    if (srv->running) {
        grpc_server_stop(srv);
    }

    WSACleanup();
    pthread_mutex_destroy(&srv->mutex);
    free(srv);
}

void grpc_server_set_callbacks(grpc_server_t *srv, grpc_server_callbacks_t *callbacks) {
    if (!srv || !callbacks) return;
    srv->callbacks = *callbacks;
}

void grpc_server_set_user_data(grpc_server_t *srv, void *user_data) {
    if (!srv) return;
    srv->user_data = user_data;
}
