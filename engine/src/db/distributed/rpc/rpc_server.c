/**
 * @file rpc_server.c
 * @brief RPC 服务端实现
 *
 * 实现基于 TCP 的 RPC 服务端，支持多线程处理请求。
 */
#include "db/distributed/rpc.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** 最大并发处理线程数 */
#define RPC_SERVER_MAX_THREADS 32

/** 监听队列大小 */
#define RPC_SERVER_BACKLOG 128

/* ========================================================================
 * 客户端处理上下文
 * ======================================================================== */

/**
 * @brief 客户端处理上下文
 */
typedef struct client_context_s {
    int fd;                         /**< 客户端 socket */
    struct sockaddr_in addr;        /**< 客户端地址 */
    rpc_server_t *server;           /**< 所属服务端 */
    pthread_t thread;               /**< 处理线程 */
    bool is_running;                /**< 是否运行中 */
} client_context_t;

/* ========================================================================
 * 服务端处理线程
 * ======================================================================== */

/**
 * @brief 处理单个客户端连接
 */
static void* client_handler_thread(void *arg)
{
    client_context_t *ctx = (client_context_t *)arg;
    rpc_server_t *server = ctx->server;

    LOG_DEBUG("客户端连接: %s:%d",
              inet_ntoa(ctx->addr.sin_addr), ntohs(ctx->addr.sin_port));

    while (ctx->is_running && server->is_running) {
        /* 接收请求头 */
        rpc_message_header_t req_header;
        ssize_t n = recv(ctx->fd, &req_header, sizeof(req_header), 0);
        if (n <= 0) {
            if (n < 0 && errno != ECONNRESET) {
                LOG_ERROR("接收请求头失败: %s", strerror(errno));
            }
            break;
        }

        if (n != sizeof(req_header)) {
            LOG_ERROR("请求头大小不匹配: %zd != %zu", n, sizeof(req_header));
            break;
        }

        /* 检查魔数 */
        if (req_header.magic != RPC_MAGIC) {
            LOG_ERROR("请求魔数无效: 0x%08X", req_header.magic);
            break;
        }

        /* 检查消息类型 */
        if (req_header.type != RPC_MSG_REQUEST && req_header.type != RPC_MSG_HEARTBEAT) {
            LOG_ERROR("不支持的消息类型: %d", req_header.type);
            break;
        }

        /* 处理心跳 */
        if (req_header.type == RPC_MSG_HEARTBEAT) {
            rpc_message_header_t ack_header;
            memset(&ack_header, 0, sizeof(ack_header));
            ack_header.magic = RPC_MAGIC;
            ack_header.version = RPC_PROTOCOL_VERSION;
            ack_header.type = RPC_MSG_HEARTBEAT_ACK;
            ack_header.request_id = req_header.request_id;
            ack_header.payload_size = 0;
            ack_header.checksum = rpc_crc32(&ack_header, sizeof(ack_header) - sizeof(uint32_t));

            send(ctx->fd, &ack_header, sizeof(ack_header), 0);
            continue;
        }

        /* 接收请求体 */
        void *req_payload = NULL;
        if (req_header.payload_size > 0) {
            if (req_header.payload_size > RPC_MAX_MESSAGE_SIZE) {
                LOG_ERROR("请求体过大: %u", req_header.payload_size);
                break;
            }

            req_payload = malloc(req_header.payload_size);
            if (req_payload == NULL) {
                LOG_ERROR("请求体内存分配失败");
                break;
            }

            n = recv(ctx->fd, req_payload, req_header.payload_size, 0);
            if (n != (ssize_t)req_header.payload_size) {
                LOG_ERROR("接收请求体失败");
                free(req_payload);
                break;
            }
        }

        /* 构建请求消息 */
        rpc_message_t request;
        request.header = req_header;
        request.payload = req_payload;

        /* 构建响应消息 */
        rpc_message_t response;
        memset(&response, 0, sizeof(response));
        response.header.magic = RPC_MAGIC;
        response.header.version = RPC_PROTOCOL_VERSION;
        response.header.type = RPC_MSG_RESPONSE;
        response.header.request_id = req_header.request_id;

        /* 调用处理器 */
        if (server->handler.handle_request != NULL) {
            int rc = server->handler.handle_request(&request, &response, server->handler.user_data);
            if (rc != 0) {
                LOG_WARN("请求处理器返回错误: %d", rc);
                response.header.payload_size = 0;
            }
        }

        /* 序列化响应 */
        void *resp_buffer = NULL;
        uint32_t resp_buffer_size = 0;
        if (rpc_message_serialize(&response, &resp_buffer, &resp_buffer_size) != 0) {
            LOG_ERROR("响应序列化失败");
            free(req_payload);
            free(response.payload);
            break;
        }

        /* 发送响应 */
        ssize_t sent = 0;
        while (sent < (ssize_t)resp_buffer_size) {
            ssize_t n = send(ctx->fd, (char *)resp_buffer + sent, resp_buffer_size - sent, 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                LOG_ERROR("发送响应失败: %s", strerror(errno));
                break;
            }
            sent += n;
        }

        free(resp_buffer);
        free(req_payload);
        free(response.payload);

        if (sent != (ssize_t)resp_buffer_size) {
            break;
        }
    }

    LOG_DEBUG("客户端断开: %s:%d",
              inet_ntoa(ctx->addr.sin_addr), ntohs(ctx->addr.sin_port));

    close(ctx->fd);
    ctx->is_running = false;
    return NULL;
}

/* ========================================================================
 * RPC 服务端实现
 * ======================================================================== */

rpc_server_t* rpc_server_create(const rpc_node_address_t *bind_addr,
                                 const rpc_server_handler_t *handler)
{
    if (bind_addr == NULL || handler == NULL) {
        LOG_ERROR("参数为空");
        return NULL;
    }

    rpc_server_t *server = (rpc_server_t *)calloc(1, sizeof(rpc_server_t));
    if (server == NULL) {
        LOG_ERROR("服务端内存分配失败");
        return NULL;
    }

    server->listen_fd = -1;
    server->bind_addr = *bind_addr;
    server->handler = *handler;
    server->is_running = false;

    pthread_mutex_init(&server->lock, NULL);

    LOG_INFO("创建 RPC 服务端: %s:%u", bind_addr->host, bind_addr->port);
    return server;
}

void rpc_server_destroy(rpc_server_t *server)
{
    if (server == NULL) return;

    if (server->is_running) {
        rpc_server_stop(server);
    }

    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }

    pthread_mutex_destroy(&server->lock);
    free(server);

    LOG_DEBUG("销毁 RPC 服务端");
}

int rpc_server_start(rpc_server_t *server)
{
    if (server == NULL) {
        return -1;
    }

    /* 创建 socket */
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        LOG_ERROR("创建 socket 失败: %s", strerror(errno));
        return -1;
    }

    /* 设置 SO_REUSEADDR */
    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->bind_addr.port);

    if (server->bind_addr.host[0] != '\0') {
        addr.sin_addr.s_addr = inet_addr(server->bind_addr.host);
    } else {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("绑定地址失败: %s", strerror(errno));
        close(server->listen_fd);
        server->listen_fd = -1;
        return -1;
    }

    /* 开始监听 */
    if (listen(server->listen_fd, RPC_SERVER_BACKLOG) < 0) {
        LOG_ERROR("监听失败: %s", strerror(errno));
        close(server->listen_fd);
        server->listen_fd = -1;
        return -1;
    }

    server->is_running = true;
    LOG_INFO("RPC 服务端启动: %s:%u", server->bind_addr.host, server->bind_addr.port);
    return 0;
}

int rpc_server_stop(rpc_server_t *server)
{
    if (server == NULL) {
        return -1;
    }

    server->is_running = false;

    /* 关闭监听 socket */
    if (server->listen_fd >= 0) {
        shutdown(server->listen_fd, SHUT_RDWR);
        close(server->listen_fd);
        server->listen_fd = -1;
    }

    LOG_INFO("RPC 服务端停止");
    return 0;
}