/**
 * @file pgwire.h
 * @brief PostgreSQL Wire Protocol 头文件
 *
 * 实现 PostgreSQL 兼容协议：
 * 1. 协议框架和状态机
 * 2. StartupMessage 握手
 * 3. 简单查询协议
 * 4. 扩展查询协议
 * 5. 类型系统
 */
#ifndef DB_PGWIRE_H
#define DB_PGWIRE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 协议常量
 * ======================================================================== */

/** 协议版本号 */
#define PGWIRE_PROTOCOL_VERSION 0x00030000  /* 3.0 */
#define PGWIRE_PROTOCOL_MIN 0x00020000     /* 2.0 */

/** 消息类型 */
#define PGWIRE_MSG_AUTHENTICATE        'R'
#define PGWIRE_MSG_BACKEND_KEY_DATA    'K'
#define PGWIRE_MSG_BIND                'B'
#define PGWIRE_MSG_BIND_COMPLETE       '2'
#define PGWIRE_MSG_CANCEL_REQUEST      ' '
#define PGWIRE_MSG_CLOSE               'C'
#define PGWIRE_MSG_CLOSE_COMPLETE       '3'
#define PGWIRE_MSG_COMMAND_COMPLETE     'C'
#define PGWIRE_MSG_COPY_DATA           'd'
#define PGWIRE_MSG_COPY_DONE           'c'
#define PGWIRE_MSG_COPY_IN_RESPONSE     'G'
#define PGWIRE_MSG_COPY_OUT_RESPONSE    'H'
#define PGWIRE_MSG_COPY_BOTH_RESPONSE   'W'
#define PGWIRE_MSG_DATA_ROW            'D'
#define PGWIRE_MSG_DESCRIBE            'D'
#define PGWIRE_MSG_EMPTY_QUERY         'I'
#define PGWIRE_MSG_ERROR_RESPONSE       'E'
#define PGWIRE_MSG_EXECUTE             'E'
#define PGWIRE_MSG_FLUSH               'H'
#define PGWIRE_MSG_FUNCTION_CALL        'F'
#define PGWIRE_MSG_FUNCTION_CALL_RESPONSE 'V'
#define PGWIRE_MSG_GSSENC              'p'
#define PGWIRE_MSG_GSS_RESPONSE        'p'
#define PGWIRE_MSG_PARSE              'P'
#define PGWIRE_MSG_PARSE_COMPLETE       '1'
#define PGWIRE_MSG_PASSWORD_MESSAGE     'p'
#define PGWIRE_MSG_POSSIBLE_PRFX_NAMES 'N'
#define PGWIRE_MSG_QUERY                'Q'
#define PGWIRE_MSG_READY_FOR_QUERY      'Z'
#define PGWIRE_MSG_ROW_DESCRIPTION      'T'
#define PGWIRE_MSG_SASL_INITIAL_RESPONSE 'p'
#define PGWIRE_MSG_SASL_RESPONSE        'p'
#define PGWIRE_MSG_SYNC                 'S'
#define PGWIRE_MSG_TERMINATE           'X'

/** 认证类型 */
#define PGWIRE_AUTH_OK                 0
#define PGWIRE_AUTH_KRB4               1
#define PGWIRE_AUTH_KRB5               2
#define PGWIRE_AUTH_TRUST              3
#define PGWIRE_AUTH_CRYPT              4
#define PGWIRE_AUTH_MD5                5
#define PGWIRE_AUTH_SCM_CRE            6
#define PGWIRE_AUTH_GSS                7
#define PGWIRE_AUTH_GSS_CONTINUE       8
#define PGWIRE_AUTH_SASL              10
#define PGWIRE_AUTH_SASL_CONTINUE     11
#define PGWIRE_AUTH_SASL_FINAL        12

/** 事务状态 */
typedef enum PgwireTxnState_e {
    PGWIRE_TXN_IDLE,          /**< 空闲 */
    PGWIRE_TXN_BLOCK,         /**< 块中（事务中） */
    PGWIRE_TXN_ERR            /**< 错误状态 */
} PgwireTxnState;

/** 连接状态 */
typedef enum PgwireConnState_e {
    PGWIRE_CONN_IDLE,         /**< 空闲 */
    PGWIRE_CONN_AUTH,         /**< 认证中 */
    PGWIRE_CONN_READY,        /**< 就绪 */
    PGWIRE_CONN_PROCESSING,   /**< 处理中 */
    PGWIRE_CONN_ERROR         /**< 错误 */
} PgwireConnState;

/** 查询状态 */
typedef enum PgwireQueryState_e {
    PGWIRE_QUERY_IDLE,        /**< 空闲 */
    PGWIRE_QUERY_PARSE,       /**< 解析中 */
    PGWIRE_QUERY_BIND,        /**< 绑定中 */
    PGWIRE_QUERY_EXECUTE,     /**< 执行中 */
    PGWIRE_QUERY_DONE         /**< 完成 */
} PgwireQueryState;

/* ========================================================================
 * 消息结构
 * ======================================================================== */

/** 消息头 */
typedef struct PgwireMsgHeader_s {
    int32_t length;           /**< 消息长度（包含自身） */
    char type;               /**< 消息类型 */
} PgwireMsgHeader;

/** StartupMessage */
typedef struct PgwireStartupMsg_s {
    int protocol_version;      /**< 协议版本 */
    int nparams;              /**< 参数数量 */
    char *params;             /**< 参数列表（name\0value\0...） */
} PgwireStartupMsg;

/** 认证请求 */
typedef struct PgwireAuth_s {
    int authtype;             /**< 认证类型 */
    char *data;              /**< 认证数据（如 Salt） */
} PgwireAuth;

/** 错误响应 */
typedef struct PgwireError_s {
    char *sqlstate;           /**< SQL 状态码 */
    char *message;            /**< 错误消息 */
    char *detail;            /**< 详细消息 */
    char *hint;              /**< 提示 */
    char *file;              /**< 源文件 */
    int line;                /**< 源行号 */
    char *routine;           /**< 函数名 */
} PgwireError;

/** 行描述 */
typedef struct PgwireRowDesc_s {
    int nfields;              /**< 字段数 */
    struct PgwireField_s {
        char *name;           /**< 字段名 */
        int table_oid;        /**< 表 OID */
        int column_oid;       /**< 列 OID */
        int type_oid;         /**< 类型 OID */
        int type_len;         /**< 类型长度 */
        int type_mod;         /**< 类型修饰符 */
        int format;           /**< 格式（0=文本，1=二进制） */
    } *fields;
} PgwireRowDesc;

/** 数据行 */
typedef struct PgwireDataRow_s {
    int nfields;              /**< 字段数 */
    int32_t *lengths;        /**< 各字段长度 */
    char **values;           /**< 各字段值 */
} PgwireDataRow;

/** Parse 消息 */
typedef struct PgwireParse_s {
    char *name;              /**< 预处理语句名 */
    char *query;             /**< SQL 查询 */
    int nparams;              /**< 参数数量 */
    int *param_types;        /**< 参数类型 OID */
} PgwireParse;

/** Bind 消息 */
typedef struct PgwireBind_s {
    char *portal;            /**< Portal 名 */
    char *statement;         /**< 预处理语句名 */
    int nformats;             /**< 格式码数量 */
    int16_t *formats;        /**< 格式码（0=文本，1=二进制） */
    int nvalues;             /**< 值数量 */
    int32_t *value_lengths; /**< 值长度 */
    char **values;          /**< 值 */
    int nresult_formats;     /**< 结果格式数量 */
    int16_t *result_formats; /**< 结果格式 */
} PgwireBind;

/** Execute 消息 */
typedef struct PgwireExecute_s {
    char *portal;            /**< Portal 名 */
    int max_rows;            /**< 最大返回行数 */
} PgwireExecute;

/** 命令完成 */
typedef struct PgwireCommandComplete_s {
    char *tag;               /**< 命令标签 */
} PgwireCommandComplete;

/* ========================================================================
 * 类型映射
 * ======================================================================== */

/** PostgreSQL 内置类型 OID */
typedef enum PgTypeOid_e {
    PGTYPE_BOOL          = 16,
    PGTYPE_BYTEA         = 17,
    PGTYPE_CHAR          = 18,
    PGTYPE_NAME          = 19,
    PGTYPE_INT8          = 20,
    PGTYPE_INT2          = 21,
    PGTYPE_INT2VECTOR    = 22,
    PGTYPE_INT4          = 23,
    PGTYPE_REGPROC       = 24,
    PGTYPE_TEXT          = 25,
    PGTYPE_OID           = 26,
    PGTYPE_TID           = 27,
    PGTYPE_XID           = 28,
    PGTYPE_CID           = 29,
    PGTYPE_VECTOR         = 30,
    PGTYPE_JSON          = 114,
    PGTYPE_XML           = 142,
    PGTYPE_PG_NODE_TREE  = 194,
    PGTYPE_SMGR          = 210,
    PGTYPE_PATH          = 602,
    PGTYPE_POLYGON       = 604,
    PGTYPE_CIDR          = 650,
    PGTYPE_FLOAT4        = 700,
    PGTYPE_FLOAT8        = 701,
    PGTYPE_ABSTIME       = 702,
    PGTYPE_RELTIME       = 703,
    PGTYPE_TINTERVAL     = 704,
    PGTYPE_CIRCLE        = 718,
    PGTYPE_CASH          = 790,
    PGTYPE_MACADDR       = 829,
    PGTYPE_INET          = 869,
    PGTYPE_BOOLARRAY     = 1000,
    PGTYPE_INT2ARRAY     = 1005,
    PGTYPE_INT4ARRAY     = 1007,
    PGTYPE_TEXTARRAY     = 1009,
    PGTYPE_OIDARRAY      = 1028,
    PGTYPE_CHARARRAY     = 1002,
    PGTYPE_NAMEARRAY     = 1003,
    PGTYPE_BPCHARARRAY   = 1014,
    PGTYPE_VARCHARARRAY  = 1015,
    PGTYPE_INT2VECTORARRAY = 1006,
    PGTYPE_TIMESTAMPTZ   = 1184,
    PGTYPE_INTERVAL      = 1186,
    PGTYPE_LSEG          = 601,
    PGTYPE_POINT         = 600,
    PGTYPE_BOX           = 603,
    PGTYPE_NUMERIC       = 1700,
    PGTYPE_UNKNOWN       = 705,
    PGTYPE_DATE          = 1082,
    PGTYPE_TIME          = 1083,
    PGTYPE_TIMESTAMPTZARRAY = 1185,
    PGTYPE_TIMESTAMP     = 1114,
    PGTYPE_TIMESTAMPTZ   = 1184,

    /* 自定义类型 */
    PGTYPE_VECTOR_F32    = 2000,
    PGTYPE_VECTOR_F16    = 2001,
    PGTYPE_TIMESERIES    = 3000,
    PGTYPE_JSONB         = 3802,
    PGTYPE_JSONBARRAY    = 3807,
    PGTYPE_DOCUMENT      = 4000,
    PGTYPE_GRAPH         = 5000,
    PGTYPE_SPACIAL       = 6000
} PgTypeOid;

/** 类型信息 */
typedef struct PgTypeInfo_s {
    int oid;                 /**< 类型 OID */
    const char *name;        /**< 类型名 */
    int len;                /**< 长度（-1=变长） */
    int alignment;          /**< 对齐要求 */
    int byval;              /**< 是否按值传递 */
} PgTypeInfo;

/* ========================================================================
 * Portal 和 Prepared Statement
 * ======================================================================== */

/** Portal 状态 */
typedef struct PgwirePortal_s {
    char name[64];           /**< Portal 名 */
    struct PgwirePreparedStmt_s *stmt; /**< 关联的预处理语句 */
    void *tuple_store;       /**< 结果元组存储 */
    int cursor_pos;         /**< 游标位置 */
    bool finished;           /**< 是否完成 */
} PgwirePortal;

/** 预处理语句状态 */
typedef struct PgwirePreparedStmt_s {
    char name[64];           /**< 语句名 */
    char query[4096];        /**< SQL 查询 */
    int nparams;             /**< 参数数 */
    int *param_types;       /**< 参数类型 OID */
    void *plan;             /**< 执行计划 */
    bool valid;              /**< 是否有效 */
} PgwirePreparedStmt;

/* ========================================================================
 * 连接结构
 * ======================================================================== */

/** 客户端信息 */
typedef struct PgwireClientInfo_s {
    char *user;             /**< 用户名 */
    char *database;         /**< 数据库名 */
    char *app_name;         /**< 应用名 */
    char *client_addr;      /**< 客户端地址 */
    int client_port;        /**< 客户端端口 */
} PgwireClientInfo;

/** pgwire 连接 */
typedef struct PgwireConn_s {
    int fd;                         /**< socket fd */
    PgwireConnState state;          /**< 连接状态 */
    PgwireTxnState txn_state;       /**< 事务状态 */
    PgwireQueryState query_state;   /**< 查询状态 */
    PgwireClientInfo client;        /**< 客户端信息 */

    /* 读取缓冲区 */
    char *read_buf;                /**< 读取缓冲区 */
    int read_pos;                  /**< 读取位置 */
    int read_len;                  /**< 已读取长度 */
    int read_capacity;             /**< 缓冲区容量 */

    /* 写入缓冲区 */
    char *write_buf;               /**< 写入缓冲区 */
    int write_pos;                 /**< 写入位置 */
    int write_capacity;            /**< 缓冲区容量 */

    /* Portal 列表 */
    PgwirePortal *portals;         /**< Portal 数组 */
    int nportals;                  /**< Portal 数量 */
    int max_portals;              /**< 最大 Portal 数 */

    /* Prepared Statement 列表 */
    PgwirePreparedStmt *stmts;     /**< 预处理语句数组 */
    int nstmts;                   /**< 预处理语句数量 */
    int max_stmts;                /**< 最大预处理语句数 */

    /* 后端密钥 */
    int32_t backend_key;          /**< 后端密钥 */
    int32_t secret_key;           /**< 密钥 */

    /* 错误信息 */
    char error_msg[512];          /**< 错误消息 */
    bool in_error;                /**< 是否在错误状态 */

    /* 用户数据 */
    void *user_data;              /**< 用户数据 */
} PgwireConn;

/* ========================================================================
 * 会话结构
 * ======================================================================== */

/** pgwire 会话 */
typedef struct PgwireSession_s {
    int64_t session_id;           /**< 会话 ID */
    PgwireConn *conn;             /**< 连接 */
    char *user;                  /**< 当前用户 */
    char *database;               /**< 当前数据库 */
    void *txn_ctx;               /**< 事务上下文 */
    void *catalog;               /**< 目录 */
    int xact_id;                  /**< 当前事务 ID */
    bool is_autocommit;           /**< 是否自动提交 */
    int64_t xact_start_time;      /**< 事务开始时间 */
    void *portal_list;           /**< Portal 列表 */
    void *prepared_list;         /**< 预处理语句列表 */
} PgwireSession;

/* ========================================================================
 * 回调函数类型
 * ======================================================================== */

/** 认证回调 */
typedef int (*PgwireAuthCallback)(PgwireConn *conn, const char *user,
                                 const char *password);

/** SQL 执行回调 */
typedef int (*PgwireExecCallback)(PgwireSession *session, const char *query,
                                 void **result);

/** 参数处理回调 */
typedef const char *(*PgwireParamCallback)(PgwireSession *session,
                                           const char *name);

/* ========================================================================
 * pgwire 服务器
 * ======================================================================== */

/** pgwire 服务器配置 */
typedef struct PgwireServerConfig_s {
    int port;                    /**< 监听端口 */
    int max_connections;         /**< 最大连接数 */
    int shared_buffers;          /**< 共享缓冲区大小 */
    int work_mem;               /**< 工作内存 */
    int auth_method;            /**< 认证方法 */
    char *auth_file;            /**< 认证文件 */
    bool ssl_enabled;            /**< 是否启用 SSL */
    char *ssl_cert_file;        /**< SSL 证书 */
    char *ssl_key_file;         /**< SSL 密钥 */
} PgwireServerConfig;

/** pgwire 服务器 */
typedef struct PgwireServer_s {
    int listen_fd;               /**< 监听 fd */
    PgwireServerConfig config;   /**< 配置 */
    PgwireAuthCallback auth_cb;  /**< 认证回调 */
    PgwireExecCallback exec_cb;  /**< 执行回调 */
    PgwireParamCallback param_cb; /**< 参数回调 */
    PgwireConn **connections;   /**< 连接数组 */
    int nconnections;           /**< 连接数 */
    int max_connections;        /**< 最大连接数 */
    void *user_data;            /**< 用户数据 */
} PgwireServer;

/* ========================================================================
 * 消息读写函数
 * ======================================================================== */

/**
 * @brief 读取消息头
 */
int pgwire_read_msg_header(PgwireConn *conn, PgwireMsgHeader *header);

/**
 * @brief 读取消息体
 */
int pgwire_read_msg_body(PgwireConn *conn, const PgwireMsgHeader *header,
                        void *buf, int len);

/**
 * @brief 读取 StartupMessage
 */
int pgwire_read_startup(PgwireConn *conn, PgwireStartupMsg *msg);

/**
 * @brief 读取密码消息
 */
int pgwire_read_password(PgwireConn *conn, char *password, int max_len);

/**
 * @brief 读取 Query 消息
 */
int pgwire_read_query(PgwireConn *conn, char *query, int max_len);

/**
 * @brief 读取 Parse 消息
 */
int pgwire_read_parse(PgwireConn *conn, PgwireParse *msg);

/**
 * @brief 读取 Bind 消息
 */
int pgwire_read_bind(PgwireConn *conn, PgwireBind *msg);

/**
 * @brief 读取 Execute 消息
 */
int pgwire_read_execute(PgwireConn *conn, PgwireExecute *msg);

/**
 * @brief 写入认证请求
 */
int pgwire_write_auth(PgwireConn *conn, int authtype, const char *data);

/**
 * @brief 写入错误响应
 */
int pgwire_write_error(PgwireConn *conn, const PgwireError *error);

/**
 * @brief 写入行描述
 */
int pgwire_write_row_desc(PgwireConn *conn, const PgwireRowDesc *desc);

/**
 * @brief 写入数据行
 */
int pgwire_write_data_row(PgwireConn *conn, const PgwireDataRow *row);

/**
 * @brief 写入命令完成
 */
int pgwire_write_command_complete(PgwireConn *conn, const char *tag);

/**
 * @brief 写入 ReadyForQuery
 */
int pgwire_write_ready(PgwireConn *conn, PgwireTxnState state);

/**
 * @brief 写入 ParseComplete
 */
int pgwire_write_parse_complete(PgwireConn *conn);

/**
 * @brief 写入 BindComplete
 */
int pgwire_write_bind_complete(PgwireConn *conn);

/**
 * @brief 写入 CloseComplete
 */
int pgwire_write_close_complete(PgwireConn *conn);

/**
 * @brief 写入 ParameterStatus
 */
int pgwire_write_param_status(PgwireConn *conn, const char *name,
                              const char *value);

/**
 * @brief 写入 BackendKeyData
 */
int pgwire_write_backend_key(PgwireConn *conn, int32_t backend_key,
                            int32_t secret_key);

/**
 * @brief 发送所有缓冲数据
 */
int pgwire_flush(PgwireConn *conn);

/* ========================================================================
 * 连接管理
 * ======================================================================== */

/**
 * @brief 创建连接
 */
PgwireConn *pgwire_conn_create(int fd);

/**
 * @brief 销毁连接
 */
void pgwire_conn_destroy(PgwireConn *conn);

/**
 * @brief 重置连接
 */
void pgwire_conn_reset(PgwireConn *conn);

/**
 * @brief 处理连接数据
 */
int pgwire_conn_process(PgwireConn *conn);

/**
 * @brief 获取连接错误
 */
const char *pgwire_conn_error(PgwireConn *conn);

/**
 * @brief 创建会话
 */
PgwireSession *pgwire_session_create(PgwireConn *conn);

/**
 * @brief 销毁会话
 */
void pgwire_session_destroy(PgwireSession *session);

/**
 * @brief 绑定会话参数
 */
void pgwire_session_set_param(PgwireSession *session, const char *name,
                              const char *value);

/**
 * @brief 获取会话参数
 */
const char *pgwire_session_get_param(PgwireSession *session,
                                     const char *name);

/* ========================================================================
 * Portal 和 Prepared Statement
 * ======================================================================== */

/**
 * @brief 创建 Portal
 */
PgwirePortal *pgwire_portal_create(PgwireConn *conn, const char *name);

/**
 * @brief 获取 Portal
 */
PgwirePortal *pgwire_portal_get(PgwireConn *conn, const char *name);

/**
 * @brief 关闭 Portal
 */
void pgwire_portal_close(PgwireConn *conn, const char *name);

/**
 * @brief 创建 Prepared Statement
 */
PgwirePreparedStmt *pgwire_stmt_create(PgwireConn *conn, const char *name);

/**
 * @brief 获取 Prepared Statement
 */
PgwirePreparedStmt *pgwire_stmt_get(PgwireConn *conn, const char *name);

/**
 * @brief 关闭 Prepared Statement
 */
void pgwire_stmt_close(PgwireConn *conn, const char *name);

/* ========================================================================
 * 类型系统
 * ======================================================================== */

/**
 * @brief 获取类型信息
 */
const PgTypeInfo *pgwire_get_type_info(int oid);

/**
 * @brief 根据名称查找类型
 */
int pgwire_lookup_type(const char *name);

/**
 * @brief 将值转换为二进制格式
 */
int pgwire_encode_binary(int oid, const char *value, char *buf, int buflen);

/**
 * @brief 将二进制格式转换为值
 */
int pgwire_decode_binary(int oid, const char *buf, int buflen, char *value,
                        int maxlen);

/**
 * @brief 将值转换为文本格式
 */
int pgwire_encode_text(int oid, const char *value, char *buf, int buflen);

/**
 * @brief OID 数组转文本
 */
const char *pgwire_type_array_oid(int oid);

/* ========================================================================
 * 服务器 API
 * ======================================================================== */

/**
 * @brief 创建 pgwire 服务器
 */
PgwireServer *pgwire_server_create(const PgwireServerConfig *config);

/**
 * @brief 销毁 pgwire 服务器
 */
void pgwire_server_destroy(PgwireServer *server);

/**
 * @brief 设置认证回调
 */
void pgwire_server_set_auth_callback(PgwireServer *server,
                                    PgwireAuthCallback callback);

/**
 * @brief 设置执行回调
 */
void pgwire_server_set_exec_callback(PgwireServer *server,
                                    PgwireExecCallback callback);

/**
 * @brief 设置参数回调
 */
void pgwire_server_set_param_callback(PgwireServer *server,
                                      PgwireParamCallback callback);

/**
 * @brief 启动服务器
 */
int pgwire_server_start(PgwireServer *server);

/**
 * @brief 停止服务器
 */
void pgwire_server_stop(PgwireServer *server);

/**
 * @brief 接受新连接
 */
PgwireConn *pgwire_server_accept(PgwireServer *server);

/**
 * @brief 处理服务器事件
 */
int pgwire_server_poll(PgwireServer *server, int timeout_ms);

/**
 * @brief 获取服务器统计
 */
void pgwire_server_get_stats(PgwireServer *server, int *nconnections,
                            int *nqueries);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 设置非阻塞模式
 */
int pgwire_set_nonblocking(int fd, bool nonblocking);

/**
 * @brief 读取完整的 N 字节
 */
int pgwire_read_full(int fd, void *buf, int len);

/**
 * @brief 写入完整的 N 字节
 */
int pgwire_write_full(int fd, const void *buf, int len);

/**
 * @brief 创建错误响应
 */
PgwireError *pgwire_error_create(const char *sqlstate, const char *message);

/**
 * @brief 销毁错误响应
 */
void pgwire_error_destroy(PgwireError *error);

/**
 * @brief 设置连接错误
 */
void pgwire_conn_set_error(PgwireConn *conn, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* DB_PGWIRE_H */
