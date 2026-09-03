/**
 * @file raft_cluster.h
 * @brief P1: in-process cluster abstraction
 */
#ifndef DB_RAFT_CLUSTER_H
#define DB_RAFT_CLUSTER_H

#include <stdint.h>
#include <stddef.h>
#include "db/consensus/raft.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RaftCluster RaftCluster_t;
typedef struct RaftNodeRef RaftNodeRef_t;

RaftCluster_t *raft_cluster_create(uint32_t size, uint64_t base_id);
void raft_cluster_destroy(RaftCluster_t *cl);

/* 推进所有节点时间，并投递 pending 消息 */
void raft_cluster_advance_ms(RaftCluster_t *cl, uint64_t ms);

/* 让 leader 推进 commit_index（call 后 quorum ack） */
void raft_cluster_tick_leader_commit(RaftCluster_t *cl);

/* 选择 leader（force 全部 follower 触发选举） */
void raft_cluster_force_election(RaftCluster_t *cl);

/* 获取 server */
RaftServer_t *raft_cluster_get_node(RaftCluster_t *cl, uint64_t node_id);

/* 统计 leader 数（应当 ≤ 1） */
uint32_t raft_cluster_count_leaders(RaftCluster_t *cl);

/* leader id（若有），否则 RAFT_UNKNOWN_LEADER */
uint64_t raft_cluster_get_leader_id(RaftCluster_t *cl);

#ifdef __cplusplus
}
#endif

#endif
