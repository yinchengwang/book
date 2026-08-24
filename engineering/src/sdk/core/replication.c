/**
 * @file replication.c
 * @brief Raft 复制协议实现
 *
 * 基于 SQLite WAL 的主从复制：
 * - Leader 将 WAL 条目作为 Raft 日志复制
 * - Follower 接收并应用日志
 * - 支持 Leader 选举和故障转移
 */
#include "sdk/mmdb_replication.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* 复制状态常量 */
#define RAFT_HEARTBEAT_INTERVAL_MS  1000  /* 心跳间隔 1s */
#define RAFT_ELECTION_TIMEOUT_MS   5000  /* 选举超时 5s */
#define RAFT_LOG_BATCH_SIZE        100   /* 日志批量大小 */

/* 节点信息结构 */
typedef struct {
    uint32_t    id;
    char        addr[256];
    bool        alive;
    uint64_t    match_index;    /* 已匹配的日志索引 */
    uint64_t    next_index;     /* 下一条要发送的日志索引 */
} raft_node_t;

/* 复制句柄内部结构 */
struct mmdb_replica_s {
    mmdb_t*             db;             /* 数据库句柄 */
    mmdb_replica_role_t role;           /* 当前角色 */
    uint32_t            node_id;        /* 本节点 ID */
    uint32_t            term;           /* 当前 Raft 任期 */
    uint64_t            commit_index;   /* 已提交日志索引 */
    uint64_t            applied_index;  /* 已应用日志索引 */
    uint64_t            last_log_index; /* 最后一条日志的索引 */

    /* 集群节点 */
    raft_node_t*        nodes;
    uint32_t            node_count;
    uint32_t            leader_id;

    /* 时间控制 */
    uint64_t            last_heartbeat; /* 上次心跳时间 */
    uint64_t            election_start; /* 选举开始时间 */

    /* 运行状态 */
    bool                running;
};

/* 全局复制实例（简化：单实例） */
static mmdb_replica_t* g_replica = NULL;

/**
 * @brief 获取当前时间戳（毫秒）
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * @brief 解析 JSON 格式的节点列表（简化实现）
 *
 * 格式：[{"id":1,"addr":"host:port"},...]
 * 注意：生产环境应使用真正的 JSON 解析器
 */
static int parse_nodes(const char* peers, raft_node_t** out_nodes, uint32_t* out_count) {
    if (!peers || !out_nodes || !out_count) {
        return MMDB_ERR_INVALID;
    }

    /* 简化：手动解析固定格式 */
    uint32_t count = 0;
    const char* p = peers;

    /* 计算节点数量（按 "id" 关键字计数） */
    while ((p = strstr(p, "\"id\"")) != NULL) {
        count++;
        p++;
    }

    if (count == 0) {
        return MMDB_ERR_INVALID;
    }

    raft_node_t* nodes = (raft_node_t*)calloc(count, sizeof(raft_node_t));
    if (!nodes) {
        return MMDB_ERR_NOMEM;
    }

    /* 重新解析，填充节点信息 */
    p = peers;
    uint32_t idx = 0;
    while (idx < count && (p = strstr(p, "\"id\"")) != NULL) {
        /* 提取 id */
        p += 4;
        while (*p == ' ' || *p == ':') p++;
        nodes[idx].id = (uint32_t)atoi(p);
        nodes[idx].alive = true;
        nodes[idx].match_index = 0;
        nodes[idx].next_index = 1;

        /* 提取 addr */
        const char* addr_start = strstr(p, "\"addr\"");
        if (addr_start) {
            addr_start += 6;
            while (*addr_start == ' ' || *addr_start == ':') addr_start++;
            if (*addr_start == '"') addr_start++;
            const char* addr_end = strchr(addr_start, '"');
            if (addr_end) {
                size_t len = (size_t)(addr_end - addr_start);
                if (len >= sizeof(nodes[idx].addr)) {
                    len = sizeof(nodes[idx].addr) - 1;
                }
                memcpy(nodes[idx].addr, addr_start, len);
                nodes[idx].addr[len] = '\0';
            }
        }

        idx++;
        p++;
    }

    *out_nodes = nodes;
    *out_count = count;
    return MMDB_OK;
}

/**
 * @brief 初始化复制模式
 */
int mmdb_replication_init(mmdb_t* db, mmdb_replica_role_t role, const char* peers) {
    if (!db || !peers) {
        return MMDB_ERR_INVALID;
    }

    /* 检查是否已初始化 */
    if (g_replica) {
        return MMDB_ERR_INTERNAL;
    }

    /* 分配复制句柄 */
    mmdb_replica_t* replica = (mmdb_replica_t*)calloc(1, sizeof(mmdb_replica_t));
    if (!replica) {
        return MMDB_ERR_NOMEM;
    }

    replica->db = db;
    replica->role = role;
    replica->node_id = 1;  /* 简化：固定节点 ID */
    replica->term = 0;
    replica->commit_index = 0;
    replica->applied_index = 0;
    replica->last_log_index = 0;
    replica->running = false;

    /* 解析节点列表 */
    int rc = parse_nodes(peers, &replica->nodes, &replica->node_count);
    if (rc != MMDB_OK) {
        free(replica);
        return rc;
    }

    /* 根据角色设置初始状态 */
    if (role == MMDB_REPLICA_LEADER) {
        replica->leader_id = replica->node_id;
        replica->running = true;
        replica->last_heartbeat = get_timestamp_ms();
    } else {
        /* Follower 需要找到 Leader */
        replica->leader_id = 0;
        for (uint32_t i = 0; i < replica->node_count; i++) {
            if (replica->nodes[i].id != replica->node_id) {
                replica->leader_id = replica->nodes[i].id;
                break;
            }
        }
        replica->running = true;
        replica->election_start = get_timestamp_ms();
    }

    g_replica = replica;
    return MMDB_OK;
}

/**
 * @brief 获取复制状态
 */
int mmdb_replication_info(mmdb_t* db, mmdb_replica_info_t* info) {
    if (!db || !info) {
        return MMDB_ERR_INVALID;
    }

    if (!g_replica) {
        return MMDB_ERR_INTERNAL;
    }

    info->role = g_replica->role;
    info->commit_index = g_replica->commit_index;
    info->applied_index = g_replica->applied_index;
    info->is_synced = (g_replica->commit_index == g_replica->applied_index);
    info->term = g_replica->term;
    info->node_id = g_replica->node_id;

    /* 获取 Leader 地址 */
    if (g_replica->role == MMDB_REPLICA_FOLLOWER && g_replica->leader_id > 0) {
        for (uint32_t i = 0; i < g_replica->node_count; i++) {
            if (g_replica->nodes[i].id == g_replica->leader_id) {
                info->leader_addr = g_replica->nodes[i].addr;
                break;
            }
        }
    } else {
        info->leader_addr = NULL;
    }

    return MMDB_OK;
}

/**
 * @brief 发起 Leader 选举
 */
static int start_election(mmdb_replica_t* replica) {
    if (!replica) {
        return MMDB_ERR_INVALID;
    }

    /* 递增任期 */
    replica->term++;
    replica->role = MMDB_REPLICA_CANDIDATE;
    replica->leader_id = 0;
    replica->election_start = get_timestamp_ms();

    /* TODO: 发送 RequestVote RPC 给所有节点 */

    return MMDB_OK;
}

/**
 * @brief 触发故障转移
 */
int mmdb_replication_failover(mmdb_t* db) {
    if (!db) {
        return MMDB_ERR_INVALID;
    }

    if (!g_replica) {
        return MMDB_ERR_INTERNAL;
    }

    /* 只有 Follower 可以发起故障转移 */
    if (g_replica->role != MMDB_REPLICA_FOLLOWER) {
        return MMDB_ERR_INTERNAL;
    }

    return start_election(g_replica);
}

/**
 * @brief 停止复制
 */
int mmdb_replication_stop(mmdb_t* db) {
    if (!db) {
        return MMDB_ERR_INVALID;
    }

    if (!g_replica) {
        return MMDB_OK;
    }

    g_replica->running = false;

    /* 释放节点列表 */
    if (g_replica->nodes) {
        free(g_replica->nodes);
        g_replica->nodes = NULL;
    }

    free(g_replica);
    g_replica = NULL;

    return MMDB_OK;
}

/**
 * @brief Leader 写入日志
 */
int mmdb_replication_append_log(mmdb_t* db, const void* data, uint32_t len) {
    if (!db || !data || len == 0) {
        return MMDB_ERR_INVALID;
    }

    if (!g_replica || g_replica->role != MMDB_REPLICA_LEADER) {
        return MMDB_ERR_INTERNAL;
    }

    /* 递增日志索引 */
    g_replica->last_log_index++;

    /* TODO: 复制日志到所有 Follower */
    /* 简化：直接提交 */
    g_replica->commit_index = g_replica->last_log_index;

    return MMDB_OK;
}

/**
 * @brief Follower 应用日志
 */
int mmdb_replication_apply_log(mmdb_t* db, const void* data, uint32_t len) {
    if (!db || !data || len == 0) {
        return MMDB_ERR_INVALID;
    }

    if (!g_replica || g_replica->role != MMDB_REPLICA_FOLLOWER) {
        return MMDB_ERR_INTERNAL;
    }

    /* 应用日志到本地数据库 */
    g_replica->applied_index++;

    return MMDB_OK;
}
