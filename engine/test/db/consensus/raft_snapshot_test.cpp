/**
 * @file raft_snapshot_test.cpp
 * @brief P7 — Raft Snapshot + Log Compaction smoke 测试
 *
 * 测试场景：
 * - TakeSnapshot: 正常创建 snapshot + 日志压缩
 * - InstallSnapshot: 安装 snapshot（清空日志）
 * - SnapshotRejectsUncommitted: 不能 snapshot 未提交的日志
 * - GetSnapshotAfterTake: 读回 snapshot 元信息
 * - SnapshotEmptyData: 空应用层数据也能 snapshot
 * - HasSnapshotFlag: 查询 raft_has_snapshot 状态
 * - CompactThenAppend: 压缩后还能继续 append（first_log_index 位移）
 * - FirstLogIndexAfterCompaction: 压缩后 first_log_index 正确
 */

#include <gtest/gtest.h>
#include "db/consensus/raft.h"

#include <cstdint>
#include <cstring>

namespace {

RaftServerConfig_t make_config(uint64_t node_id = 1) {
    RaftServerConfig_t cfg = {};
    cfg.node_id = node_id;
    cfg.cluster_size = 1;
    cfg.heartbeat_interval_ms = 150;
    cfg.election_timeout_min_ms = 200;
    cfg.election_timeout_max_ms = 400;
    return cfg;
}

/* 助手：建 server → 变为 leader → 提交 N 条日志 */
RaftServer_t *create_server_with_log(int n) {
    RaftServerConfig_t cfg = make_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    raft_server_start(srv);
    raft_test_force_election_timeout(srv);
    raft_tick(srv);
    EXPECT_TRUE(raft_is_leader(srv));

    const char *data = "payload";
    for (int i = 0; i < n; i++) {
        raft_submit(srv, data, strlen(data) + 1);
    }
    return srv;
}

/* ============================================================
 * 基本 Snapshot 测试
 * ============================================================ */

TEST(RaftSnapshot, TakeSnapshot) {
    RaftServer_t *srv = create_server_with_log(5);
    EXPECT_EQ(raft_get_commit_index(srv), 5u);

    /* snapshot 已提交的 3 条（留下后 2 条） */
    EXPECT_EQ(raft_take_snapshot(srv, "snap", 5, 3), 0);

    /* snapshot 已存在 */
    EXPECT_TRUE(raft_has_snapshot(srv));
    EXPECT_EQ(raft_get_snapshot_last_index(srv), 3u);

    /* first_log_index 应前进到 4 */
    EXPECT_EQ(raft_get_first_log_index(srv), 4u);

    /* 还能继续 append */
    const char *more = "new";
    size_t idx = raft_submit(srv, more, strlen(more) + 1);
    EXPECT_EQ(idx, 6u);

    raft_server_destroy(srv);
}

TEST(RaftSnapshot, TakeSnapshotRejectsUncommitted) {
    RaftServer_t *srv = create_server_with_log(3);

    /* commit_index=3，尝试 snapshot index=4（未提交）→ 拒绝 */
    EXPECT_EQ(raft_take_snapshot(srv, NULL, 0, 4), -1);

    EXPECT_FALSE(raft_has_snapshot(srv));

    raft_server_destroy(srv);
}

TEST(RaftSnapshot, InstallSnapshot) {
    RaftServerConfig_t cfg = make_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    raft_server_start(srv);

    const char *snap_data = "installed-state";
    EXPECT_EQ(raft_install_snapshot(srv, snap_data, strlen(snap_data) + 1,
                                    10 /* last index */, 3 /* last term */), 0);

    EXPECT_TRUE(raft_has_snapshot(srv));
    EXPECT_EQ(raft_get_snapshot_last_index(srv), 10u);
    EXPECT_EQ(raft_get_snapshot_last_term(srv), 3u);
    EXPECT_EQ(raft_get_commit_index(srv), 10u);
    EXPECT_EQ(raft_get_last_applied(srv), 10u);
    EXPECT_EQ(raft_get_first_log_index(srv), 11u);

    /* 读回 snapshot */
    char buf[64] = {};
    size_t out_size = 0;
    EXPECT_EQ(raft_get_snapshot(srv, buf, sizeof(buf), &out_size, NULL, NULL), 0);
    EXPECT_GT(out_size, 0u);
    EXPECT_STREQ(buf, "installed-state");

    raft_server_destroy(srv);
}

TEST(RaftSnapshot, GetSnapshotReturnsMeta) {
    RaftServer_t *srv = create_server_with_log(5);

    /* snapshot 无 snapshot → 返回 -1 */
    size_t out_size = 99;
    EXPECT_EQ(raft_get_snapshot(srv, NULL, 0, &out_size, NULL, NULL), -1);

    raft_take_snapshot(srv, "data", 5, 3);

    uint64_t last_idx = 0, last_term = 0;
    EXPECT_EQ(raft_get_snapshot(srv, NULL, 0, &out_size, &last_idx, &last_term), 0);
    EXPECT_EQ(out_size, 5u);
    EXPECT_EQ(last_idx, 3u);

    raft_server_destroy(srv);
}

TEST(RaftSnapshot, SnapshotEmptyData) {
    RaftServer_t *srv = create_server_with_log(3);

    /* snapshot_data=NULL, data_size=0 允许（仅压缩日志，无应用层数据） */
    EXPECT_EQ(raft_take_snapshot(srv, NULL, 0, 3), 0);
    EXPECT_TRUE(raft_has_snapshot(srv));
    EXPECT_EQ(raft_get_snapshot_last_index(srv), 3u);

    /* 读回 snapshot 应报告 size=0 */
    size_t out_size = 99;
    uint64_t last_idx = 0;
    EXPECT_EQ(raft_get_snapshot(srv, NULL, 0, &out_size, &last_idx, NULL), 0);
    EXPECT_EQ(out_size, 0u);

    raft_server_destroy(srv);
}

TEST(RaftSnapshot, InstallSnapshotClearsLog) {
    RaftServer_t *srv = create_server_with_log(5);
    EXPECT_EQ(raft_get_commit_index(srv), 5u);

    /* install snapshot 大幅前进 */
    raft_install_snapshot(srv, "snap", 5, 100, 10);

    /* 日志已被清空，commit/applied 跳转到 100 */
    EXPECT_EQ(raft_get_commit_index(srv), 100u);
    EXPECT_EQ(raft_get_last_applied(srv), 100u);

    /* first_log_index = 101 */
    EXPECT_EQ(raft_get_first_log_index(srv), 101u);

    raft_server_destroy(srv);
}

TEST(RaftSnapshot, CompactThenContinueAppend) {
    RaftServer_t *srv = create_server_with_log(10);
    EXPECT_EQ(raft_get_commit_index(srv), 10u);

    /* 压缩掉前 6 条 */
    EXPECT_EQ(raft_take_snapshot(srv, "snap", 5, 6), 0);
    EXPECT_EQ(raft_get_first_log_index(srv), 7u);

    /* 剩余日志 4 条（index 7, 8, 9, 10） */
    /* 继续提交 */
    const char *data = "extra";
    size_t idx = raft_submit(srv, data, strlen(data) + 1);
    EXPECT_EQ(idx, 11u);  /* 全局递增索引 */
    EXPECT_EQ(raft_get_commit_index(srv), 11u);

    /* 再 snapshot 一次：全量压缩 */
    EXPECT_EQ(raft_take_snapshot(srv, "snap2", 6, 11), 0);
    EXPECT_EQ(raft_get_snapshot_last_index(srv), 11u);
    EXPECT_EQ(raft_get_first_log_index(srv), 12u);

    raft_server_destroy(srv);
}

TEST(RaftSnapshot, HasSnapshotFlag) {
    RaftServerConfig_t cfg = make_config();
    RaftServer_t *srv = raft_server_create(&cfg);
    EXPECT_FALSE(raft_has_snapshot(srv));

    raft_install_snapshot(srv, "x", 1, 5, 1);
    EXPECT_TRUE(raft_has_snapshot(srv));

    raft_server_destroy(srv);
}

}  // namespace
