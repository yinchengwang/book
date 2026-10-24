/**
 * @file include/db/ecosystem/grpc_server.h
 * @brief Ecosystem gRPC 服务器接口
 *
 * 基于 Winsock 的简单 gRPC 服务器。
 * 支持 Query 和 Execute 两个 RPC 方法。
 */
#ifndef DB_ECOSYSTEM_GRPC_SERVER_H
#define DB_ECOSYSTEM_GRPC_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/** gRPC 服务器句柄 */
typedef struct grpc_server grpc_server_t;

/** Query 结果 */
typedef struct {
    int status;              /**< 状态码，0 表示成功 */
    char *message;           /**< 消息 */
    void *data;              /**< 数据 */
    size_t data_len;         /**< 数据长度 */
} grpc_query_result_t;

/** Execute 结果 */
typedef struct {
    int status;              /**< 状态码，0 表示成功 */
    char *message;           /**< 消息 */
    int64_t rows_affected;   /**< 影响行数 */
    int64_t last_insert_id;  /**< 最后插入 ID */
} grpc_execute_result_t;

/** gRPC 服务器回调函数 */
typedef struct {
    /** Query 回调函数 */
    int (*on_query)(const void *request, size_t request_len,
                   void *user_data, grpc_query_result_t *result);
    /** Execute 回调函数 */
    int (*on_execute)(const void *request, size_t request_len,
                     void *user_data, grpc_execute_result_t *result);
} grpc_server_callbacks_t;

/* ========================================================================
 * 生命周期管理
 * ======================================================================== */

/**
 * @brief 创建 gRPC 服务器
 * @param port 监听端口
 * @return 服务器句柄，失败返回 NULL
 */
grpc_server_t *grpc_server_create(int port);

/**
 * @brief 启动 gRPC 服务器
 * @param srv 服务器句柄
 * @return 0 成功，-1 失败
 */
int grpc_server_start(grpc_server_t *srv);

/**
 * @brief 停止 gRPC 服务器
 * @param srv 服务器句柄
 */
void grpc_server_stop(grpc_server_t *srv);

/**
 * @brief 销毁 gRPC 服务器
 * @param srv 服务器句柄
 */
void grpc_server_destroy(grpc_server_t *srv);

/* ========================================================================
 * 配置
 * ======================================================================== */

/**
 * @brief 设置回调函数
 * @param srv 服务器句柄
 * @param callbacks 回调函数结构体
 */
void grpc_server_set_callbacks(grpc_server_t *srv, grpc_server_callbacks_t *callbacks);

/**
 * @brief 设置用户数据
 * @param srv 服务器句柄
 * @param user_data 用户数据指针
 */
void grpc_server_set_user_data(grpc_server_t *srv, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* DB_ECOSYSTEM_GRPC_SERVER_H */
