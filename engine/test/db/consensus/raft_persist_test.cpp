/**
 * @file raft_persist_test.cpp
 * @brief P5 — Raft 状态持久化 smoke 测试
 *
 * 测试场景：
 * - SaveState: raft_server_save_state() 写出 bin 文件
 * - LoadRecover: create server B → load 文件 → 状态恢复
 * - RejectNoPath: state_path=NULL 时拒绝 save
 * - TermPersist: current_term round-trip
 * - LogPersist: 日志 entries round-trip
 */

#include <gtest/gtest.h>
#include "db/consensus/raft.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

namespace {

constexpr char kStatePath[] = "raft_state_test.bin";

RaftServerConfig_t make_config(uint64_t node_id = 42) {
    RaftServerConfig_t cfg = {};
    cfg.node_id = node_id;
    cfg.cluster_size = 3;
    cfg.heartbeat_interval_ms = 150;
    cfg.election_timeout_min_ms = 200;
    cfg.election_timeout_max_ms = 400;
    return cfg;
}

void clean_state_file() {
    std::remove(kStatePath);
}

class RaftPersist : public ::testing::Test {
protected:
    void SetUp() override { clean_state_file(); }
    void TearDown() override { clean_state_file(); }
};

TEST_F(RaftPersist, SaveStateWithoutPathFails) {
    RaftServerConfig_t cfg = make_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    ASSERT_NE(srv, nullptr);

    /* state_path=NULL 时 save 应失败 */
    EXPECT_EQ(raft_server_save_state(srv), -1);

    raft_server_destroy(srv);
}

TEST_F(RaftPersist, SaveAndLoadEmptyState) {
    RaftServerConfig_t cfg = make_config();
    RaftStateConfig_t scfg = {};
    scfg.state_path = kStatePath;

    RaftServer_t *srv = raft_server_create_ex(&cfg, &scfg);
    ASSERT_NE(srv, nullptr);

    /* 修改 term（vote 后）然后 save */
    RaftRequestVoteArgs_t va = {7, 42, 0, 0};
    EXPECT_TRUE(raft_handle_request_vote(srv, &va).vote_granted);
    EXPECT_EQ(raft_get_current_term(srv), 7u);

    EXPECT_EQ(raft_server_save_state(srv), 0);
    raft_server_destroy(srv);

    /* 装载到新 server */
    RaftServer_t *srv2 = raft_server_create_ex(&cfg, &scfg);
    ASSERT_NE(srv2, nullptr);
    EXPECT_EQ(raft_get_current_term(srv2), 7u);

    raft_server_destroy(srv2);
}

TEST_F(RaftPersist, LogRoundTrip) {
    RaftServerConfig_t cfg = make_config();
    RaftStateConfig_t scfg = {};
    scfg.state_path = kStatePath;

    RaftServer_t *srv = raft_server_create_ex(&cfg, &scfg);
    ASSERT_NE(srv, nullptr);

    /* AppendEntries from leader 模拟 */
    RaftLogEntry_t entries[3];
    const char *data[] = {"alpha", "bravo", "charlie"};
    for (int i = 0; i < 3; i++) {
        entries[i].term = 5;
        entries[i].data = (void *)data[i];
        entries[i].data_size = strlen(data[i]) + 1;
    }

    RaftAppendEntriesArgs_t ae = {0};
    ae.term = 5;
    ae.leader_id = 1;
    ae.prev_log_index = 0;
    ae.prev_log_term = 0;
    ae.leader_commit = 3;
    ae.entries = entries;
    ae.entry_count = 3;
    EXPECT_TRUE(raft_handle_append_entries(srv, &ae).success);

    /* 验证日志存在（私有，但可用 raft_get_commit_index 间接确认） */
    EXPECT_EQ(raft_get_commit_index(srv), 3u);

    EXPECT_EQ(raft_server_save_state(srv), 0);
    raft_server_destroy(srv);

    /* 重建后状态恢复：term=5、commit=3 */
    RaftServer_t *srv2 = raft_server_create_ex(&cfg, &scfg);
    ASSERT_NE(srv2, nullptr);
    EXPECT_EQ(raft_get_current_term(srv2), 5u);
    EXPECT_EQ(raft_get_commit_index(srv2), 3u);

    raft_server_destroy(srv2);
}

TEST_F(RaftPersist, MultipleSavesStable) {
    RaftServerConfig_t cfg = make_config();
    RaftStateConfig_t scfg = {};
    scfg.state_path = kStatePath;

    RaftServer_t *srv = raft_server_create_ex(&cfg, &scfg);
    ASSERT_NE(srv, nullptr);

    /* 多次保存：每次都成功 */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(raft_server_save_state(srv), 0);
    }
    raft_server_destroy(srv);

    /* 文件非空 */
    FILE *fp = std::fopen(kStatePath, "rb");
    ASSERT_NE(fp, nullptr);
    long size = 0;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fclose(fp);
    EXPECT_GT(size, 0);
}

TEST_F(RaftPersist, ResetClearsState) {
    RaftServerConfig_t cfg = make_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    raft_server_start(srv);

    RaftRequestVoteArgs_t va = {3, 42, 0, 0};
    raft_handle_request_vote(srv, &va);
    EXPECT_EQ(raft_get_current_term(srv), 3u);

    raft_test_reset_state(srv);
    EXPECT_EQ(raft_get_current_term(srv), 0u);

    raft_server_destroy(srv);
}

}  // namespace
