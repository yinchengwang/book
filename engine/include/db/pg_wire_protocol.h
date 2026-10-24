/**
 * @file pg_wire_protocol.h
 * @brief PostgreSQL Wire Protocol 兼容层
 *
 * 实现 PostgreSQL v3.0 wire 协议，允许标准 PostgreSQL 客户端
 * （psql、pgAdmin、JDBC 等）连接多模态数据库。
 */
#ifndef DB_PG_WIRE_PROTOCOL_H
#define DB_PG_WIRE_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * PostgreSQL 协议常量
 * ============================================================ */

/** 协议版本号 */
#define PG_PROTOCOL_VERSION_MAJOR    3
#define PG_PROTOCOL_VERSION_MINOR    0
#define PG_PROTOCOL_VERSION          ((PG_PROTOCOL_VERSION_MAJOR << 16) | PG_PROTOCOL_VERSION_MINOR)

/** 消息长度占用字节数（含自身） */
#define PG_MSG_LENGTH_SIZE           4

/** 数据类型 OID */
#define PG_TYPE_OID_BOOL             16
#define PG_TYPE_OID_BYTEA            17
#define PG_TYPE_OID_INT8             20
#define PG_TYPE_OID_INT2             21
#define PG_TYPE_OID_INT4             23
#define PG_TYPE_OID_TEXT              25
#define PG_TYPE_OID_FLOAT4           700
#define PG_TYPE_OID_FLOAT8           701
#define PG_TYPE_OID_VARCHAR          1043
#define PG_TYPE_OID_DATE             1082
#define PG_TYPE_OID_TIME             1083
#define PG_TYPE_OID_TIMESTAMP        1114
#define PG_TYPE_OID_TIMESTAMPTZ      1184
#define PG_TYPE_OID_JSON             114
#define PG_TYPE_OID_UUID             2950
#define PG_TYPE_OID_UNKNOWN          0

/** 格式代码 */
#define PG_FORMAT_TEXT               0
#define PG_FORMAT_BINARY             1

/** 最大消息长度 */
#define PG_MAX_MSG_LENGTH            (1 << 20)  /* 1MB */

/* ============================================================
 * 消息类型枚举
 * ============================================================ */

typedef enum {
    /* 前端消息（客户端 -> 服务端） */
    PG_MSG_STARTUP      = 0,     /**< 启动消息（无类型字节） */
    PG_MSG_PASSWORD      = 'p',  /**< 密码消息 */
    PG_MSG_QUERY         = 'Q',  /**< 简单查询 */
    PG_MSG_PARSE         = 'P',  /**< 解析（扩展查询） */
    PG_MSG_BIND          = 'B',  /**< 绑定参数（扩展查询） */
    PG_MSG_EXECUTE       = 'E',  /**< 执行（扩展查询） */
    PG_MSG_DESCRIBE      = 'D',  /**< 描述（扩展查询） */
    PG_MSG_SYNC          = 'S',  /**< 同步（扩展查询） */
    PG_MSG_FLUSH         = 'H',  /**< 刷新 */
    PG_MSG_CLOSE         = 'C',  /**< 关闭语句/门户 */
    PG_MSG_TERMINATE     = 'X',  /**< 终止连接 */

    /* 后端消息（服务端 -> 客户端） */
    PG_MSG_AUTH_REQUEST         = 'R',  /**< 认证请求 */
    PG_MSG_PARAMETER_STATUS    = 'K',  /**< 参数状态 */
    PG_MSG_BACKEND_KEY_DATA    = 'k',  /**< 后端密钥数据 */
    PG_MSG_READY_FOR_QUERY     = 'Z',  /**< 就绪等待查询 */
    PG_MSG_ROW_DESCRIPTION     = 'T',  /**< 行描述 */
    PG_MSG_DATA_ROW            = 'D',  /**< 数据行 */
    PG_MSG_COMMAND_COMPLETE     = 'C',  /**< 命令完成 */
    PG_MSG_EMPTY_QUERY_RESP    = 'I',  /**< 空查询响应 */
    PG_MSG_ERROR_RESPONSE      = 'E',  /**< 错误响应 */
    PG_MSG_NOTICE_RESPONSE     = 'N',  /**< 通知响应 */
    PG_MSG_PARSE_COMPLETE      = '1',  /**< 解析完成 */
    PG_MSG_BIND_COMPLETE       = '2',  /**< 绑定完成 */
    PG_MSG_CLOSE_COMPLETE      = '3',  /**< 关闭完成 */
    PG_MSG_NO_DATA             = 'n',  /**< 无数据 */
    PG_MSG_PARAMETER_DESCRIPTION = 't', /**< 参数描述 */
} pg_message_type_t;

/** 认证子类型 */
typedef enum {
    PG_AUTH_OK                  = 0,   /**< 认证成功 */
    PG_AUTH_KERBEROS_V5        = 2,   /**< Kerberos V5 */
    PG_AUTH_CLEARTEXT_PASSWORD = 3,   /**< 明文密码 */
    PG_AUTH_CRYPT_PASSWORD     = 4,   /**< crypt 密码 */
    PG_AUTH_MD5_PASSWORD       = 5,   /**< MD5 密码 */
    PG_AUTH_SCM_CREDENTIAL     = 6,   /**< SCM 凭据 */
    PG_AUTH_GSS                 = 7,   /**< GSS */
    PG_AUTH_GSS_CONTINUE       = 8,   /**< GSS 继续 */
    PG_AUTH_SSPI               = 9,   /**< SSPI */
    PG_AUTH_SASL               = 10,  /**< SASL */
    PG_AUTH_SASL_CONTINUE      = 11,  /**< SASL 继续 */
    PG_AUTH_SASL_FINAL         = 12,  /**< SASL 最终 */
} pg_auth_type_t;

/** 事务状态指示符 */
typedef enum {
    PG_TXN_IDLE     = 'I',  /**< 空闲（不在事务中） */
    PG_TXN_IN_BLOCK = 'T',  /**< 在事务块中 */
    PG_TXN_FAILED   = 'E',  /**< 事务失败 */
} pg_txn_status_t;

/* ============================================================
 * 错误/通知字段标识符
 * ============================================================ */

typedef enum {
    PG_ERR_SEVERITY    = 'S',  /**< 严重性 */
    PG_ERR_SQLSTATE    = 'C',  /**< SQLSTATE */
    PG_ERR_MESSAGE     = 'M',  /**< 消息文本 */
    PG_ERR_DETAIL      = 'D',  /**< 详细信息 */
    PG_ERR_HINT        = 'H',  /**< 提示 */
    PG_ERR_POSITION    = 'P',  /**< 位置 */
    PG_ERR_INTERNAL_POS = 'p', /**< 内部位置 */
    PG_ERR_SCHEMA      = 's',  /**< 模式名 */
    PG_ERR_TABLE       = 't',  /**< 表名 */
    PG_ERR_COLUMN      = 'c',  /**< 列名 */
    PG_ERR_DATATYPE    = 'd',  /**< 数据类型名 */
    PG_ERR_CONSTRAINT  = 'n',  /**< 约束名 */
    PG_ERR_FILE        = 'F',  /**< 文件 */
    PG_ERR_LINE        = 'L',  /**< 行号 */
    PG_ERR_ROUTINE     = 'R',  /**< 例程名 */
    PG_ERR_END         = '\0', /**< 消息结束标记 */
} pg_err_field_t;

/* ============================================================
 * 连接结构
 * ============================================================ */

/** 连接状态 */
typedef enum {
    PG_CONN_STATE_STARTUP,      /**< 等待启动消息 */
    PG_CONN_STATE_AUTH,         /**< 认证中 */
    PG_CONN_STATE_READY,        /**< 就绪，等待查询 */
    PG_CONN_STATE_QUERY,        /**< 正在处理查询 */
    PG_CONN_STATE_TERMINATED,   /**< 已终止 */
} pg_conn_state_t;

/** 连接配置 */
typedef struct {
    char *server_encoding;      /**< 服务器编码（默认 UTF8） */
    int   integer_datetimes;    /**< 整数日期时间（默认 1） */
    char *client_encoding;      /**< 客户端编码 */
    char *database_encoding;    /**< 数据库编码 */
    char *date_style;           /**< 日期风格 */
    char *interval_style;       /**< 间隔风格 */
    char *timezone;             /**< 时区 */
    char *application_name;     /**< 应用名称 */
} pg_conn_config_t;

/** PostgreSQL 连接上下文 */
typedef struct pg_connection_s {
    int               sock;              /**< socket 描述符 */
    pg_conn_state_t   state;             /**< 连接状态 */
    int               protocol_version;  /**< 协议版本 */
    bool              authenticated;     /**< 是否已认证 */

    /* 连接信息 */
    char             *user;              /**< 用户名 */
    char             *database;          /**< 数据库名 */

    /* 读写缓冲区 */
    char             *read_buf;          /**< 读缓冲区 */
    int               read_buf_size;     /**< 读缓冲区大小 */
    int               read_buf_len;      /**< 读缓冲区中数据长度 */
    int               read_buf_pos;      /**< 读缓冲区当前位置 */

    char             *write_buf;         /**< 写缓冲区 */
    int               write_buf_size;    /**< 写缓冲区大小 */
    int               write_buf_len;     /**< 写缓冲区中数据长度 */

    /* 配置 */
    pg_conn_config_t  config;            /**< 连接配置 */

    /* 扩展查询状态 */
    char             *portal_name;       /**< 当前门户名称 */
    bool              portal_open;       /**< 门户是否打开 */

    /* 私有数据 */
    void             *priv_data;         /**< 引擎层私有数据 */
    void             *executor_data;     /**< 执行器相关数据 */

    /* 追踪 */
    uint64_t          query_count;       /**< 已执行查询数 */
    uint64_t          bytes_sent;        /**< 已发送字节数 */
    uint64_t          bytes_received;    /**< 已接收字节数 */
} pg_connection_t;

/* ============================================================
 * 查询结果结构
 * ============================================================ */

/** 单列描述 */
typedef struct {
    char  *name;           /**< 列名 */
    int32_t table_oid;     /**< 所属表 OID */
    int16_t column_attr;   /**< 列属性编号 */
    int32_t type_oid;      /**< 数据类型 OID */
    int16_t type_len;      /**< 类型长度（-1 表示可变长） */
    int32_t type_mod;      /**< 类型修饰符 */
    int16_t format_code;   /**< 格式代码（0=文本，1=二进制） */
} pg_column_desc_t;

/** 结果集 */
typedef struct pg_result_s {
    pg_column_desc_t *columns;     /**< 列描述数组 */
    int               num_columns; /**< 列数 */
    char            **values;      /**< 值数组（num_rows * num_columns） */
    int              *value_lens;  /**< 值长度数组（-1 表示 NULL） */
    int               num_rows;    /**< 行数 */
    int               num_rows_alloc; /**< 已分配行数 */

    /* 命令完成信息 */
    char             *cmd_status;     /**< 命令标签（如 "SELECT 5"） */
    int64_t           cmd_affected;   /**< 影响行数（如 5） */
    int64_t           cmd_oid;        /**< 插入的 OID（0 表示无） */

    /* 错误信息 */
    bool              is_error;       /**< 是否为错误结果 */
    char             *error_message;  /**< 错误消息 */
    char             *error_severity; /**< 错误严重级别 */
    char             *error_sqlstate; /**< SQLSTATE 错误码 */
} pg_result_t;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief 创建连接上下文
 * @param sock socket 描述符
 * @return 连接上下文，失败返回 NULL
 */
pg_connection_t* pg_connection_create(int sock);

/**
 * @brief 释放连接上下文
 * @param conn 连接上下文
 */
void pg_connection_free(pg_connection_t *conn);

/**
 * @brief 重置连接状态（用于连接复用）
 * @param conn 连接上下文
 * @return 0 成功，-1 失败
 */
int pg_connection_reset(pg_connection_t *conn);

/* ------------------------------------------------------------
 * 读写操作
 * ------------------------------------------------------------ */

/**
 * @brief 从 socket 读取数据到缓冲区
 * @param conn 连接上下文
 * @return 0 成功，-1 连接关闭，-2 错误
 */
int pg_connection_read(pg_connection_t *conn);

/**
 * @brief 将缓冲区数据刷写到 socket
 * @param conn 连接上下文
 * @return 0 成功，-1 失败
 */
int pg_connection_flush(pg_connection_t *conn);

/**
 * @brief 向写缓冲区追加数据
 * @param conn 连接上下文
 * @param data 数据
 * @param len  数据长度
 * @return 0 成功，-1 缓冲区溢出
 */
int pg_connection_write(pg_connection_t *conn, const void *data, int len);

/* ------------------------------------------------------------
 * 协议处理
 * ------------------------------------------------------------ */

/**
 * @brief 处理启动消息
 * @param conn 连接上下文
 * @param data 启动消息数据（不含长度前缀）
 * @param len  数据长度
 * @return 0 成功，-1 失败
 */
int pg_handle_startup(pg_connection_t *conn, const char *data, int len);

/**
 * @brief 处理密码消息
 * @param conn 连接上下文
 * @param data 密码消息数据
 * @param len  数据长度
 * @return 0 成功，-1 失败
 */
int pg_handle_password(pg_connection_t *conn, const char *data, int len);

/**
 * @brief 处理简单查询
 * @param conn 连接上下文
 * @param query SQL 查询字符串
 * @return 0 成功，-1 失败
 */
int pg_handle_query(pg_connection_t *conn, const char *query);

/**
 * @brief 处理终止消息
 * @param conn 连接上下文
 * @return 0 成功
 */
int pg_handle_terminate(pg_connection_t *conn);

/**
 * @brief 处理扩展查询 - Parse
 * @param conn 连接上下文
 * @param stmt_name 语句名称（"" 或 NULL 表示无名语句）
 * @param query     SQL 查询
 * @param num_params 参数数量
 * @param param_oids 参数类型 OID 数组
 * @return 0 成功，-1 失败
 */
int pg_handle_parse(pg_connection_t *conn, const char *stmt_name,
                    const char *query, int num_params, const int32_t *param_oids);

/**
 * @brief 处理扩展查询 - Bind
 * @param conn 连接上下文
 * @param portal_name 门户名称
 * @param stmt_name   语句名称
 * @param num_params  参数数量
 * @param param_formats 参数格式代码数组
 * @param param_values  参数值数组
 * @param param_lens    参数长度数组
 * @param result_format 结果格式代码
 * @return 0 成功，-1 失败
 */
int pg_handle_bind(pg_connection_t *conn, const char *portal_name,
                   const char *stmt_name, int num_params,
                   const int16_t *param_formats,
                   const char *const *param_values,
                   const int32_t *param_lens,
                   int result_format);

/**
 * @brief 处理扩展查询 - Execute
 * @param conn 连接上下文
 * @param portal_name 门户名称
 * @param max_rows 最大返回行数（0 表示不限制）
 * @return 0 成功，-1 失败
 */
int pg_handle_execute(pg_connection_t *conn, const char *portal_name, int max_rows);

/**
 * @brief 处理扩展查询 - Describe
 * @param conn 连接上下文
 * @param target_type 目标类型（'S' 语句，'P' 门户）
 * @param name 名称
 * @return 0 成功，-1 失败
 */
int pg_handle_describe(pg_connection_t *conn, char target_type, const char *name);

/**
 * @brief 处理扩展查询 - Sync
 * @param conn 连接上下文
 * @return 0 成功
 */
int pg_handle_sync(pg_connection_t *conn);

/**
 * @brief 处理扩展查询 - Close
 * @param conn 连接上下文
 * @param target_type 目标类型（'S' 语句，'P' 门户）
 * @param name 名称
 * @return 0 成功
 */
int pg_handle_close(pg_connection_t *conn, char target_type, const char *name);

/* ------------------------------------------------------------
 * 响应发送
 * ------------------------------------------------------------ */

/**
 * @brief 发送认证成功
 * @param conn 连接上下文
 * @return 0 成功，-1 失败
 */
int pg_send_auth_ok(pg_connection_t *conn);

/**
 * @brief 发送参数状态消息
 * @param conn 连接上下文
 * @param name  参数名
 * @param value 参数值
 * @return 0 成功，-1 失败
 */
int pg_send_parameter_status(pg_connection_t *conn, const char *name, const char *value);

/**
 * @brief 发送后端密钥数据
 * @param conn 连接上下文
 * @param pid   进程 ID
 * @param key   密钥
 * @return 0 成功，-1 失败
 */
int pg_send_backend_key_data(pg_connection_t *conn, int32_t pid, int32_t key);

/**
 * @brief 发送 ReadyForQuery
 * @param conn 连接上下文
 * @return 0 成功，-1 失败
 */
int pg_send_ready_for_query(pg_connection_t *conn);

/**
 * @brief 发送错误响应
 * @param conn 连接上下文
 * @param severity 严重级别
 * @param sqlstate SQLSTATE 码
 * @param message  消息文本
 * @return 0 成功，-1 失败
 */
int pg_send_error_response(pg_connection_t *conn, const char *severity,
                           const char *sqlstate, const char *message);

/**
 * @brief 发送 RowDescription
 * @param conn 连接上下文
 * @param columns 列描述数组
 * @param num_columns 列数
 * @return 0 成功，-1 失败
 */
int pg_send_row_description(pg_connection_t *conn, const pg_column_desc_t *columns,
                            int num_columns);

/**
 * @brief 发送 DataRow
 * @param conn 连接上下文
 * @param values 值数组
 * @param value_lens 值长度数组（-1 表示 NULL）
 * @param num_columns 列数
 * @return 0 成功，-1 失败
 */
int pg_send_data_row(pg_connection_t *conn, const char *const *values,
                     const int32_t *value_lens, int num_columns);

/**
 * @brief 发送 CommandComplete
 * @param conn 连接上下文
 * @param tag 命令标签（如 "SELECT 5"）
 * @return 0 成功，-1 失败
 */
int pg_send_command_complete(pg_connection_t *conn, const char *tag);

/**
 * @brief 发送空查询响应
 * @param conn 连接上下文
 * @return 0 成功，-1 失败
 */
int pg_send_empty_query_response(pg_connection_t *conn);

/**
 * @brief 发送 ParseComplete
 * @param conn 连接上下文
 * @return 0 成功，-1 失败
 */
int pg_send_parse_complete(pg_connection_t *conn);

/**
 * @brief 发送 BindComplete
 * @param conn 连接上下文
 * @return 0 成功，-1 失败
 */
int pg_send_bind_complete(pg_connection_t *conn);

/**
 * @brief 发送 CloseComplete
 * @param conn 连接上下文
 * @return 0 成功，-1 失败
 */
int pg_send_close_complete(pg_connection_t *conn);

/* ------------------------------------------------------------
 * 结果集操作
 * ------------------------------------------------------------ */

/**
 * @brief 创建空结果集
 * @return 结果集，失败返回 NULL
 */
pg_result_t* pg_result_create(void);

/**
 * @brief 释放结果集
 * @param result 结果集
 */
void pg_result_free(pg_result_t *result);

/**
 * @brief 设置列描述
 * @param result 结果集
 * @param columns 列描述数组（会拷贝）
 * @param num_columns 列数
 * @return 0 成功，-1 失败
 */
int pg_result_set_columns(pg_result_t *result, const pg_column_desc_t *columns,
                          int num_columns);

/**
 * @brief 添加一行数据
 * @param result 结果集
 * @param values 值数组
 * @param value_lens 值长度数组（-1 表示 NULL）
 * @param num_columns 列数
 * @return 0 成功，-1 失败
 */
int pg_result_add_row(pg_result_t *result, const char *const *values,
                      const int32_t *value_lens, int num_columns);

/**
 * @brief 设置命令完成信息
 * @param result 结果集
 * @param tag 命令标签
 * @param affected 影响行数
 * @param oid 插入的 OID
 */
void pg_result_set_command(pg_result_t *result, const char *tag,
                           int64_t affected, int64_t oid);

/**
 * @brief 设置错误信息
 * @param result 结果集
 * @param severity 严重级别
 * @param sqlstate SQLSTATE 码
 * @param message 消息文本
 */
void pg_result_set_error(pg_result_t *result, const char *severity,
                         const char *sqlstate, const char *message);

/**
 * @brief 发送完整结果集（RowDescription + DataRows + CommandComplete）
 * @param conn 连接上下文
 * @param result 结果集
 * @return 0 成功，-1 失败
 */
int pg_send_result(pg_connection_t *conn, pg_result_t *result);

/* ------------------------------------------------------------
 * 工具函数
 * ------------------------------------------------------------ */

/**
 * @brief 构建命令完成标签
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param tag 命令类型（如 "SELECT", "INSERT"）
 * @param oid 插入的 OID（0 表示无）
 * @param affected 影响行数
 * @return buf 指针
 */
char* pg_build_command_tag(char *buf, size_t buf_size, const char *tag,
                           int64_t oid, int64_t affected);

/**
 * @brief 根据 C 类型推断 PG 类型 OID
 * @param c_type C 类型名
 * @return PG 类型 OID
 */
int32_t pg_type_oid_from_c_type(const char *c_type);

#ifdef __cplusplus
}
#endif

#endif /* DB_PG_WIRE_PROTOCOL_H */
