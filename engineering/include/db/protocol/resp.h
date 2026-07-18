/**
 * @file resp.h
 * @brief RESP（Redis 序列化协议）解析与构建
 *
 * 支持 RESP2 和 RESP3 混合模式：
 * - Simple String: +OK\r\n
 * - Error: -ERR message\r\n
 * - Integer: :42\r\n
 * - Bulk String: $6\r\nfoobar\r\n
 * - Array: *3\r\n...\r\n
 * - Null: $-1\r\n / *-1\r\n
 * - Boolean: #t\r\n / #f\r\n (RESP3)
 * - Double: ,1.23\r\n (RESP3)
 * - Map: %2\r\n... (RESP3)
 * - Set: ~3\r\n... (RESP3)
 */
#ifndef DB_PROTOCOL_RESP_H
#define DB_PROTOCOL_RESP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * RESP 类型枚举
 * ======================================================================== */

typedef enum {
    RESP_STRING,        /**< Simple String: +OK\r\n */
    RESP_ERROR,         /**< Error: -ERR message\r\n */
    RESP_INTEGER,       /**< Integer: :42\r\n */
    RESP_BULK_STRING,   /**< Bulk String: $6\r\nfoobar\r\n */
    RESP_ARRAY,         /**< Array: *3\r\n... */
    RESP_NULL,          /**< Null: $-1\r\n / *-1\r\n */
    RESP_BOOLEAN,       /**< Boolean: #t\r\n / #f\r\n (RESP3) */
    RESP_DOUBLE,        /**< Double: ,1.23\r\n (RESP3) */
    RESP_MAP,           /**< Map: %2\r\nkey\r\nval (RESP3) */
    RESP_SET,           /**< Set: ~3\r\n... (RESP3) */
    RESP_PUSH,          /**< Push: >2\r\n... (RESP3, pub/sub) */
    RESP_VERBATIM,      /**< Verbatim: =15\r\ntxt:Hello\r\n (RESP3) */
    RESP_BIG_NUMBER,    /**< Big Number: (123456789\r\n (RESP3) */
    RESP_NIL,           /**< Nil (RESP3): _\r\n */
    RESP_END            /**< 解析结束/未定义 */
} resp_type_t;

/* ========================================================================
 * RESP 值结构
 * ======================================================================== */

/** RESP 值（递归结构） */
typedef struct resp_value_s {
    resp_type_t type;                /**< 值类型 */
    union {
        struct {
            const char *str;         /**< Simple String / Error 消息 */
            int len;                 /**< 字符串长度 */
        } string;
        struct {
            const char *str;         /**< Bulk String 数据 */
            int len;                 /**< Bulk String 长度（-1 表示 null） */
        } bulk_string;
        int64_t integer;             /**< Integer 值 */
        double dval;                 /**< Double 值 */
        bool boolean;                /**< Boolean 值 */
        struct {
            struct resp_value_s *elements; /**< Array/Map/Set 元素 */
            int count;               /**< 元素数量 */
        } collection;
    } data;
} resp_value_t;

/* ========================================================================
 * RESP 解析器
 * ======================================================================== */

/**
 * @brief RESP 解析器上下文
 */
typedef struct resp_parser_s {
    const char *buf;            /**< 输入缓冲区 */
    int buf_len;                /**< 缓冲区长度 */
    int pos;                    /**< 当前解析位置 */
} resp_parser_t;

/**
 * @brief 初始化 RESP 解析器
 */
void resp_parser_init(resp_parser_t *parser, const char *buf, int buf_len);

/**
 * @brief 解析一个 RESP 值
 *
 * @param parser 解析器上下文
 * @param out_value 输出值（调用者负责用 resp_free_value 释放）
 * @return 0 成功，-1 解析错误
 */
int resp_parse_next(resp_parser_t *parser, resp_value_t *out_value);

/**
 * @brief 释放 RESP 值（递归释放）
 */
void resp_free_value(resp_value_t *value);

/* ========================================================================
 * RESP 构建器
 * ======================================================================== */

/**
 * @brief 构建 Simple String
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param str 字符串
 * @return 写入的字节数（不含 NUL），-1 失败
 */
int resp_build_string(char *buf, int buf_size, const char *str);

/**
 * @brief 构建 Error
 */
int resp_build_error(char *buf, int buf_size, const char *msg);

/**
 * @brief 构建 Integer
 */
int resp_build_integer(char *buf, int buf_size, int64_t val);

/**
 * @brief 构建 Bulk String
 * @param str 字符串数据（可为 NULL 表示 null bulk string）
 * @param len 字符串长度（-1 表示 null）
 */
int resp_build_bulk_string(char *buf, int buf_size, const char *str, int len);

/**
 * @brief 构建 Array 头部
 * @param count 元素数量（-1 表示 null array）
 */
int resp_build_array_header(char *buf, int buf_size, int count);

/**
 * @brief 构建 Boolean (RESP3)
 */
int resp_build_boolean(char *buf, int buf_size, bool val);

/**
 * @brief 构建 Double (RESP3)
 */
int resp_build_double(char *buf, int buf_size, double val);

/**
 * @brief 构建 Map 头部 (RESP3)
 */
int resp_build_map_header(char *buf, int buf_size, int count);

/**
 * @brief 构建 OK 响应
 */
int resp_build_ok(char *buf, int buf_size);

/**
 * @brief 构建 PONG 响应
 */
int resp_build_pong(char *buf, int buf_size);

/* ========================================================================
 * RESP 命令解析
 * ======================================================================== */

/**
 * @brief RESP 命令结构
 */
typedef struct resp_command_s {
    char **argv;        /**< 命令参数数组 */
    int argc;           /**< 参数数量 */
} resp_command_t;

/**
 * @brief 解析 RESP 命令（Array of Bulk Strings）
 *
 * @param parser 解析器上下文
 * @param out_cmd 输出命令（调用者负责用 resp_free_command 释放）
 * @return 0 成功，-1 解析错误
 */
int resp_parse_command(resp_parser_t *parser, resp_command_t *out_cmd);

/**
 * @brief 释放 RESP 命令
 */
void resp_free_command(resp_command_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* DB_PROTOCOL_RESP_H */