/*
 * replication.c - 主从复制实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <db/replication/replication.h>

/* ─────────────────────────────────────────────────────────────────
 * 复制连接结构
 * ───────────────────────────────────────────────────────────────── */

struct repl_connection {
    int fd;                     /* socket 描述符 */
    repl_role_t role;           /* 角色 */
    repl_state_t state;         /* 连接状态 */
    uint64_t last_wal_lsn;      /* 最后接收的 WAL LSN */
    uint64_t last_sent_lsn;     /* 最后发送的 LSN */
    time_t last_heartbeat;      /* 最后心跳时间 */
};

/* ─────────────────────────────────────────────────────────────────
 * 复制管理器结构
 * ───────────────────────────────────────────────────────────────── */

struct repl_manager {
    repl_config_t *config;       /* 复制配置 */
    repl_role_t role;           /* 当前角色 */
    repl_state_t state;         /* 当前状态 */
    int listen_fd;              /* 监听 socket */
    repl_stats_t stats;         /* 统计信息 */
    repl_connection_t **connections;  /* 活跃连接 */
    int connection_count;        /* 连接数 */
    time_t start_time;          /* 启动时间 */
};

/* ─────────────────────────────────────────────────────────────────
 * 连接管理实现
 * ───────────────────────────────────────────────────────────────── */

repl_connection_t *repl_connect_primary(repl_manager_t *manager,
                                        const char *host, int port)
{
    (void)manager;
    (void)host;
    (void)port;

    if (manager == NULL || host == NULL) return NULL;

    repl_connection_t *conn = (repl_connection_t *)calloc(1, sizeof(repl_connection_t));
    if (conn == NULL) return NULL;

    conn->fd = -1;
    conn->role = REPL_ROLE_REPLICA;
    conn->state = REPL_STATE_DISCONNECTED;
    conn->last_wal_lsn = 0;
    conn->last_sent_lsn = 0;
    conn->last_heartbeat = time(NULL);

    /* TODO: 实现实际的 socket 连接
     * conn->fd = socket(AF_INET, SOCK_STREAM, 0);
     * connect(conn->fd, ...)
     */

    return conn;
}

void repl_connection_close(repl_connection_t *conn)
{
    if (conn == NULL) return;

    if (conn->fd >= 0) {
        /* close(conn->fd); */
        conn->fd = -1;
    }

    free(conn);
}

repl_message_t *repl_recv(repl_connection_t *conn, int timeout_ms)
{
    if (conn == NULL || conn->fd < 0) return NULL;

    /* TODO: 实现实际的接收逻辑 */
    (void)timeout_ms;
    return NULL;
}

ssize_t repl_send(repl_connection_t *conn, const repl_message_t *msg)
{
    if (conn == NULL || conn->fd < 0 || msg == NULL) return -1;

    /* TODO: 实现实际的发送逻辑 */
    return msg->length;
}

/* ─────────────────────────────────────────────────────────────────
 * 复制管理器实现
 * ───────────────────────────────────────────────────────────────── */

repl_manager_t *repl_manager_create(const repl_config_t *config)
{
    if (config == NULL) return NULL;

    repl_manager_t *manager = (repl_manager_t *)calloc(1, sizeof(repl_manager_t));
    if (manager == NULL) return NULL;

    manager->config = (repl_config_t *)malloc(sizeof(repl_config_t));
    if (manager->config == NULL) {
        free(manager);
        return NULL;
    }

    memcpy(manager->config, config, sizeof(repl_config_t));
    manager->role = config->role;
    manager->state = REPL_STATE_DISCONNECTED;
    manager->listen_fd = -1;
    manager->connection_count = 0;
    manager->connections = NULL;
    manager->start_time = time(NULL);

    /* 初始化统计信息 */
    memset(&manager->stats, 0, sizeof(repl_stats_t));

    return manager;
}

void repl_manager_destroy(repl_manager_t *manager)
{
    if (manager == NULL) return;

    repl_stop(manager);

    if (manager->config) free(manager->config);
    if (manager->connections) free(manager->connections);
    free(manager);
}

int repl_start(repl_manager_t *manager)
{
    if (manager == NULL) return -1;

    if (manager->role == REPL_ROLE_PRIMARY) {
        /* 主节点：启动监听 */
        manager->state = REPL_STATE_NORMAL;
    } else {
        /* 从节点：连接到主节点 */
        manager->state = REPL_STATE_CONNECTING;
    }

    manager->start_time = time(NULL);
    return 0;
}

void repl_stop(repl_manager_t *manager)
{
    if (manager == NULL) return;

    manager->state = REPL_STATE_SHUTDOWN;

    /* 关闭所有连接 */
    for (int i = 0; i < manager->connection_count; i++) {
        if (manager->connections[i]) {
            repl_connection_close(manager->connections[i]);
        }
    }
    if (manager->connections) {
        free(manager->connections);
        manager->connections = NULL;
    }
    manager->connection_count = 0;

    /* 关闭监听 socket */
    if (manager->listen_fd >= 0) {
        /* close(manager->listen_fd); */
        manager->listen_fd = -1;
    }
}

repl_role_t repl_get_role(const repl_manager_t *manager)
{
    return manager ? manager->role : REPL_ROLE_PRIMARY;
}

repl_state_t repl_get_state(const repl_manager_t *manager)
{
    return manager ? manager->state : REPL_STATE_DISCONNECTED;
}

const repl_stats_t *repl_get_stats(const repl_manager_t *manager)
{
    return manager ? &manager->stats : NULL;
}

ssize_t repl_send_wal(repl_manager_t *manager, const void *wal_data,
                       size_t len, uint64_t lsn)
{
    if (manager == NULL || wal_data == NULL) return -1;

    /* TODO: 实现实际的 WAL 发送逻辑
     * 遍历所有从节点连接，发送 WAL 数据
     */

    manager->stats.bytes_sent += len;
    manager->stats.msgs_sent++;
    manager->stats.last_wal_lsn = lsn;

    return len;
}

int repl_wait_sync(repl_manager_t *manager, uint64_t min_lsn, int timeout_ms)
{
    if (manager == NULL) return -1;

    /* TODO: 实现同步等待逻辑
     * 等待所有从节点确认已接收 min_lsn 之前的 WAL
     */

    (void)min_lsn;
    (void)timeout_ms;

    return 0;
}

int64_t repl_get_lag_ms(const repl_manager_t *manager)
{
    if (manager == NULL) return -1;

    /* 计算复制延迟 */
    if (manager->stats.last_wal_lsn > manager->stats.last_applied_lsn) {
        return (manager->stats.last_wal_lsn - manager->stats.last_applied_lsn) / 1024;  /* 简化计算 */
    }
    return 0;
}

bool repl_need_failover(const repl_manager_t *manager)
{
    if (manager == NULL) return false;

    /* 检查是否需要故障切换
     * 条件：主节点不可达且有从节点可以晋升
     */
    if (manager->role != REPL_ROLE_REPLICA) return false;

    /* 检查心跳超时 */
    time_t now = time(NULL);
    for (int i = 0; i < manager->connection_count; i++) {
        if (manager->connections[i] &&
            now - manager->connections[i]->last_heartbeat > 30) {  /* 30秒超时 */
            return true;
        }
    }

    return false;
}

int repl_do_failover(repl_manager_t *manager)
{
    if (manager == NULL) return -1;

    /* 执行故障切换
     * 1. 断开与主节点的连接
     * 2. 提升为新的主节点
     * 3. 开始接受写入
     */
    manager->role = REPL_ROLE_PRIMARY;
    manager->state = REPL_STATE_NORMAL;

    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数实现
 * ───────────────────────────────────────────────────────────────── */

repl_config_t *repl_config_create(repl_role_t role)
{
    repl_config_t *config = (repl_config_t *)calloc(1, sizeof(repl_config_t));
    if (config == NULL) return NULL;

    config->role = role;
    config->primary_port = 5432;
    config->replica_port = 5433;
    config->max_replicas = 8;
    config->heartbeat_interval = 1000;    /* 1秒 */
    config->connect_timeout = 10;          /* 10秒 */
    config->max_retry = 3;
    config->sync_replication = false;

    return config;
}

void repl_config_destroy(repl_config_t *config)
{
    free(config);
}

const char *repl_state_name(repl_state_t state)
{
    switch (state) {
        case REPL_STATE_DISCONNECTED: return "disconnected";
        case REPL_STATE_CONNECTING:  return "connecting";
        case REPL_STATE_HANDSHAKE:   return "handshake";
        case REPL_STATE_STREAMING:   return "streaming";
        case REPL_STATE_CATCHUP:     return "catchup";
        case REPL_STATE_NORMAL:     return "normal";
        case REPL_STATE_SHUTDOWN:    return "shutdown";
        default: return "unknown";
    }
}

const char *repl_role_name(repl_role_t role)
{
    switch (role) {
        case REPL_ROLE_PRIMARY:  return "primary";
        case REPL_ROLE_REPLICA:  return "replica";
        default: return "unknown";
    }
}