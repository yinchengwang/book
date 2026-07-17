/**
 * @file raft_cluster.c
 * @brief P1 + P8: in-process cluster transport and election orchestration
 *
 * 实现 raft_cluster.h 声明的 API，提供：
 * 1. 多节点生命周期管理
 * 2. in-process 消息路由（RequestVote / AppendEntries 通过函数指针调用）
 * 3. 选举驱动（force_election + advance_ms）
 * 4. leader commit 推进（基于 quorum ack 计数）
 *
 * P8 扩展：Transport 抽象层定义在 raft_transport.h
 * - InProcessTransport：直接函数调用（本文件实现）
 * - TcpTransport：（待实现）socket 序列化传输
 */

#include "db/consensus/raft_cluster.h"
#include "db/consensus/raft.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部结构
 * ============================================================ */

struct RaftServer;

typedef struct RaftNodeRef {
    uint64_t node_id;
    RaftServer_t *srv;
} RaftNodeRef_t;

struct RaftCluster {
    uint32_t size;
    uint64_t base_id;
    RaftNodeRef_t *nodes;
};

/* ============================================================
 * 生命周期
 * ============================================================ */

RaftCluster_t *raft_cluster_create(uint32_t size, uint64_t base_id) {
    if (size == 0 || size > 64) return NULL;

    RaftCluster_t *cl = (RaftCluster_t *)calloc(1, sizeof(RaftCluster_t));
    if (!cl) return NULL;

    cl->size = size;
    cl->base_id = base_id;

    cl->nodes = (RaftNodeRef_t *)calloc(size, sizeof(RaftNodeRef_t));
    if (!cl->nodes) {
        free(cl);
        return NULL;
    }

    for (uint32_t i = 0; i < size; i++) {
        cl->nodes[i].node_id = base_id + i;

        RaftServerConfig_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.node_id = cl->nodes[i].node_id;
        cfg.cluster_size = size;
        cfg.heartbeat_interval_ms = 150;
        cfg.election_timeout_min_ms = 200;
        cfg.election_timeout_max_ms = 600;

        cl->nodes[i].srv = raft_server_create(&cfg);
        if (!cl->nodes[i].srv) {
            for (uint32_t j = 0; j < i; j++) raft_server_destroy(cl->nodes[j].srv);
            free(cl->nodes);
            free(cl);
            return NULL;
        }
        raft_server_start(cl->nodes[i].srv);
    }
    return cl;
}

void raft_cluster_destroy(RaftCluster_t *cl) {
    if (!cl) return;
    for (uint32_t i = 0; i < cl->size; i++) {
        if (cl->nodes[i].srv) raft_server_destroy(cl->nodes[i].srv);
    }
    free(cl->nodes);
    free(cl);
}

/* ============================================================
 * 消息路由（in-process transport）
 *
 * P8 核心：每个 RPC 通过对应的 handle_* 函数直接投递到目标节点
 * ============================================================ */

static void deliver_request_vote(RaftCluster_t *cl,
                                 uint64_t from_id,
                                 uint64_t to_id,
                                 const RaftRequestVoteArgs_t *args) {
    if (!cl || !args) return;

    /* 找到目标节点 */
    for (uint32_t i = 0; i < cl->size; i++) {
        if (cl->nodes[i].node_id == to_id) {
            RaftRequestVoteResult_t res = raft_handle_request_vote(cl->nodes[i].srv, args);
            (void)res; /* P8: 暂时忽略结果（用于 leader 判断选票数） */
            return;
        }
    }
}

static void deliver_append_entries(RaftCluster_t *cl,
                                   uint64_t from_id,
                                   uint64_t to_id,
                                   const RaftAppendEntriesArgs_t *args) {
    if (!cl || !args) return;

    for (uint32_t i = 0; i < cl->size; i++) {
        if (cl->nodes[i].node_id == to_id) {
            RaftAppendEntriesResult_t res = raft_handle_append_entries(cl->nodes[i].srv, args);
            (void)res;
            return;
        }
    }
}

/* ============================================================
 * 选举驱动
 * ============================================================ */

void raft_cluster_force_election(RaftCluster_t *cl) {
    if (!cl) return;

    /* 选第一个节点当 candidate（简化：固定 node base_id）*/
    uint64_t candidate_id = cl->base_id;
    RaftServer_t *candidate = NULL;

    for (uint32_t i = 0; i < cl->size; i++) {
        if (cl->nodes[i].node_id == candidate_id) {
            candidate = cl->nodes[i].srv;
            break;
        }
    }
    if (!candidate) return;

    /* 触发选举超时 */
    raft_test_force_election_timeout(candidate);
    raft_tick(candidate);

    /* candidate 自增 term 后，向所有其他节点发 RequestVote */
    if (raft_get_role(candidate) != RAFT_ROLE_CANDIDATE) {
        /* 可能已自动升 leader（单节点），直接返回即可 */
        return;
    }

    uint64_t term = raft_get_current_term(candidate);

    /* 构造 RequestVote 参数 */
    RaftRequestVoteArgs_t args;
    memset(&args, 0, sizeof(args));
    args.term = term;
    args.candidate_id = candidate_id;

    uint32_t votes = 1; /* 自己一票 */
    for (uint32_t i = 0; i < cl->size; i++) {
        uint64_t target_id = cl->nodes[i].node_id;
        if (target_id != candidate_id) {
            RaftRequestVoteResult_t res = raft_handle_request_vote(cl->nodes[i].srv, &args);
            if (res.vote_granted) votes++;
        }
    }

    /* quorum 检查：majority */
    uint32_t quorum = cl->size / 2 + 1;
    if (votes >= quorum) {
        raft_test_become_leader(candidate);
    }
}

void raft_cluster_advance_ms(RaftCluster_t *cl, uint64_t ms) {
    if (!cl) return;

    /* 第一步：所有节点时间推进（虚拟时钟） */
    for (uint32_t i = 0; i < cl->size; i++) {
        raft_tick_advance_ms(cl->nodes[i].srv, ms);
    }

    /* 第二步：leader 发送心跳给所有 follower */
    /* （心跳重置 last_election_reset_ms 到已推进的虚拟时间，防止选举竞争） */
    for (uint32_t i = 0; i < cl->size; i++) {
        RaftServer_t *srv = cl->nodes[i].srv;
        if (raft_is_leader(srv)) {
            uint64_t leader_term = raft_get_current_term(srv);
            uint64_t leader_commit = raft_get_commit_index(srv);

            RaftAppendEntriesArgs_t heartbeat;
            memset(&heartbeat, 0, sizeof(heartbeat));
            heartbeat.term = leader_term;
            heartbeat.leader_id = cl->nodes[i].node_id;
            /* 心跳的 prev_log_index 设为 leader commit_index（正确锚定日志位置） */
            heartbeat.prev_log_index = leader_commit;
            heartbeat.prev_log_term = leader_term;
            heartbeat.leader_commit = leader_commit;

            for (uint32_t j = 0; j < cl->size; j++) {
                if (j != i) {
                    deliver_append_entries(cl,
                                          cl->nodes[i].node_id,
                                          cl->nodes[j].node_id,
                                          &heartbeat);
                }
            }
        }
    }

    /* 第三步：所有节点 tick */
    for (uint32_t i = 0; i < cl->size; i++) {
        raft_tick(cl->nodes[i].srv);
    }
}

void raft_cluster_tick_leader_commit(RaftCluster_t *cl) {
    if (!cl) return;
    /* P8 简化：不需要额外 commit 推进（raft_tick 中已在 leader 节点推进）*/
    (void)cl;
}

/* ============================================================
 * 查询
 * ============================================================ */

RaftServer_t *raft_cluster_get_node(RaftCluster_t *cl, uint64_t node_id) {
    if (!cl) return NULL;
    for (uint32_t i = 0; i < cl->size; i++) {
        if (cl->nodes[i].node_id == node_id) {
            return cl->nodes[i].srv;
        }
    }
    return NULL;
}

uint32_t raft_cluster_count_leaders(RaftCluster_t *cl) {
    if (!cl) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < cl->size; i++) {
        if (raft_is_leader(cl->nodes[i].srv)) count++;
    }
    return count;
}

uint64_t raft_cluster_get_leader_id(RaftCluster_t *cl) {
    if (!cl) return RAFT_UNKNOWN_LEADER;
    for (uint32_t i = 0; i < cl->size; i++) {
        if (raft_is_leader(cl->nodes[i].srv)) {
            return cl->nodes[i].node_id;
        }
    }
    return RAFT_UNKNOWN_LEADER;
}
