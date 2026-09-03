/**
 * @file db_server.h
 * @brief 数据库服务器 - 简化的 PostgreSQL wire 协议服务器
 */
#ifndef DB_SERVER_H
#define DB_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 配置
 * ============================================================ */

#define DEFAULT_PORT 5432
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 8192

/* ============================================================
 * 连接状态
 * ============================================================ */

/** 连接状态 */
typedef enum conn_state_e {
    CONN_STATE_IDLE,          /**< 空闲 */
    CONN_STATE_READY,          /**< 就绪 */
    CONN_STATE_PROCESSING,      /**< 处理查询 */
    CONN_STATE_ERROR           /**< 错误 */
} conn_state_t;

/** 连接信息 */
typedef struct connection_s {
    int                  fd;             /**< socket 描述符 */
    conn_state_t        state;          /**< 连接状态 */
    char               *database;       /**< 当前数据库 */
    char               *username;       /**< 当前用户 */
    char                buffer[BUFFER_SIZE];
    int                 buf_len;
    int                 buf_pos;
} connection_t;

/* ============================================================
 * 服务器函数
 * ============================================================ */

/**
 * @brief 启动服务器
 * @param port 端口
 * @param data_dir 数据目录
 * @return 0 成功，-1 失败
 */
int server_start(int port, const char *data_dir);

/**
 * @brief 停止服务器
 */
void server_stop(void);

/**
 * @brief 服务器主循环
 */
void server_run(void);

/**
 * @brief 处理单个连接
 * @param fd socket 描述符
 */
void server_handle_connection(int fd);

/**
 * @brief 执行 SQL
 * @param sql SQL 语句
 * @param output 输出缓冲区
 * @param output_len 输出缓冲区大小
 * @return 0 成功，-1 失败
 */
int server_execute_sql(const char *sql, char *output, size_t output_len);

#ifdef __cplusplus
}
#endif

#endif /* DB_SERVER_H */
