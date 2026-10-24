/**
 * @file mysql_wire_protocol.h
 * @brief MySQL 线协议兼容层
 *
 * 实现 MySQL 4.1+ 线协议，允许标准 MySQL 客户端连接多模态数据库。
 */
#ifndef MYSQL_WIRE_PROTOCOL_H
#define MYSQL_WIRE_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * MySQL 消息类型定义
 * ============================================================ */

typedef enum {
    MYSQL_MSG_QUERY            = 0x03,  /**< 查询 */
    MYSQL_MSG_FIELD_LIST       = 0x04,  /**< 字段列表 */
    MYSQL_MSG_INIT_DB          = 0x02,  /**< 初始化数据库 */
    MYSQL_MSG_STATISTICS       = 0x09,  /**< 统计信息 */
    MYSQL_MSG_PROCESS_INFO    = 0x0A,  /**< 进程信息 */
    MYSQL_MSG_PROCESS_KILL    = 0x0C,  /**< 终止进程 */
    MYSQL_MSG_PING             = 0x0E,  /**< 心跳检测 */
    MYSQL_MSG_SET_OPTION       = 0x1B,  /**< 设置选项 */
    MYSQL_MSG_FETCH            = 0x1C,  /**< 获取数据 */
    MYSQL_MSG_STMT_EXECUTE    = 0x17,  /**< 预处理执行 */
} mysql_message_type_t;

/* ============================================================
 * MySQL 数据类型定义
 * ============================================================ */

typedef enum {
    MYSQL_TYPE_DECIMAL     = 0x00,
    MYSQL_TYPE_TINY        = 0x01,
    MYSQL_TYPE_SHORT       = 0x02,
    MYSQL_TYPE_LONG        = 0x03,
    MYSQL_TYPE_FLOAT       = 0x04,
    MYSQL_TYPE_DOUBLE      = 0x05,
    MYSQL_TYPE_NULL        = 0x06,
    MYSQL_TYPE_TIMESTAMP   = 0x07,
    MYSQL_TYPE_LONGLONG    = 0x08,
    MYSQL_TYPE_INT24       = 0x09,
    MYSQL_TYPE_DATE        = 0x0A,
    MYSQL_TYPE_TIME        = 0x0B,
    MYSQL_TYPE_DATETIME    = 0x0C,
    MYSQL_TYPE_YEAR        = 0x0D,
    MYSQL_TYPE_VARCHAR     = 0xFE,  /**< 新协议中 VARCHAR 使用此类型 */
    MYSQL_TYPE_BLOB        = 0xFC,
    MYSQL_TYPE_STRING      = 0xFD,
    MYSQL_TYPE_GEOMETRY    = 0xFF,
} mysql_field_type_t;

/* ============================================================
 * 连接状态
 * ============================================================ */

typedef struct mysql_connection_s {
    int     sock;              /**< socket 描述符 */
    char   *user;              /**< 用户名 */
    char   *database;          /**< 当前数据库 */
    uint32_t packet_id;        /**< 包序列号 */
    bool    authenticated;     /**< 是否已认证 */
    void   *priv_data;         /**< 私有数据（指向数据库上下文） */
} mysql_connection_t;

/* ============================================================
 * 列定义
 * ============================================================ */

typedef struct mysql_column_def_s {
    char    *catalog;          /**< 目录名 */
    char    *schema;           /**< 库名 */
    char    *table;            /**< 表名（别名） */
    char    *org_table;        /**< 原始表名 */
    char    *name;             /**< 列名（别名） */
    char    *org_name;         /**< 原始列名 */
    uint16_t charsetnr;        /**< 字符集编号 */
    uint32_t length;           /**< 列长度 */
    uint8_t  type;             /**< 列类型（mysql_field_type_t） */
    uint16_t flags;            /**< 列标志 */
    uint8_t  decimals;         /**< 小数位数 */
} mysql_column_def_t;

/* ============================================================
 * 结果集
 * ============================================================ */

typedef struct mysql_result_s {
    mysql_column_def_t *columns;   /**< 列定义数组 */
    char              **rows;      /**< 行数据数组（每行为 NULL 分隔的字段字符串） */
    int                 num_columns;  /**< 列数 */
    int                 num_rows;     /**< 行数 */
    int                 error_code;   /**< 错误码（0 表示成功） */
    char               *error_msg;    /**< 错误消息 */
} mysql_result_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief 创建连接对象
 * @param sock socket 描述符
 * @return 连接对象指针，失败返回 NULL
 */
mysql_connection_t* mysql_connection_create(int sock);

/**
 * @brief 释放连接对象
 * @param conn 连接对象
 */
void mysql_connection_free(mysql_connection_t *conn);

/**
 * @brief 处理 MySQL 握手握手流程
 * @param conn 连接对象
 * @return 0 成功，-1 失败
 */
int mysql_handle_handshake(mysql_connection_t *conn);

/**
 * @brief 处理查询请求
 * @param conn 连接对象
 * @param query SQL 查询字符串
 * @return 0 成功，-1 失败
 */
int mysql_handle_query(mysql_connection_t *conn, const char *query);

/**
 * @brief 发送错误包
 * @param conn 连接对象
 * @param code 错误码
 * @param sqlstate SQLSTATE 字符串
 * @param message 错误消息
 * @return 0 成功，-1 失败
 */
int mysql_send_error(mysql_connection_t *conn, uint16_t code,
                     const char *sqlstate, const char *message);

/**
 * @brief 发送结果集
 * @param conn 连接对象
 * @param result 结果集
 * @return 0 成功，-1 失败
 */
int mysql_send_result(mysql_connection_t *conn, mysql_result_t *result);

/**
 * @brief 发送 OK 包
 * @param conn 连接对象
 * @param affected_rows 受影响行数
 * @param last_insert_id 最后插入 ID
 * @return 0 成功，-1 失败
 */
int mysql_send_ok(mysql_connection_t *conn, uint64_t affected_rows,
                  uint64_t last_insert_id);

/**
 * @brief 发送 EOF 包
 * @param conn 连接对象
 * @param warnings 警告数
 * @return 0 成功，-1 失败
 */
int mysql_send_eof(mysql_connection_t *conn, uint16_t warnings);

/**
 * @brief 发送字段包（列定义）
 * @param conn 连接对象
 * @param column 列定义
 * @return 0 成功，-1 失败
 */
int mysql_send_column_def(mysql_connection_t *conn,
                          const mysql_column_def_t *column);

/**
 * @brief 发送行数据
 * @param conn 连接对象
 * @param fields 字段值数组
 * @param lengths 字段长度数组
 * @param num_fields 字段数
 * @return 0 成功，-1 失败
 */
int mysql_send_row(mysql_connection_t *conn, const char * const *fields,
                   const uint32_t *lengths, int num_fields);

/**
 * @brief 发送行数据（NULL 值标记为 0xFB）
 * @param conn 连接对象
 * @param fields 字段值数组（NULL 表示 NULL 值）
 * @param num_fields 字段数
 * @return 0 成功，-1 失败
 */
int mysql_send_row_nullable(mysql_connection_t *conn,
                            const char * const *fields, int num_fields);

/**
 * @brief 初始化结果集
 * @param result 结果集
 * @param num_columns 列数
 */
void mysql_result_init(mysql_result_t *result, int num_columns);

/**
 * @brief 释放结果集
 * @param result 结果集
 */
void mysql_result_free(mysql_result_t *result);

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief MySQL 包编号自增
 * @param conn 连接对象
 * @return 当前包编号
 */
uint32_t mysql_next_packet_id(mysql_connection_t *conn);

/**
 * @brief 读取 MySQL 包
 * @param conn 连接对象
 * @param buffer 接收缓冲区
 * @param max_len 缓冲区大小
 * @return 包体长度，失败返回 -1
 */
int mysql_read_packet(mysql_connection_t *conn, void *buffer, size_t max_len);

/**
 * @brief 发送原始数据
 * @param conn 连接对象
 * @param data 数据
 * @param len 数据长度
 * @return 0 成功，-1 失败
 */
int mysql_send_raw(mysql_connection_t *conn, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MYSQL_WIRE_PROTOCOL_H */
