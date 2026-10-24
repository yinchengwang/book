/*
 * replication.h - 主从复制接口
 */

#ifndef DB_REPLICATION_H
#define DB_REPLICATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 复制角色
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    REPL_ROLE_PRIMARY,     /* 主节点（Leader） */
    REPL_ROLE_REPLICA      /* 从节点（Follower） */
} repl_role_t;

/* ─────────────────────────────────────────────────────────────────
 * 复制状态
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    REPL_STATE_DISCONNECTED,   /* 未连接 */
    REPL_STATE_CONNECTING,     /* 连接中 */
    REPL_STATE_HANDSHAKE,      /* 握手阶段 */
    REPL_STATE_STREAMING,      /* 流复制中 */
    REPL_STATE_CATCHUP,        /* 追赶中 */
    REPL_STATE_NORMAL,         /* 正常复制 */
    REPL_STATE_SHUTDOWN        /* 关闭中 */
} repl_state_t;

/* ─────────────────────────────────────────────────────────────────
 * 复制消息类型
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    REPL_MSG_HANDSHAKE,        /* 握手请求 */
    REPL_MSG_HANDSHAKE_ACK,    /* 握手确认 */
    REPL_MSG_WAL_DATA,         /* WAL 数据 */
    REPL_MSG_WAL_POSITION,     /* WAL 位置同步 */
    REPL_MSG_HEARTBEAT,        /* 心跳 */
    REPL_MSG_HEARTBEAT_ACK,    /* 心跳响应 */
    REPL_MSG_SYNC_REQUEST,     /* 同步请求 */
    REPL_MSG_SYNC_RESPONSE,    /* 同步响应 */
    REPL_MSG_RECONNECT         /* 重连请求 */
} repl_msg_type_t;

/* ─────────────────────────────────────────────────────────────────
 * 复制消息结构
 * ───────────────────────────────────────────────────────────────── */

typedef struct repl_message {
    repl_msg_type_t type;      /* 消息类型 */
    uint32_t        length;    /* 消息长度 */
    uint64_t        wal_lsn;   /* WAL LSN */
    uint64_t        timestamp; /* 时间戳 */
    /* 消息数据跟随此结构 */
} repl_message_t;

/* ─────────────────────────────────────────────────────────────────
 * 复制配置
 * ───────────────────────────────────────────────────────────────── */

typedef struct repl_config {
    repl_role_t role;              /* 复制角色 */
    char       *primary_host;      /* 主节点地址（从节点用） */
    int         primary_port;      /* 主节点端口 */
    char       *replica_host;      /* 本机监听地址 */
    int         replica_port;      /* 本机监听端口 */
    int         max_replicas;      /* 最大从节点数 */
    int         heartbeat_interval;/* 心跳间隔（毫秒） */
    int         connect_timeout;   /* 连接超时（秒） */
    int         max_retry;         /* 最大重试次数 */
    bool        sync_replication;  /* 是否同步复制 */
} repl_config_t;

/* ─────────────────────────────────────────────────────────────────
 * 复制统计信息
 * ───────────────────────────────────────────────────────────────── */

typedef struct repl_stats {
    uint64_t bytes_sent;           /* 发送字节数 */
    uint64_t bytes_received;       /* 接收字节数 */
    uint64_t msgs_sent;            /* 发送消息数 */
    uint64_t msgs_received;        /* 接收消息数 */
    uint64_t last_wal_lsn;         /* 最后发送的 WAL LSN */
    uint64_t last_applied_lsn;     /* 最后应用的 LSN */
    uint64_t replication_lag;      /* 复制延迟（字节） */
    int      active_connections;   /* 活跃连接数 */
} repl_stats_t;

/* ─────────────────────────────────────────────────────────────────
 * 复制连接
 * ───────────────────────────────────────────────────────────────── */

typedef struct repl_connection repl_connection_t;

/* ─────────────────────────────────────────────────────────────────
 * 复制管理器
 * ───────────────────────────────────────────────────────────────── */

typedef struct repl_manager repl_manager_t;

/**
 * @brief 创建复制管理器
 * @param config 复制配置
 * @return 复制管理器句柄
 */
repl_manager_t *repl_manager_create(const repl_config_t *config);

/**
 * @brief 销毁复制管理器
 * @param manager 复制管理器
 */
void repl_manager_destroy(repl_manager_t *manager);

/**
 * @brief 启动复制服务
 * @param manager 复制管理器
 * @return 0 成功
 */
int repl_start(repl_manager_t *manager);

/**
 * @brief 停止复制服务
 * @param manager 复制管理器
 */
void repl_stop(repl_manager_t *manager);

/**
 * @brief 获取当前角色
 * @param manager 复制管理器
 * @return 角色
 */
repl_role_t repl_get_role(const repl_manager_t *manager);

/**
 * @brief 获取复制状态
 * @param manager 复制管理器
 * @return 复制状态
 */
repl_state_t repl_get_state(const repl_manager_t *manager);

/**
 * @brief 获取复制统计
 * @param manager 复制管理器
 * @return 统计信息
 */
const repl_stats_t *repl_get_stats(const repl_manager_t *manager);

/**
 * @brief 发送 WAL 数据（主节点调用）
 * @param manager 复制管理器
 * @param wal_data WAL 数据
 * @param len 数据长度
 * @param lsn WAL LSN
 * @return 发送的字节数，-1 失败
 */
ssize_t repl_send_wal(repl_manager_t *manager, const void *wal_data,
                      size_t len, uint64_t lsn);

/**
 * @brief 等待同步（同步复制模式）
 * @param manager 复制管理器
 * @param min_lsn 最小 LSN
 * @param timeout_ms 超时（毫秒）
 * @return 0 成功，-1 超时
 */
int repl_wait_sync(repl_manager_t *manager, uint64_t min_lsn, int timeout_ms);

/**
 * @brief 获取复制延迟
 * @param manager 复制管理器
 * @return 延迟（毫秒）
 */
int64_t repl_get_lag_ms(const repl_manager_t *manager);

/**
 * @brief 检查是否需要故障切换
 * @param manager 复制管理器
 * @return true 需要切换
 */
bool repl_need_failover(const repl_manager_t *manager);

/**
 * @brief 执行故障切换
 * @param manager 复制管理器
 * @return 0 成功
 */
int repl_do_failover(repl_manager_t *manager);

/* ─────────────────────────────────────────────────────────────────
 * 复制连接操作（高级 API）
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 创建到主节点的连接（从节点调用）
 * @param manager 复制管理器
 * @param host 主节点地址
 * @param port 主节点端口
 * @return 连接句柄
 */
repl_connection_t *repl_connect_primary(repl_manager_t *manager,
                                         const char *host, int port);

/**
 * @brief 关闭连接
 * @param conn 连接句柄
 */
void repl_connection_close(repl_connection_t *conn);

/**
 * @brief 接收消息
 * @param conn 连接句柄
 * @param timeout_ms 超时
 * @return 消息结构（需手动释放）
 */
repl_message_t *repl_recv(repl_connection_t *conn, int timeout_ms);

/**
 * @brief 发送消息
 * @param conn 连接句柄
 * @param msg 消息
 * @return 发送的字节数
 */
ssize_t repl_send(repl_connection_t *conn, const repl_message_t *msg);

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 创建默认配置
 * @param role 角色
 * @return 配置（需手动释放）
 */
repl_config_t *repl_config_create(repl_role_t role);

/**
 * @brief 销毁配置
 * @param config 配置
 */
void repl_config_destroy(repl_config_t *config);

/**
 * @brief 获取状态描述
 * @param state 状态
 * @return 描述字符串
 */
const char *repl_state_name(repl_state_t state);

/**
 * @brief 获取角色描述
 * @param role 角色
 * @return 描述字符串
 */
const char *repl_role_name(repl_role_t role);

#ifdef __cplusplus
}
#endif

#endif /* DB_REPLICATION_H */