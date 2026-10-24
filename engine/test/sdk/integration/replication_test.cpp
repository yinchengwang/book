/**
 * @file replication_test.cpp
 * @brief Raft 复制协议测试
 *
 * 覆盖：
 *   1. LeaderInit - Leader 初始化测试
 *   2. FollowerInit - Follower 初始化测试
 *   3. ReplicationInfo - 状态查询测试
 *   4. Failover - 故障转移测试
 *   5. AppendLog - 日志追加测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_replication.h"
#include "sdk/impl/sqlite_backend.h"
}

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kDbPath = "test_replication.db";
constexpr const char* kPeers = R"([
    {"id":1,"addr":"localhost:5432"},
    {"id":2,"addr":"localhost:5433"},
    {"id":3,"addr":"localhost:5434"}
])";

void cleanup_db() {
    std::remove(kDbPath);
    std::remove((std::string(kDbPath) + "-wal").c_str());
    std::remove((std::string(kDbPath) + "-shm").c_str());
}

}  // namespace

class ReplicationTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;

    void SetUp() override {
        cleanup_db();
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
    }

    void TearDown() override {
        /* 停止复制 */
        mmdb_replication_stop(db_);

        if (db_) mmdb_close(db_);
        cleanup_db();
    }
};

/* 测试 1: Leader 初始化 */
TEST_F(ReplicationTest, LeaderInit) {
    int rc = mmdb_replication_init(db_, MMDB_REPLICA_LEADER, kPeers);
    ASSERT_EQ(rc, MMDB_OK);

    mmdb_replica_info_t info;
    ASSERT_EQ(mmdb_replication_info(db_, &info), MMDB_OK);
    ASSERT_EQ(info.role, MMDB_REPLICA_LEADER);
    ASSERT_EQ(info.node_id, 1u);
    ASSERT_EQ(info.term, 0u);
}

/* 测试 2: Follower 初始化 */
TEST_F(ReplicationTest, FollowerInit) {
    int rc = mmdb_replication_init(db_, MMDB_REPLICA_FOLLOWER, kPeers);
    ASSERT_EQ(rc, MMDB_OK);

    mmdb_replica_info_t info;
    ASSERT_EQ(mmdb_replication_info(db_, &info), MMDB_OK);
    ASSERT_EQ(info.role, MMDB_REPLICA_FOLLOWER);
    ASSERT_NE(info.leader_addr, nullptr);
}

/* 测试 3: 状态查询 */
TEST_F(ReplicationTest, ReplicationInfo) {
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_LEADER, kPeers), MMDB_OK);

    mmdb_replica_info_t info;
    ASSERT_EQ(mmdb_replication_info(db_, &info), MMDB_OK);

    /* Leader 初始状态 */
    ASSERT_EQ(info.role, MMDB_REPLICA_LEADER);
    ASSERT_EQ(info.commit_index, 0u);
    ASSERT_EQ(info.applied_index, 0u);
    ASSERT_TRUE(info.is_synced);
    ASSERT_EQ(info.leader_addr, nullptr);
}

/* 测试 4: 故障转移（Follower 发起选举） */
TEST_F(ReplicationTest, Failover) {
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_FOLLOWER, kPeers), MMDB_OK);

    /* 发起故障转移 */
    ASSERT_EQ(mmdb_replication_failover(db_), MMDB_OK);

    /* 验证角色变为 Candidate */
    mmdb_replica_info_t info;
    ASSERT_EQ(mmdb_replication_info(db_, &info), MMDB_OK);
    ASSERT_EQ(info.role, MMDB_REPLICA_CANDIDATE);
    ASSERT_GT(info.term, 0u);
}

/* 测试 5: Leader 不能发起故障转移 */
TEST_F(ReplicationTest, LeaderNoFailover) {
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_LEADER, kPeers), MMDB_OK);

    /* Leader 发起故障转移应失败 */
    ASSERT_EQ(mmdb_replication_failover(db_), MMDB_ERR_INTERNAL);
}

/* 测试 6: 日志追加（Leader） */
TEST_F(ReplicationTest, AppendLog) {
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_LEADER, kPeers), MMDB_OK);

    /* 追加日志 */
    const char* data = "test log entry";
    ASSERT_EQ(mmdb_replication_append_log(db_, data, strlen(data)), MMDB_OK);

    /* 验证日志索引递增 */
    mmdb_replica_info_t info;
    ASSERT_EQ(mmdb_replication_info(db_, &info), MMDB_OK);
    ASSERT_EQ(info.commit_index, 1u);
}

/* 测试 7: 日志追加（Follower 失败） */
TEST_F(ReplicationTest, FollowerNoAppend) {
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_FOLLOWER, kPeers), MMDB_OK);

    /* Follower 追加日志应失败 */
    const char* data = "test log entry";
    ASSERT_EQ(mmdb_replication_append_log(db_, data, strlen(data)), MMDB_ERR_INTERNAL);
}

/* 测试 8: 停止复制 */
TEST_F(ReplicationTest, StopReplication) {
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_LEADER, kPeers), MMDB_OK);

    /* 停止复制 */
    ASSERT_EQ(mmdb_replication_stop(db_), MMDB_OK);

    /* 重复停止应成功 */
    ASSERT_EQ(mmdb_replication_stop(db_), MMDB_OK);
}

/* 测试 9: 无效参数 */
TEST_F(ReplicationTest, InvalidParams) {
    /* NULL 参数 */
    ASSERT_EQ(mmdb_replication_init(nullptr, MMDB_REPLICA_LEADER, kPeers), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_LEADER, nullptr), MMDB_ERR_INVALID);

    mmdb_replica_info_t info;
    ASSERT_EQ(mmdb_replication_info(nullptr, &info), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_replication_info(db_, nullptr), MMDB_ERR_INVALID);
}

/* 测试 10: 重复初始化 */
TEST_F(ReplicationTest, DoubleInit) {
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_LEADER, kPeers), MMDB_OK);

    /* 重复初始化应失败 */
    ASSERT_EQ(mmdb_replication_init(db_, MMDB_REPLICA_LEADER, kPeers), MMDB_ERR_INTERNAL);
}
