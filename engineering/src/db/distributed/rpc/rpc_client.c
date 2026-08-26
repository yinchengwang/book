/**
 * @file rpc_client.c
 * @brief RPC 客户端实现
 *
 * 实现基于 TCP 的 RPC 客户端，包括连接池管理、消息序列化/反序列化、超时重试等。
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
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

/* ========================================================================
 * 内部常量
 * ======================================================================== */

/** RPC 魔数 "RPC1" */
#define RPC_MAGIC 0x52504331

/** 协议版本 */
#define RPC_PROTOCOL_VERSION 1

/* ========================================================================
 * CRC32 实现
 * ======================================================================== */

static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void init_crc32_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

uint32_t rpc_crc32(const void *data, size_t size)
{
    if (!crc32_table_initialized) {
        init_crc32_table();
    }

    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

/* ========================================================================
 * 错误处理
 * ======================================================================== */

const char* rpc_error_string(rpc_error_code_t error_code)
{
    switch (error_code) {
        case RPC_OK:                return "成功";
        case RPC_ERR_NETWORK:       return "网络错误";
        case RPC_ERR_TIMEOUT:       return "超时";
        case RPC_ERR_CONNECTION:    return "连接失败";
        case RPC_ERR_PROTOCOL:      return "协议错误";
        case RPC_ERR_MEMORY:        return "内存错误";
        case RPC_ERR_NOT_FOUND:     return "节点未找到";
        case RPC_ERR_INVALID_PARAM: return "参数无效";
        case RPC_ERR_POOL_EXHAUSTED:return "连接池耗尽";
        case RPC_ERR_CLOSED:        return "连接已关闭";
        default:                    return "未知错误";
    }
}

/* ========================================================================
 * 连接管理
 * ======================================================================== */

static rpc_connection_t* connection_create(const char *host, uint16_t port)
{
    rpc_connection_t *conn = (rpc_connection_t *)calloc(1, sizeof(rpc_connection_t));
    if (conn == NULL) {
        LOG_ERROR("连接内存分配失败");
        return NULL;
    }

    conn->fd = -1;
    conn->is_connected = false;
    conn->is_busy = false;
    conn->last_active_time = 0;

    /* 设置地址 */
    strncpy(conn->addr.host, host, sizeof(conn->addr.host) - 1);
    conn->addr.port = port;

    return conn;
}

static void connection_destroy(rpc_connection_t *conn)
{
    if (conn == NULL) return;

    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }

    conn->is_connected = false;
    free(conn);
}

static int connection_connect(rpc_connection_t *conn, uint32_t timeout_ms)
{
    if (conn == NULL || conn->fd >= 0) {
        return -1;
    }

    /* 创建 socket */
    conn->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->fd < 0) {
        LOG_ERROR("创建 socket 失败: %s", strerror(errno));
        return -1;
    }

    /* 设置非阻塞模式 */
    int flags = fcntl(conn->fd, F_GETFL, 0);
    fcntl(conn->fd, F_SETFL, flags | O_NONBLOCK);

    /* 解析主机名 */
    struct hostent *he = gethostbyname(conn->addr.host);
    if (he == NULL) {
        LOG_ERROR("解析主机名失败: %s", conn->addr.host);
        close(conn->fd);
        conn->fd = -1;
        return -1;
    }

    /* 连接 */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(conn->addr.port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    int ret = connect(conn->fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0 && errno != EINPROGRESS) {
        LOG_ERROR("连接失败: %s", strerror(errno));
        close(conn->fd);
        conn->fd = -1;
        return -1;
    }

    /* 等待连接完成 */
    if (ret < 0) {
        fd_set write_fds;
        struct timeval tv;

        FD_ZERO(&write_fds);
        FD_SET(conn->fd, &write_fds);

        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        ret = select(conn->fd + 1, NULL, &write_fds, NULL, &tv);
        if (ret <= 0) {
            LOG_ERROR("连接超时");
            close(conn->fd);
            conn->fd = -1;
            return -1;
        }

        /* 检查是否连接成功 */
        int error = 0;
        socklen_t error_len = sizeof(error);
        getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &error, &error_len);
        if (error != 0) {
            LOG_ERROR("连接失败: %s", strerror(error));
            close(conn->fd);
            conn->fd = -1;
            return -1;
        }
    }

    /* 恢复阻塞模式 */
    fcntl(conn->fd, F_SETFL, flags);

    conn->is_connected = true;
    conn->last_active_time = (uint64_t)time(NULL) * 1000;

    LOG_DEBUG("连接成功: %s:%u", conn->addr.host, conn->addr.port);
    return 0;
}

static int connection_send(rpc_connection_t *conn, const void *data, size_t size)
{
    if (conn == NULL || conn->fd < 0) {
        return -1;
    }

    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(conn->fd, (const char *)data + sent, size - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_ERROR("发送超时");
                return -1;
            }
            LOG_ERROR("发送失败: %s", strerror(errno));
            return -1;
        }
        sent += n;
    }

    conn->last_active_time = (uint64_t)time(NULL) * 1000;
    return 0;
}

static int connection_recv(rpc_connection_t *conn, void *data, size_t size)
{
    if (conn == NULL || conn->fd < 0) {
        return -1;
    }

    size_t received = 0;
    while (received < size) {
        ssize_t n = recv(conn->fd, (char *)data + received, size - received, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_ERROR("接收超时");
                return -1;
            }
            LOG_ERROR("接收失败: %s", strerror(errno));
            return -1;
        }
        if (n == 0) {
            LOG_ERROR("连接关闭");
            return -1;
        }
        received += n;
    }

    conn->last_active_time = (uint64_t)time(NULL) * 1000;
    return 0;
}

/* ========================================================================
 * 连接池实现
 * ======================================================================== */

static rpc_connection_pool_t* pool_create(const rpc_config_t *config)
{
    rpc_connection_pool_t *pool = (rpc_connection_pool_t *)calloc(1, sizeof(rpc_connection_pool_t));
    if (pool == NULL) {
        LOG_ERROR("连接池内存分配失败");
        return NULL;
    }

    pool->capacity = config->pool_size;
    pool->active_count = 0;
    pool->config = *config;

    pool->connections = (rpc_connection_t *)calloc(pool->capacity, sizeof(rpc_connection_t));
    if (pool->connections == NULL) {
        LOG_ERROR("连接数组内存分配失败");
        free(pool);
        return NULL;
    }

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->cond, NULL);

    /* 初始化所有连接 */
    for (uint32_t i = 0; i < pool->capacity; i++) {
        pool->connections[i].fd = -1;
        pool->connections[i].is_connected = false;
        pool->connections[i].is_busy = false;
    }

    LOG_DEBUG("创建连接池: 容量=%u", pool->capacity);
    return pool;
}

static void pool_destroy(rpc_connection_pool_t *pool)
{
    if (pool == NULL) return;

    /* 关闭所有连接 */
    for (uint32_t i = 0; i < pool->capacity; i++) {
        if (pool->connections[i].fd >= 0) {
            close(pool->connections[i].fd);
        }
    }

    free(pool->connections);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
    free(pool);

    LOG_DEBUG("销毁连接池");
}

static rpc_connection_t* pool_acquire(rpc_connection_pool_t *pool, uint32_t timeout_ms)
{
    if (pool == NULL) return NULL;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&pool->lock);

    /* 查找空闲连接 */
    while (true) {
        for (uint32_t i = 0; i < pool->capacity; i++) {
            rpc_connection_t *conn = &pool->connections[i];
            if (!conn->is_busy && conn->is_connected) {
                conn->is_busy = true;
                pthread_mutex_unlock(&pool->lock);
                return conn;
            }
        }

        /* 没有空闲连接，等待 */
        if (timeout_ms == 0) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }

        int rc = pthread_cond_timedwait(&pool->cond, &pool->lock, &ts);
        if (rc == ETIMEDOUT) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }
    }
}

static void pool_release(rpc_connection_pool_t *pool, rpc_connection_t *conn)
{
    if (pool == NULL || conn == NULL) return;

    pthread_mutex_lock(&pool->lock);
    conn->is_busy = false;
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
}

/* ========================================================================
 * 消息操作实现
 * ======================================================================== */

rpc_message_t* rpc_message_alloc(uint32_t payload_size)
{
    rpc_message_t *msg = (rpc_message_t *)calloc(1, sizeof(rpc_message_t));
    if (msg == NULL) {
        return NULL;
    }

    if (payload_size > 0) {
        msg->payload = malloc(payload_size);
        if (msg->payload == NULL) {
            free(msg);
            return NULL;
        }
    }

    msg->header.magic = RPC_MAGIC;
    msg->header.version = RPC_PROTOCOL_VERSION;
    msg->header.payload_size = payload_size;

    return msg;
}

void rpc_message_free(rpc_message_t *msg)
{
    if (msg == NULL) return;
    if (msg->payload != NULL) {
        free(msg->payload);
    }
    free(msg);
}

int rpc_message_serialize(const rpc_message_t *msg, void **buffer, uint32_t *buffer_size)
{
    if (msg == NULL || buffer == NULL || buffer_size == NULL) {
        return -1;
    }

    uint32_t total_size = sizeof(rpc_message_header_t) + msg->header.payload_size;
    *buffer = malloc(total_size);
    if (*buffer == NULL) {
        return -1;
    }

    /* 复制消息头 */
    memcpy(*buffer, &msg->header, sizeof(rpc_message_header_t));

    /* 复制消息体 */
    if (msg->header.payload_size > 0 && msg->payload != NULL) {
        memcpy((char *)*buffer + sizeof(rpc_message_header_t), msg->payload, msg->header.payload_size);
    }

    /* 计算校验和 */
    rpc_message_header_t *header = (rpc_message_header_t *)*buffer;
    header->checksum = rpc_crc32(*buffer, total_size - sizeof(uint32_t));

    *buffer_size = total_size;
    return 0;
}

rpc_message_t* rpc_message_deserialize(const void *buffer, uint32_t buffer_size)
{
    if (buffer == NULL || buffer_size < sizeof(rpc_message_header_t)) {
        return NULL;
    }

    const rpc_message_header_t *header = (const rpc_message_header_t *)buffer;

    /* 检查魔数 */
    if (header->magic != RPC_MAGIC) {
        LOG_ERROR("无效的魔数: 0x%08X", header->magic);
        return NULL;
    }

    /* 检查版本 */
    if (header->version != RPC_PROTOCOL_VERSION) {
        LOG_ERROR("不支持的协议版本: %u", header->version);
        return NULL;
    }

    /* 检查大小 */
    if (header->payload_size > RPC_MAX_MESSAGE_SIZE) {
        LOG_ERROR("消息体过大: %u", header->payload_size);
        return NULL;
    }

    uint32_t expected_size = sizeof(rpc_message_header_t) + header->payload_size;
    if (buffer_size < expected_size) {
        LOG_ERROR("缓冲区大小不足: %u < %u", buffer_size, expected_size);
        return NULL;
    }

    /* 验证校验和 */
    uint32_t expected_checksum = rpc_crc32(buffer, buffer_size - sizeof(uint32_t));
    if (header->checksum != expected_checksum) {
        LOG_ERROR("校验和不匹配: 0x%08X != 0x%08X", header->checksum, expected_checksum);
        return NULL;
    }

    /* 创建消息 */
    rpc_message_t *msg = rpc_message_alloc(header->payload_size);
    if (msg == NULL) {
        return NULL;
    }

    /* 复制消息头 */
    memcpy(&msg->header, header, sizeof(rpc_message_header_t));

    /* 复制消息体 */
    if (header->payload_size > 0) {
        memcpy(msg->payload, (const char *)buffer + sizeof(rpc_message_header_t), header->payload_size);
    }

    return msg;
}

/* ========================================================================
 * RPC 客户端实现
 * ======================================================================== */

rpc_client_t* rpc_client_create(const rpc_config_t *config)
{
    if (config == NULL) {
        LOG_ERROR("配置参数为空");
        return NULL;
    }

    rpc_client_t *client = (rpc_client_t *)calloc(1, sizeof(rpc_client_t));
    if (client == NULL) {
        LOG_ERROR("客户端内存分配失败");
        return NULL;
    }

    client->config = *config;
    client->is_running = true;
    client->next_request_id = 1;

    /* 创建连接池 */
    client->pool = pool_create(config);
    if (client->pool == NULL) {
        LOG_ERROR("连接池创建失败");
        free(client);
        return NULL;
    }

    LOG_INFO("创建 RPC 客户端: %s:%u", config->server_addr.host, config->server_addr.port);
    return client;
}

void rpc_client_destroy(rpc_client_t *client)
{
    if (client == NULL) return;

    client->is_running = false;

    if (client->pool != NULL) {
        pool_destroy(client->pool);
    }

    free(client);
    LOG_DEBUG("销毁 RPC 客户端");
}

int rpc_client_send_request(rpc_client_t *client, uint32_t request_id,
                            const void *payload, uint32_t payload_size,
                            rpc_message_t *response)
{
    if (client == NULL || response == NULL) {
        return RPC_ERR_INVALID_PARAM;
    }

    if (!client->is_running) {
        return RPC_ERR_CLOSED;
    }

    /* 从连接池获取连接 */
    rpc_connection_t *conn = pool_acquire(client->pool, client->config.connect_timeout_ms);
    if (conn == NULL) {
        LOG_ERROR("获取连接失败");
        return RPC_ERR_POOL_EXHAUSTED;
    }

    /* 如果未连接，尝试连接 */
    if (!conn->is_connected) {
        int rc = connection_connect(conn, client->config.connect_timeout_ms);
        if (rc != 0) {
            pool_release(client->pool, conn);
            return RPC_ERR_CONNECTION;
        }
    }

    /* 构建请求消息 */
    rpc_message_t *request = rpc_message_alloc(payload_size);
    if (request == NULL) {
        pool_release(client->pool, conn);
        return RPC_ERR_MEMORY;
    }

    request->header.type = RPC_MSG_REQUEST;
    request->header.request_id = request_id;

    if (payload != NULL && payload_size > 0) {
        memcpy(request->payload, payload, payload_size);
    }

    /* 序列化请求 */
    void *buffer = NULL;
    uint32_t buffer_size = 0;
    if (rpc_message_serialize(request, &buffer, &buffer_size) != 0) {
        rpc_message_free(request);
        pool_release(client->pool, conn);
        return RPC_ERR_MEMORY;
    }

    /* 发送请求 */
    int send_rc = connection_send(conn, buffer, buffer_size);
    free(buffer);
    rpc_message_free(request);

    if (send_rc != 0) {
        LOG_ERROR("发送请求失败");
        pool_release(client->pool, conn);
        return RPC_ERR_NETWORK;
    }

    /* 接收响应 */
    rpc_message_header_t resp_header;
    if (connection_recv(conn, &resp_header, sizeof(resp_header)) != 0) {
        LOG_ERROR("接收响应头失败");
        pool_release(client->pool, conn);
        return RPC_ERR_NETWORK;
    }

    /* 检查响应头 */
    if (resp_header.magic != RPC_MAGIC) {
        LOG_ERROR("响应魔数无效");
        pool_release(client->pool, conn);
        return RPC_ERR_PROTOCOL;
    }

    if (resp_header.request_id != request_id) {
        LOG_ERROR("响应请求 ID 不匹配: %u != %u", resp_header.request_id, request_id);
        pool_release(client->pool, conn);
        return RPC_ERR_PROTOCOL;
    }

    /* 接收响应体 */
    void *resp_payload = NULL;
    if (resp_header.payload_size > 0) {
        resp_payload = malloc(resp_header.payload_size);
        if (resp_payload == NULL) {
            pool_release(client->pool, conn);
            return RPC_ERR_MEMORY;
        }

        if (connection_recv(conn, resp_payload, resp_header.payload_size) != 0) {
            LOG_ERROR("接收响应体失败");
            free(resp_payload);
            pool_release(client->pool, conn);
            return RPC_ERR_NETWORK;
        }
    }

    /* 构建响应消息 */
    response->header = resp_header;
    response->payload = resp_payload;

    pool_release(client->pool, conn);
    return RPC_OK;
}

int rpc_client_call(rpc_client_t *client, const void *payload, uint32_t payload_size,
                    rpc_message_t *response)
{
    if (client == NULL) {
        return RPC_ERR_INVALID_PARAM;
    }

    uint32_t request_id = __sync_fetch_and_add(&client->next_request_id, 1);

    /* 带重试的调用 */
    for (uint32_t retry = 0; retry <= client->config.max_retry_count; retry++) {
        int rc = rpc_client_send_request(client, request_id, payload, payload_size, response);
        if (rc == RPC_OK) {
            return RPC_OK;
        }

        if (rc == RPC_ERR_PROTOCOL || rc == RPC_ERR_INVALID_PARAM) {
            /* 协议错误不重试 */
            return rc;
        }

        if (retry < client->config.max_retry_count) {
            uint32_t interval = client->config.retry_base_interval_ms * (1 << retry);
            LOG_WARN("RPC 调用失败，%ums 后重试 (%u/%u): %s",
                     interval, retry + 1, client->config.max_retry_count, rpc_error_string(rc));
            usleep(interval * 1000);
        }
    }

    return RPC_ERR_TIMEOUT;
}