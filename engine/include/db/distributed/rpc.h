/**
 * @file rpc.h
 * @brief 分布式 RPC 通信接口
 *
 * 提供基于 TCP 的节点间 RPC 调用机制，支持请求/响应序列化、连接池管理和超时重试。
 */
#ifndef DB_DISTRIBUTED_RPC_H
#define DB_DISTRIBUTED_RPC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** RPC 消息最大大小 (64MB) */
#define RPC_MAX_MESSAGE_SIZE (64 * 1024 * 1024)

/** 默认连接超时时间 (5s) */
#define RPC_DEFAULT_CONNECT_TIMEOUT_MS 5000

/** 默认请求超时时间 (30s) */
#define RPC_DEFAULT_REQUEST_TIMEOUT_MS 30000

/** 默认连接池大小 */
#define RPC_DEFAULT_POOL_SIZE 8

/** 最大重试次数 */
#define RPC_MAX_RETRY_COUNT 3

/** 重试间隔基数 (ms) */
#define RPC_RETRY_BASE_INTERVAL_MS 100

/* ========================================================================
 * 错误码定义
 * ======================================================================== */

/** RPC 错误码 */
typedef enum {
    RPC_OK = 0,                     /**< 成功 */
    RPC_ERR_NETWORK = -1,           /**< 网络错误 */
    RPC_ERR_TIMEOUT = -2,           /**< 超时 */
    RPC_ERR_CONNECTION = -3,        /**< 连接失败 */
    RPC_ERR_PROTOCOL = -4,          /**< 协议错误 */
    RPC_ERR_MEMORY = -5,            /**< 内存错误 */
    RPC_ERR_NOT_FOUND = -6,         /**< 节点未找到 */
    RPC_ERR_INVALID_PARAM = -7,     /**< 参数无效 */
    RPC_ERR_POOL_EXHAUSTED = -8,    /**< 连接池耗尽 */
    RPC_ERR_CLOSED = -9,            /**< 连接已关闭 */
} rpc_error_code_t;

/* ========================================================================
 * 消息类型定义
 * ======================================================================== */

/** RPC 消息类型 */
typedef enum {
    RPC_MSG_REQUEST = 0x01,         /**< 请求消息 */
    RPC_MSG_RESPONSE = 0x02,        /**< 响应消息 */
    RPC_MSG_HEARTBEAT = 0x03,       /**< 心跳消息 */
    RPC_MSG_HEARTBEAT_ACK = 0x04,   /**< 心跳响应 */
} rpc_message_type_t;

/** RPC 消息标志 */
typedef enum {
    RPC_FLAG_NONE = 0,              /**< 无标志 */
    RPC_FLAG_MORE_DATA = 0x01,      /**< 后续还有数据 */
    RPC_FLAG_COMPRESSED = 0x02,     /**< 数据已压缩 */
    RPC_FLAG_ENCRYPTED = 0x04,      /**< 数据已加密 */
} rpc_message_flag_t;

/* ========================================================================
 * 数据结构定义
 * ======================================================================== */

/**
 * @brief RPC 消息头
 *
 * 固定 24 字节，用于消息序列化/反序列化
 */
typedef struct rpc_message_header_s {
    uint32_t magic;                 /**< 魔数 0x52504331 ("RPC1") */
    uint8_t version;                /**< 协议版本 */
    uint8_t type;                   /**< 消息类型 (rpc_message_type_t) */
    uint16_t flags;                 /**< 消息标志 (rpc_message_flag_t) */
    uint32_t request_id;            /**< 请求 ID (用于匹配请求/响应) */
    uint32_t payload_size;          /**< 消息体大小 */
    uint32_t checksum;              /**< 校验和 (CRC32) */
} rpc_message_header_t;

/**
 * @brief RPC 消息
 */
typedef struct rpc_message_s {
    rpc_message_header_t header;    /**< 消息头 */
    void *payload;                  /**< 消息体 (动态分配) */
} rpc_message_t;

/**
 * @brief RPC 节点地址
 */
typedef struct rpc_node_address_s {
    char host[256];                 /**< 主机名或 IP */
    uint16_t port;                  /**< 端口号 */
    uint64_t node_id;               /**< 节点 ID */
} rpc_node_address_t;

/**
 * @brief RPC 连接配置
 */
typedef struct rpc_config_s {
    rpc_node_address_t server_addr; /**< 服务端地址 */
    uint32_t connect_timeout_ms;    /**< 连接超时 */
    uint32_t request_timeout_ms;    /**< 请求超时 */
    uint32_t pool_size;             /**< 连接池大小 */
    uint32_t max_retry_count;       /**< 最大重试次数 */
    uint32_t retry_base_interval_ms;/**< 重试间隔基数 */
} rpc_config_t;

/**
 * @brief RPC 连接
 */
typedef struct rpc_connection_s {
    int fd;                         /**< socket 文件描述符 */
    rpc_node_address_t addr;        /**< 远端地址 */
    bool is_connected;              /**< 是否已连接 */
    bool is_busy;                   /**< 是否正在使用 */
    uint64_t last_active_time;      /**< 最后活跃时间 (ms) */
} rpc_connection_t;

/**
 * @brief RPC 连接池
 */
typedef struct rpc_connection_pool_s {
    rpc_connection_t *connections;  /**< 连接数组 */
    uint32_t capacity;              /**< 池容量 */
    uint32_t active_count;          /**< 活跃连接数 */
    pthread_mutex_t lock;           /**< 互斥锁 */
    pthread_cond_t cond;            /**< 条件变量 */
    rpc_config_t config;            /**< 配置 */
} rpc_connection_pool_t;

/**
 * @brief RPC 客户端
 */
typedef struct rpc_client_s {
    rpc_connection_pool_t *pool;    /**< 连接池 */
    rpc_config_t config;            /**< 配置 */
    bool is_running;                /**< 是否运行中 */
    uint32_t next_request_id;       /**< 下一个请求 ID */
} rpc_client_t;

/**
 * @brief RPC 服务端处理器
 */
typedef struct rpc_server_handler_s {
    /**
     * @brief 处理请求的回调函数
     *
     * @param request 请求消息
     * @param response 响应消息 (需要填充)
     * @param user_data 用户数据
     * @return 0 成功，非 0 失败
     */
    int (*handle_request)(const rpc_message_t *request, rpc_message_t *response, void *user_data);
    void *user_data;                /**< 用户数据 */
} rpc_server_handler_t;

/**
 * @brief RPC 服务端
 */
typedef struct rpc_server_s {
    int listen_fd;                  /**< 监听 socket */
    rpc_node_address_t bind_addr;   /**< 绑定地址 */
    rpc_server_handler_t handler;   /**< 请求处理器 */
    bool is_running;                /**< 是否运行中 */
    pthread_t accept_thread;        /**< 接受连接线程 */
    pthread_mutex_t lock;           /**< 互斥锁 */
} rpc_server_t;

/* ========================================================================
 * RPC 客户端 API
 * ======================================================================== */

/**
 * @brief 创建 RPC 客户端
 *
 * @param config 客户端配置
 * @return 成功返回客户端指针，失败返回 NULL
 */
rpc_client_t* rpc_client_create(const rpc_config_t *config);

/**
 * @brief 销毁 RPC 客户端
 *
 * @param client RPC 客户端
 */
void rpc_client_destroy(rpc_client_t *client);

/**
 * @brief 发送 RPC 请求
 *
 * @param client RPC 客户端
 * @param request_id 请求 ID
 * @param payload 请求数据
 * @param payload_size 请求数据大小
 * @param response 响应消息 (调用者负责释放)
 * @return 0 成功，非 0 失败
 */
int rpc_client_send_request(rpc_client_t *client, uint32_t request_id,
                            const void *payload, uint32_t payload_size,
                            rpc_message_t *response);

/**
 * @brief 发送 RPC 请求并等待响应 (同步调用)
 *
 * @param client RPC 客户端
 * @param payload 请求数据
 * @param payload_size 请求数据大小
 * @param response 响应消息 (调用者负责释放)
 * @return 0 成功，非 0 失败
 */
int rpc_client_call(rpc_client_t *client, const void *payload, uint32_t payload_size,
                    rpc_message_t *response);

/* ========================================================================
 * RPC 服务端 API
 * ======================================================================== */

/**
 * @brief 创建 RPC 服务端
 *
 * @param bind_addr 绑定地址
 * @param handler 请求处理器
 * @return 成功返回服务端指针，失败返回 NULL
 */
rpc_server_t* rpc_server_create(const rpc_node_address_t *bind_addr,
                                 const rpc_server_handler_t *handler);

/**
 * @brief 销毁 RPC 服务端
 *
 * @param server RPC 服务端
 */
void rpc_server_destroy(rpc_server_t *server);

/**
 * @brief 启动 RPC 服务端
 *
 * @param server RPC 服务端
 * @return 0 成功，非 0 失败
 */
int rpc_server_start(rpc_server_t *server);

/**
 * @brief 停止 RPC 服务端
 *
 * @param server RPC 服务端
 * @return 0 成功，非 0 失败
 */
int rpc_server_stop(rpc_server_t *server);

/* ========================================================================
 * 消息操作 API
 * ======================================================================== */

/**
 * @brief 分配消息
 *
 * @param payload_size 消息体大小
 * @return 成功返回消息指针，失败返回 NULL
 */
rpc_message_t* rpc_message_alloc(uint32_t payload_size);

/**
 * @brief 释放消息
 *
 * @param msg 消息
 */
void rpc_message_free(rpc_message_t *msg);

/**
 * @brief 序列化消息到缓冲区
 *
 * @param msg 消息
 * @param buffer 输出缓冲区 (调用者负责释放)
 * @param buffer_size 输出缓冲区大小
 * @return 0 成功，非 0 失败
 */
int rpc_message_serialize(const rpc_message_t *msg, void **buffer, uint32_t *buffer_size);

/**
 * @brief 从缓冲区反序列化消息
 *
 * @param buffer 输入缓冲区
 * @param buffer_size 缓冲区大小
 * @return 成功返回消息指针，失败返回 NULL
 */
rpc_message_t* rpc_message_deserialize(const void *buffer, uint32_t buffer_size);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取错误码描述
 *
 * @param error_code 错误码
 * @return 错误描述字符串
 */
const char* rpc_error_string(rpc_error_code_t error_code);

/**
 * @brief 计算 CRC32 校验和
 *
 * @param data 数据
 * @param size 数据大小
 * @return CRC32 值
 */
uint32_t rpc_crc32(const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_RPC_H */