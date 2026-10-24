/**
 * @file raft_test.cpp
 * @brief Raft 模块 smoke 测试（Phase11）
 *
 * 测试 3 个：
 * - CreateAndDestroy：基本生命周期
 * - InitialRoleFollower：默认 follower
 * - TickTriggersElection：tick 推进触发 candidate → leader (单节点)
 * - LogAppend: leader 接受日志 + commit
 */

#include <gtest/gtest.h>
#include "db/consensus/raft.h"

#include <chrono>
#include <cstdint>
#include <thread>

namespace {

RaftServerConfig_t make_single_node_config() {
    RaftServerConfig_t cfg = {};
    cfg.node_id = 1;
    cfg.cluster_size = 1;
    cfg.heartbeat_interval_ms = 150;
    cfg.election_timeout_min_ms = 200;
    cfg.election_timeout_max_ms = 400;
    return cfg;
}

TEST(RaftSmoke, CreateAndDestroy) {
    RaftServerConfig_t cfg = make_single_node_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    ASSERT_NE(srv, nullptr);
    raft_server_destroy(srv);
}

TEST(RaftSmoke, InitialRoleFollower) {
    RaftServerConfig_t cfg = make_single_node_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    EXPECT_EQ(raft_get_role(srv), RAFT_ROLE_FOLLOWER);
    EXPECT_EQ(raft_get_current_term(srv), 0u);
    EXPECT_EQ(raft_get_leader_id(srv), UINT64_MAX);
    EXPECT_FALSE(raft_is_leader(srv));
    raft_server_destroy(srv);
}

TEST(RaftSmoke, ForceElectionTickBecomesLeader) {
    RaftServerConfig_t cfg = make_single_node_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    raft_server_start(srv);

    /* Phase11 实现：直接 force + tick 推进 */
    raft_test_force_election_timeout(srv);
    raft_tick(srv);

    /* 单节点立刻当选 */
    EXPECT_EQ(raft_get_role(srv), RAFT_ROLE_LEADER);
    EXPECT_EQ(raft_get_leader_id(srv), cfg.node_id);
    EXPECT_TRUE(raft_is_leader(srv));
    EXPECT_GE(raft_get_current_term(srv), 1u);

    raft_server_destroy(srv);
}

TEST(RaftSmoke, LeaderSubmitAppendsToLog) {
    RaftServerConfig_t cfg = make_single_node_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    raft_server_start(srv);

    raft_test_force_election_timeout(srv);
    raft_tick(srv);
    ASSERT_TRUE(raft_is_leader(srv));

    /* 提交 3 条数据 */
    const char *msgs[] = {"alpha", "bravo", "charlie"};
    for (int i = 0; i < 3; i++) {
        size_t idx = raft_submit(srv, msgs[i], strlen(msgs[i]) + 1);
        EXPECT_EQ(idx, (size_t)(i + 1));
    }

    EXPECT_EQ(raft_get_commit_index(srv), 3u);

    /* 读出 */
    char buf[64] = {};
    size_t n = raft_get_committed(srv, 0, buf, sizeof(buf));
    EXPECT_GT(n, 0u);
    /* 内容应至少含 "alpha" */
    EXPECT_NE(strstr(buf, "alpha"), nullptr);

    raft_server_destroy(srv);
}

TEST(RaftSmoke, FollowerRejectsSubmit) {
    RaftServerConfig_t cfg = make_single_node_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    /* 不 force election → 始终是 follower */
    EXPECT_EQ(raft_get_role(srv), RAFT_ROLE_FOLLOWER);

    size_t idx = raft_submit(srv, "x", 2);
    EXPECT_EQ(idx, SIZE_MAX);

    raft_server_destroy(srv);
}

TEST(RaftSmoke, HeartbeatResetsElectionTimeout) {
    RaftServerConfig_t cfg = make_single_node_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    raft_server_start(srv);

    /* follower 收到 term 1 心跳 → 应变 term=1，role=FOLLOWER */
    raft_test_receive_heartbeat(srv, 999 /* from */, 1 /* term */);
    EXPECT_EQ(raft_get_role(srv), RAFT_ROLE_FOLLOWER);
    EXPECT_EQ(raft_get_leader_id(srv), 999u);

    raft_server_destroy(srv);
}

}  // namespace
