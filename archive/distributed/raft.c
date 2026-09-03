/**
 * @file raft.c
 * @brief Raft 共识算法实现
 */

#include "db/core/raft.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

/* 获取当前时间戳（毫秒） */
static uint64_t get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
}

/* ========================================================================
 * Raft 服务器
 * ======================================================================== */

RaftServer *raft_server_create(uint64_t node_id, const char *address) {
    RaftServer *server = (RaftServer *)calloc(1, sizeof(RaftServer));
    if (!server) return NULL;

    server->node_id = node_id;
    if (address) {
        server->cluster_address = strdup(address);
    }

    server->role = RAFT_ROLE_FOLLOWER;
    server->current_term = 0;
    server->voted_for = UINT64_MAX;

    /* 初始化日志 */
    server->log_capacity = 1024;
    server->log = (RaftEntry *)calloc(server->log_capacity, sizeof(RaftEntry));
    server->log_size = 0;

    /* 初始化 Volatile 状态 */
    server->commit_index = 0;
    server->last_applied = 0;
    server->last_snapshot_index = 0;
    server->last_snapshot_term = 0;

    /* 初始化时间配置 */
    server->election_timeout = 1000;      /* 1 秒 */
    server->heartbeat_interval = 150;     /* 150 毫秒 */
    server->last_heartbeat = get_current_time_ms();

    /* 初始化锁 */
    pthread_mutex_init(&server->lock, NULL);

    return server;
}

void raft_server_destroy(RaftServer *server) {
    if (!server) return;

    pthread_mutex_destroy(&server->lock);
    free(server->cluster_address);
    free(server->log);
    free(server->next_index);
    free(server->match_index);
    free(server);
}

int raft_server_init(RaftServer *server,
                     RaftClusterConfig *config,
                     RaftLogStore *log_store) {
    (void)log_store;
    if (!server || !config) return -1;

    server->config = config;

    /* 初始化 Leader 状态数组 */
    if (config->num_nodes > 0) {
        server->next_index = (uint64_t *)calloc(config->num_nodes, sizeof(uint64_t));
        server->match_index = (uint64_t *)calloc(config->num_nodes, sizeof(uint64_t));
    }

    return 0;
}

int raft_server_start(RaftServer *server) {
    if (!server) return -1;

    server->last_heartbeat = get_current_time_ms();
    return 0;
}

int raft_server_stop(RaftServer *server) {
    (void)server;
    return 0;
}

uint64_t raft_get_leader(RaftServer *server) {
    if (!server) return UINT64_MAX;
    return server->role == RAFT_ROLE_LEADER ? server->node_id : UINT64_MAX;
}

bool raft_is_leader(RaftServer *server) {
    return server && server->role == RAFT_ROLE_LEADER;
}

const RaftClusterConfig *raft_get_config(RaftServer *server) {
    return server ? server->config : NULL;
}

/* ========================================================================
 * 消息处理
 * ======================================================================== */

int raft_send_message(RaftServer *server, const RaftMessage *msg) {
    (void)server; (void)msg;
    /* 简化实现：网络层省略 */
    return 0;
}

int raft_handle_message(RaftServer *server, const RaftMessage *msg) {
    if (!server || !msg) return -1;

    pthread_mutex_lock(&server->lock);

    /* 任期检查 */
    if (msg->term > server->current_term) {
        server->current_term = msg->term;
        if (server->role == RAFT_ROLE_LEADER) {
            server->role = RAFT_ROLE_FOLLOWER;
        }
        server->voted_for = UINT64_MAX;
    }

    switch (msg->type) {
    case RAFT_MSG_REQUEST_VOTE:
        /* 处理投票请求 */
        {
            bool vote_granted;
            raft_handle_request_vote(server, msg->from_node, msg->term,
                                     msg->data.request_vote.last_log_index,
                                     msg->data.request_vote.last_log_term,
                                     &vote_granted);
        }
        break;

    case RAFT_MSG_APPEND_ENTRIES:
        /* 处理追加日志 */
        {
            bool success;
            raft_handle_append_entries(server, msg->from_node, msg->term,
                                       msg->data.append_entries.prev_log_index,
                                       msg->data.append_entries.prev_log_term,
                                       msg->data.append_entries.entries,
                                       msg->data.append_entries.entries_length,
                                       msg->data.append_entries.leader_commit,
                                       &success);
        }
        break;

    case RAFT_MSG_HEARTBEAT:
        /* 更新心跳时间 */
        server->last_heartbeat = get_current_time_ms();
        if (server->role == RAFT_ROLE_CANDIDATE) {
            server->role = RAFT_ROLE_FOLLOWER;
        }
        break;

    default:
        break;
    }

    pthread_mutex_unlock(&server->lock);
    return 0;
}

/* ========================================================================
 * 选举
 * ======================================================================== */

int raft_start_election(RaftServer *server) {
    if (!server) return -1;

    /* 变为候选者 */
    server->role = RAFT_ROLE_CANDIDATE;
    server->current_term++;
    server->voted_for = server->node_id;
    server->voted_count = 1;  /* 自己投自己 */
    server->last_heartbeat = get_current_time_ms();

    /* 获取最后日志信息 */
    uint64_t last_log_index = server->log_size > 0 ? server->log[server->log_size - 1].index : 0;
    uint64_t last_log_term = server->log_size > 0 ? server->log[server->log_size - 1].term : 0;

    /* 向所有节点发送投票请求 */
    RaftMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = RAFT_MSG_REQUEST_VOTE;
    msg.from_node = server->node_id;
    msg.term = server->current_term;
    msg.data.request_vote.last_log_index = last_log_index;
    msg.data.request_vote.last_log_term = last_log_term;

    raft_send_message(server, &msg);

    return 0;
}

bool raft_check_election_timeout(RaftServer *server) {
    if (!server) return false;

    uint64_t elapsed = get_current_time_ms() - server->last_heartbeat;
    return elapsed >= server->election_timeout;
}

void raft_reset_election_timer(RaftServer *server) {
    if (server) {
        server->last_heartbeat = get_current_time_ms();
    }
}

int raft_handle_request_vote(RaftServer *server,
                              uint64_t candidate_id,
                              uint64_t candidate_term,
                              uint64_t last_log_index,
                              uint64_t last_log_term,
                              bool *out_vote_granted) {
    if (!server || !out_vote_granted) return -1;

    *out_vote_granted = false;

    /* 任期检查 */
    if (candidate_term < server->current_term) {
        return 0;
    }

    /* 检查是否已投票 */
    if (server->voted_for != UINT64_MAX && server->voted_for != candidate_id) {
        return 0;
    }

    /* 检查日志是否够新 */
    uint64_t our_last_log_index = server->log_size > 0 ? server->log[server->log_size - 1].index : 0;
    uint64_t our_last_log_term = server->log_size > 0 ? server->log[server->log_size - 1].term : 0;

    if (last_log_term < our_last_log_term) {
        return 0;
    }

    if (last_log_term == our_last_log_term && last_log_index < our_last_log_index) {
        return 0;
    }

    /* 授予投票 */
    *out_vote_granted = true;
    server->voted_for = candidate_id;
    server->current_term = candidate_term;
    raft_reset_election_timer(server);

    return 0;
}

/* ========================================================================
 * 日志复制
 * ======================================================================== */

int raft_handle_append_entries(RaftServer *server,
                               uint64_t leader_id,
                               uint64_t leader_term,
                               uint64_t prev_log_index,
                               uint64_t prev_log_term,
                               const RaftEntry *entries,
                               size_t num_entries,
                               uint64_t leader_commit,
                               bool *out_success) {
    (void)leader_id;

    if (!server || !out_success) return -1;

    *out_success = false;

    /* 任期检查 */
    if (leader_term < server->current_term) {
        return 0;
    }

    /* 重置心跳 */
    raft_reset_election_timer(server);

    /* 变成跟随者 */
    if (server->role != RAFT_ROLE_FOLLOWER) {
        server->role = RAFT_ROLE_FOLLOWER;
    }
    server->current_term = leader_term;

    /* 检查 prev_log_index */
    if (prev_log_index > 0) {
        if (prev_log_index > server->log_size) {
            return 0;  /* 日志不包含 prev_log_index */
        }

        if (prev_log_index > 0 && prev_log_index <= server->log_size) {
            RaftEntry *entry = &server->log[prev_log_index - 1];
            if (entry->index == prev_log_index && entry->term != prev_log_term) {
                /* 冲突，截断日志 */
                server->log_size = prev_log_index - 1;
            }
        }
    }

    /* 追加新条目 */
    for (size_t i = 0; i < num_entries; i++) {
        const RaftEntry *e = &entries[i];
        if (e->index <= server->log_size) {
            /* 覆盖现有条目 */
            server->log[e->index - 1] = *e;
        } else {
            /* 追加新条目 */
            if (server->log_size >= server->log_capacity) {
                server->log_capacity *= 2;
                server->log = (RaftEntry *)realloc(server->log, server->log_capacity * sizeof(RaftEntry));
            }
            server->log[server->log_size++] = *e;
        }
    }

    /* 更新提交索引 */
    if (leader_commit > server->commit_index) {
        server->commit_index = (leader_commit < server->log_size) ? leader_commit : server->log_size;
    }

    *out_success = true;
    return 0;
}

int raft_broadcast_append_entries(RaftServer *server) {
    if (!server || server->role != RAFT_ROLE_LEADER) return -1;

    RaftMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = RAFT_MSG_APPEND_ENTRIES;
    msg.from_node = server->node_id;
    msg.term = server->current_term;

    /* 广播到所有跟随者 */
    raft_send_message(server, &msg);

    return 0;
}

int raft_send_heartbeat(RaftServer *server, uint64_t to_node) {
    (void)to_node;

    if (!server || server->role != RAFT_ROLE_LEADER) return -1;

    RaftMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = RAFT_MSG_HEARTBEAT;
    msg.from_node = server->node_id;
    msg.term = server->current_term;

    return raft_send_message(server, &msg);
}

int raft_broadcast_heartbeat(RaftServer *server) {
    if (!server || server->role != RAFT_ROLE_LEADER) return -1;

    raft_broadcast_append_entries(server);
    return 0;
}

bool raft_needs_snapshot(RaftServer *server, size_t max_log_size) {
    if (!server) return false;
    return server->log_size > 0 && server->log_size * sizeof(RaftEntry) > max_log_size;
}

/* ========================================================================
 * 日志提交
 * ======================================================================== */

int raft_submit(RaftServer *server, const void *data, size_t size, uint64_t *out_index) {
    if (!server || server->role != RAFT_ROLE_LEADER) return -1;

    pthread_mutex_lock(&server->lock);

    /* 创建日志条目 */
    if (server->log_size >= server->log_capacity) {
        server->log_capacity *= 2;
        server->log = (RaftEntry *)realloc(server->log, server->log_capacity * sizeof(RaftEntry));
    }

    RaftEntry *entry = &server->log[server->log_size];
    entry->term = server->current_term;
    entry->index = server->log_size + 1;
    entry->type = RAFT_ENTRY_DATA;
    entry->data_size = size;
    entry->data = malloc(size);
    if (entry->data) {
        memcpy(entry->data, data, size);
    }
    entry->timestamp = get_current_time_ms();

    uint64_t index = entry->index;
    server->log_size++;

    if (out_index) *out_index = index;

    pthread_mutex_unlock(&server->lock);

    /* 广播给跟随者 */
    raft_broadcast_append_entries(server);

    return 0;
}

int raft_get_committed(RaftServer *server, uint64_t index, void *data, size_t *size) {
    if (!server || index > server->commit_index) return -1;

    if (index > 0 && index <= server->log_size) {
        RaftEntry *entry = &server->log[index - 1];
        if (data && size && entry->data) {
            size_t copy_size = entry->data_size < *size ? entry->data_size : *size;
            memcpy(data, entry->data, copy_size);
            *size = copy_size;
        }
        return 0;
    }

    return -1;
}

/* ========================================================================
 * 成员变更
 * ======================================================================== */

int raft_add_node(RaftServer *server, const RaftNode *node) {
    if (!server || !node) return -1;

    /* 只能通过 Leader 添加节点 */
    if (server->role != RAFT_ROLE_LEADER) return -1;

    /* 简化实现 */
    return 0;
}

int raft_remove_node(RaftServer *server, uint64_t node_id) {
    (void)server; (void)node_id;
    /* 简化实现 */
    return 0;
}

int raft_start_joint_consensus(RaftServer *server, RaftClusterConfig *new_config) {
    if (!server || !new_config) return -1;

    server->new_config = new_config;
    server->joint_consensus = true;

    return 0;
}

int raft_commit_joint_config(RaftServer *server) {
    if (!server) return -1;

    if (server->new_config) {
        free(server->config);
        server->config = server->new_config;
        server->new_config = NULL;
    }

    server->joint_consensus = false;
    return 0;
}

int raft_handle_configuration(RaftServer *server,
                              const RaftClusterConfig *config,
                              bool is_joint) {
    (void)server; (void)config; (void)is_joint;
    /* 简化实现 */
    return 0;
}

/* ========================================================================
 * 故障检测
 * ======================================================================== */

RaftFailureDetector *raft_failure_detector_create(size_t num_nodes,
                                                   uint64_t heartbeat_timeout) {
    RaftFailureDetector *detector = (RaftFailureDetector *)calloc(1, sizeof(RaftFailureDetector));
    if (!detector) return NULL;

    detector->num_nodes = num_nodes;
    detector->heartbeat_timeout = heartbeat_timeout;
    detector->last_heartbeat = (uint64_t *)calloc(num_nodes, sizeof(uint64_t));
    detector->is_alive = (bool *)calloc(num_nodes, sizeof(bool));

    for (size_t i = 0; i < num_nodes; i++) {
        detector->is_alive[i] = true;
        detector->last_heartbeat[i] = get_current_time_ms();
    }

    return detector;
}

void raft_failure_detector_record_heartbeat(RaftFailureDetector *detector, uint64_t node_id) {
    if (!detector || node_id >= detector->num_nodes) return;
    detector->last_heartbeat[node_id] = get_current_time_ms();
    detector->is_alive[node_id] = true;
}

bool raft_failure_detector_is_alive(RaftFailureDetector *detector, uint64_t node_id) {
    if (!detector || node_id >= detector->num_nodes) return false;

    uint64_t elapsed = get_current_time_ms() - detector->last_heartbeat[node_id];
    return elapsed < detector->heartbeat_timeout;
}

uint64_t *raft_failure_detector_get_suspected_nodes(RaftFailureDetector *detector,
                                                    size_t *out_count) {
    if (!detector || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    static uint64_t suspected[256];
    size_t count = 0;

    for (size_t i = 0; i < detector->num_nodes && count < 256; i++) {
        uint64_t elapsed = get_current_time_ms() - detector->last_heartbeat[i];
        if (elapsed >= detector->heartbeat_timeout) {
            suspected[count++] = i;
        }
    }

    *out_count = count;
    return suspected;
}

void raft_failure_detector_destroy(RaftFailureDetector *detector) {
    if (!detector) return;
    free(detector->last_heartbeat);
    free(detector->is_alive);
    free(detector);
}

int raft_trigger_leader_election(RaftServer *server) {
    if (!server) return -1;

    server->role = RAFT_ROLE_CANDIDATE;
    return raft_start_election(server);
}

int raft_transfer_leadership(RaftServer *server, uint64_t target_node_id) {
    (void)server; (void)target_node_id;
    /* 简化实现 */
    return 0;
}

/* ========================================================================
 * 线性一致性读
 * ======================================================================== */

uint64_t raft_get_read_index(RaftServer *server) {
    if (!server || server->role != RAFT_ROLE_LEADER) return UINT64_MAX;
    return server->commit_index;
}

int raft_confirm_read_index(RaftServer *server, uint64_t read_index) {
    (void)server; (void)read_index;
    /* 简化实现 */
    return 0;
}

bool raft_can_lease_read(RaftServer *server) {
    if (!server || server->role != RAFT_ROLE_LEADER) return false;

    /* Lease Read 需要 CheckQuorum */
    return true;
}

int raft_lease_read(RaftServer *server, int (*callback)(void *, size_t), void *ctx) {
    if (!server || server->role != RAFT_ROLE_LEADER) return -1;

    /* 简化：直接调用回调 */
    if (callback) {
        return callback(ctx, 0);
    }
    return 0;
}

void raft_update_lease(RaftServer *server) {
    /* 简化实现 */
    (void)server;
}

bool raft_verify_leader(RaftServer *server) {
    return server && server->role == RAFT_ROLE_LEADER;
}

/* ========================================================================
 * 快照
 * ======================================================================== */

int raft_create_snapshot(RaftServer *server,
                        uint64_t last_included_index,
                        uint64_t last_included_term,
                        void *snapshot_data,
                        size_t snapshot_size) {
    (void)snapshot_data;

    if (!server) return -1;

    pthread_mutex_lock(&server->lock);

    /* 截断日志 */
    if (last_included_index > 0 && last_included_index <= server->log_size) {
        /* 保留 last_included_index 之后的日志 */
        size_t new_size = server->log_size - last_included_index;
        for (size_t i = 0; i < new_size; i++) {
            server->log[i] = server->log[last_included_index + i];
        }
        server->log_size = new_size;
    } else {
        server->log_size = 0;
    }

    server->last_snapshot_index = last_included_index;
    server->last_snapshot_term = last_included_term;

    pthread_mutex_unlock(&server->lock);

    return 0;
}

int raft_restore_snapshot(RaftServer *server,
                         uint64_t last_included_index,
                         uint64_t last_included_term,
                         const void *snapshot_data,
                         size_t snapshot_size) {
    (void)snapshot_data; (void)snapshot_size;

    if (!server) return -1;

    pthread_mutex_lock(&server->lock);

    server->log_size = 0;
    server->commit_index = last_included_index;
    server->last_applied = last_included_index;
    server->last_snapshot_index = last_included_index;
    server->last_snapshot_term = last_included_term;

    pthread_mutex_unlock(&server->lock);

    return 0;
}

int raft_install_snapshot(RaftServer *server,
                          uint64_t last_included_index,
                          uint64_t last_included_term,
                          const void *snapshot_data,
                          size_t snapshot_size) {
    (void)snapshot_data; (void)snapshot_size;

    if (!server) return -1;

    /* 简化：直接恢复快照 */
    return raft_restore_snapshot(server, last_included_index, last_included_term,
                                  snapshot_data, snapshot_size);
}

int raft_send_snapshot(RaftServer *server, uint64_t to_node) {
    (void)to_node;

    if (!server || server->role != RAFT_ROLE_LEADER) return -1;

    /* 发送快照给跟随者 */
    RaftMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = RAFT_MSG_INSTALL_SNAPSHOT;
    msg.from_node = server->node_id;
    msg.term = server->current_term;
    msg.data.install_snapshot.last_included_index = server->last_snapshot_index;
    msg.data.install_snapshot.last_included_term = server->last_snapshot_term;

    return raft_send_message(server, &msg);
}

int raft_handle_install_snapshot(RaftServer *server,
                                 uint64_t leader_id,
                                 uint64_t last_included_index,
                                 uint64_t last_included_term,
                                 const void *snapshot_data,
                                 size_t snapshot_size) {
    (void)leader_id; (void)snapshot_data;

    if (!server) return -1;

    pthread_mutex_lock(&server->lock);

    /* 任期检查 */
    if (last_included_index <= server->commit_index) {
        pthread_mutex_unlock(&server->lock);
        return 0;  /* 已有的快照更新 */
    }

    /* 恢复快照 */
    server->log_size = 0;
    server->commit_index = last_included_index;
    server->last_applied = last_included_index;
    server->last_snapshot_index = last_included_index;
    server->last_snapshot_term = last_included_term;

    pthread_mutex_unlock(&server->lock);

    return 0;
}

const RaftSnapshot *raft_get_snapshot_metadata(RaftServer *server) {
    static RaftSnapshot snap;

    if (!server) return NULL;

    snap.last_included_index = server->last_snapshot_index;
    snap.last_included_term = server->last_snapshot_term;
    snap.config = server->config;

    return &snap;
}

bool raft_need_snapshot(RaftServer *server, uint64_t next_index) {
    if (!server) return false;
    return next_index <= server->last_snapshot_index;
}

/* ========================================================================
 * 定时器
 * ======================================================================== */

void raft_run_election_timer(RaftServer *server) {
    if (!server) return;

    pthread_mutex_lock(&server->lock);

    if (server->role == RAFT_ROLE_FOLLOWER || server->role == RAFT_ROLE_CANDIDATE) {
        if (raft_check_election_timeout(server)) {
            pthread_mutex_unlock(&server->lock);
            raft_start_election(server);
            return;
        }
    }

    pthread_mutex_unlock(&server->lock);
}

void raft_run_heartbeat(RaftServer *server) {
    if (!server || server->role != RAFT_ROLE_LEADER) return;

    static uint64_t last_heartbeat = 0;
    uint64_t now = get_current_time_ms();

    if (now - last_heartbeat >= server->heartbeat_interval) {
        raft_broadcast_heartbeat(server);
        last_heartbeat = now;
    }
}

void raft_run_snapshot_check(RaftServer *server) {
    (void)server;
    /* 简化实现 */
}

void raft_run_lease_update(RaftServer *server) {
    if (server && server->role == RAFT_ROLE_LEADER) {
        raft_update_lease(server);
    }
}

/* ========================================================================
 * 统计
 * ======================================================================== */

void raft_get_stats(RaftServer *server, RaftStats *out_stats) {
    if (!server || !out_stats) return;

    memset(out_stats, 0, sizeof(RaftStats));

    out_stats->current_term = server->current_term;
    out_stats->role = server->role;
    out_stats->commit_index = server->commit_index;
    out_stats->last_applied = server->last_applied;
    out_stats->log_size = server->log_size;
    out_stats->voted_count = server->voted_count;
}

void raft_print_status(RaftServer *server) {
    if (!server) return;

    const char *role_str = "UNKNOWN";
    switch (server->role) {
    case RAFT_ROLE_FOLLOWER: role_str = "FOLLOWER"; break;
    case RAFT_ROLE_CANDIDATE: role_str = "CANDIDATE"; break;
    case RAFT_ROLE_LEADER: role_str = "LEADER"; break;
    }

    printf("Raft Node %lu: term=%lu role=%s commit_index=%lu last_applied=%lu log_size=%lu\n",
           (unsigned long)server->node_id,
           (unsigned long)server->current_term,
           role_str,
           (unsigned long)server->commit_index,
           (unsigned long)server->last_applied,
           (unsigned long)server->log_size);
}
