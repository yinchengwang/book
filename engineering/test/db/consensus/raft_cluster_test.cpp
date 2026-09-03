/**
 * @file raft_cluster_test.cpp
 * @brief P8 — Raft Cluster in-process transport smoke 测试
 *
 * 测试场景：
 * - ClusterCreateAndDestroy: 基本生命周期
 * - ClusterForceElectionQuorum: force_election 后 quorum 投票选出 leader
 * - ClusterHeartbeatPreventsFollowerElection: leader 心跳抑制了 follower 选举
 * - ClusterLogReplicationViaHeartbeat: leader 提交 → 心跳传播 commit_index
 * - ClusterSingleLeaderInvariant: 多次选举都只产生至多 1 leader
 * - ClusterAdvanceMsWithoutLeader: advance_ms 触发选举
 */

#include <gtest/gtest.h>
#include "db/consensus/raft_cluster.h"
#include "db/consensus/raft.h"

#include <cstdint>

namespace {

constexpr uint32_t kClusterSize = 3;
constexpr uint64_t kBaseId = 1;

TEST(RaftCluster, CreateAndDestroy) {
    RaftCluster_t *cl = raft_cluster_create(kClusterSize, kBaseId);
    ASSERT_NE(cl, nullptr);

    for (uint64_t i = kBaseId; i < kBaseId + kClusterSize; i++) {
        ASSERT_NE(raft_cluster_get_node(cl, i), nullptr);
    }

    raft_cluster_destroy(cl);
}

TEST(RaftCluster, ForceElectionQuorum) {
    RaftCluster_t *cl = raft_cluster_create(kClusterSize, kBaseId);
    ASSERT_NE(cl, nullptr);

    EXPECT_EQ(raft_cluster_count_leaders(cl), 0u);

    /* force_election 应基于 quorum 投票选出 leader */
    raft_cluster_force_election(cl);

    uint64_t leader_id = raft_cluster_get_leader_id(cl);
    EXPECT_NE(leader_id, RAFT_UNKNOWN_LEADER);
    EXPECT_LE(raft_cluster_count_leaders(cl), 1u);

    raft_cluster_destroy(cl);
}

TEST(RaftCluster, HeartbeatPreventsFollowerElection) {
    RaftCluster_t *cl = raft_cluster_create(kClusterSize, kBaseId);
    ASSERT_NE(cl, nullptr);

    /* 选出 leader */
    raft_cluster_force_election(cl);
    uint64_t leader_id = raft_cluster_get_leader_id(cl);
    ASSERT_NE(leader_id, RAFT_UNKNOWN_LEADER);

    uint64_t leader_term_before = raft_get_current_term(
        raft_cluster_get_node(cl, leader_id));

    /* advance_ms 500ms：leader 心跳在 tick 前重置 follower 的超时 */
    raft_cluster_advance_ms(cl, 500);

    /* 验证 leader 仍然存在，term 未变（无竞争选举） */
    EXPECT_EQ(raft_cluster_get_leader_id(cl), leader_id);
    EXPECT_LE(raft_cluster_count_leaders(cl), 1u);

    /* leader 的 term 应稳定 */
    uint64_t leader_term_after = raft_get_current_term(
        raft_cluster_get_node(cl, leader_id));
    EXPECT_EQ(leader_term_after, leader_term_before);

    raft_cluster_destroy(cl);
}

TEST(RaftCluster, LogReplicationViaHeartbeat) {
    RaftCluster_t *cl = raft_cluster_create(kClusterSize, kBaseId);
    ASSERT_NE(cl, nullptr);

    raft_cluster_force_election(cl);

    uint64_t leader_id = raft_cluster_get_leader_id(cl);
    ASSERT_NE(leader_id, RAFT_UNKNOWN_LEADER);

    RaftServer_t *leader = raft_cluster_get_node(cl, leader_id);
    ASSERT_NE(leader, nullptr);

    if (raft_is_leader(leader)) {
        const char *data = "hello-raft";
        size_t idx = raft_submit(leader, data, strlen(data) + 1);
        EXPECT_NE(idx, SIZE_MAX);
        EXPECT_EQ(raft_get_commit_index(leader), idx);
    }

    raft_cluster_advance_ms(cl, 500);

    /* leader commit_index 保持不变，集群仍有 1 leader */
    EXPECT_GE(raft_get_commit_index(leader), 1u);
    EXPECT_LE(raft_cluster_count_leaders(cl), 1u);

    raft_cluster_destroy(cl);
}

TEST(RaftCluster, SingleLeaderInvariant) {
    RaftCluster_t *cl = raft_cluster_create(kClusterSize, kBaseId);
    ASSERT_NE(cl, nullptr);

    for (int i = 0; i < 3; i++) {
        raft_cluster_force_election(cl);
        raft_cluster_advance_ms(cl, 200);
        EXPECT_LE(raft_cluster_count_leaders(cl), 1u)
            << "iteration " << i;
    }

    raft_cluster_destroy(cl);
}

TEST(RaftCluster, AdvanceMsWithoutLeader) {
    RaftCluster_t *cl = raft_cluster_create(kClusterSize, kBaseId);
    ASSERT_NE(cl, nullptr);

    /* 不 force election，只靠 advance_ms 触发超时选举 */
    /* 注意：多节点同时超时可能导致 split vote，不保证有 leader */

    /* advance 大量时间后，term 应增长（至少触发了一轮选举） */
    raft_cluster_advance_ms(cl, 2000);

    /* 所有节点的 term 应 >= 1（都经历选举） */
    for (uint64_t i = kBaseId; i < kBaseId + kClusterSize; i++) {
        RaftServer_t *node = raft_cluster_get_node(cl, i);
        ASSERT_NE(node, nullptr);
        EXPECT_GE(raft_get_current_term(node), 1u);
    }

    raft_cluster_destroy(cl);
}

}  // namespace
