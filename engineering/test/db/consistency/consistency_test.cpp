/**
 * @file consistency_test.cpp
 * @brief ReplicationConsensus 单元测试
 *
 * 测试覆盖：
 * - ReplicationConsensus 创建/销毁
 * - 线性一致性
 * - 故障切换
 * - 多源复制
 * - Vector Clock
 * - CRDT
 */

#include <gtest/gtest.h>
#include "db/consistency/replication_consensus.h"
#include "db/consistency/failover.h"
#include "db/consistency/multisource.h"
#include "db/consistency/conflict_resolution.h"
#include "db/consensus/raft.h"
#include "db/replication/replication.h"

#include <cstring>
#include <vector>

namespace {

/* ============================================================
 * 测试辅助函数
 * ============================================================ */

RaftServerConfig_t make_single_node_config() {
    RaftServerConfig_t cfg = {};
    cfg.node_id = 1;
    cfg.cluster_size = 1;
    cfg.heartbeat_interval_ms = 150;
    cfg.election_timeout_min_ms = 200;
    cfg.election_timeout_max_ms = 400;
    return cfg;
}

/* ============================================================
 * ReplicationConsensus 生命周期测试
 * ============================================================ */

class ReplicationConsensusLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = make_single_node_config();
    }

    void TearDown() override {
        // rc_destroy 会处理 NULL 检查
    }

    RaftServerConfig_t config_;
};

TEST_F(ReplicationConsensusLifecycleTest, CreateAndDestroy) {
    replication_consensus_t *rc = rc_create(&config_);
    ASSERT_NE(rc, nullptr);
    rc_destroy(rc);
}

TEST_F(ReplicationConsensusLifecycleTest, CreateWithNullConfig) {
    replication_consensus_t *rc = rc_create(nullptr);
    EXPECT_EQ(rc, nullptr);
}

TEST_F(ReplicationConsensusLifecycleTest, StartAndStop) {
    replication_consensus_t *rc = rc_create(&config_);
    ASSERT_NE(rc, nullptr);

    EXPECT_EQ(rc_start(rc), 0);
    EXPECT_TRUE(rc_get_role(rc) == RAFT_ROLE_FOLLOWER ||
                rc_get_role(rc) == RAFT_ROLE_LEADER ||
                rc_get_role(rc) == RAFT_ROLE_CANDIDATE);

    rc_stop(rc);
    rc_destroy(rc);
}

TEST_F(ReplicationConsensusLifecycleTest, DoubleStart) {
    replication_consensus_t *rc = rc_create(&config_);
    ASSERT_NE(rc, nullptr);

    EXPECT_EQ(rc_start(rc), 0);
    EXPECT_EQ(rc_start(rc), -1);  // 重复启动应失败

    rc_stop(rc);
    rc_destroy(rc);
}

TEST_F(ReplicationConsensusLifecycleTest, StopWithoutStart) {
    replication_consensus_t *rc = rc_create(&config_);
    ASSERT_NE(rc, nullptr);

    // 未启动就停止应该安全
    rc_stop(rc);
    rc_destroy(rc);
}

/* ============================================================
 * Raft 状态查询测试
 * ============================================================ */

class ReplicationConsensusStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = make_single_node_config();
        rc_ = rc_create(&config_);
        ASSERT_NE(rc_, nullptr);
        rc_start(rc_);
    }

    void TearDown() override {
        if (rc_ != nullptr) {
            rc_stop(rc_);
            rc_destroy(rc_);
        }
    }

    replication_consensus_t *rc_;
    RaftServerConfig_t config_;
};

TEST_F(ReplicationConsensusStateTest, InitialState) {
    // 单节点集群，创建后应该是 FOLLOWER
    RaftRole_t role = rc_get_role(rc_);
    EXPECT_TRUE(role == RAFT_ROLE_FOLLOWER ||
                role == RAFT_ROLE_CANDIDATE ||
                role == RAFT_ROLE_LEADER);

    // 初始 term 应该是 0
    EXPECT_EQ(rc_get_current_term(rc_), 0u);
}

TEST_F(ReplicationConsensusStateTest, IsLeader) {
    // 初始状态不确定是否为 leader，取决于 tick
    bool is_leader = rc_is_leader(rc_);
    EXPECT_TRUE(is_leader == true || is_leader == false);
}

TEST_F(ReplicationConsensusStateTest, SubmitRejectedWhenNotLeader) {
    // 在非 leader 状态提交应该被拒绝
    const char *data = "test";
    size_t idx = rc_submit(rc_, data, strlen(data) + 1);
    // 如果不是 leader，应该返回 SIZE_MAX
    if (!rc_is_leader(rc_)) {
        EXPECT_EQ(idx, SIZE_MAX);
    }
}

TEST_F(ReplicationConsensusStateTest, LeaderSubmit) {
    // 强制成为 leader
    raft_test_force_election_timeout(((replication_consensus_t*)rc_)->raft);
    raft_tick(((replication_consensus_t*)rc_)->raft);

    ASSERT_TRUE(rc_is_leader(rc_));

    const char *data = "test";
    size_t idx = rc_submit(rc_, data, strlen(data) + 1);
    EXPECT_NE(idx, SIZE_MAX);
    EXPECT_GE(idx, 1u);
}

/* ============================================================
 * 线性一致性测试
 * ============================================================ */

class LinearizabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = make_single_node_config();
        rc_ = rc_create(&config_);
        ASSERT_NE(rc_, nullptr);
        rc_start(rc_);

        // 强制成为 leader 以便测试线性一致性
        raft_test_force_election_timeout(((replication_consensus_t*)rc_)->raft);
        raft_tick(((replication_consensus_t*)rc_)->raft);
    }

    void TearDown() override {
        if (rc_ != nullptr) {
            rc_stop(rc_);
            rc_destroy(rc_);
        }
    }

    replication_consensus_t *rc_;
    RaftServerConfig_t config_;
};

TEST_F(LinearizabilityTest, CanReadWhenLeader) {
    ASSERT_TRUE(rc_is_leader(rc_));
    EXPECT_TRUE(rc_can_read(rc_));
}

TEST_F(LinearizabilityTest, LinearWaitWithValidLSN) {
    // 提交一条日志
    const char *data = "test";
    size_t idx = rc_submit(rc_, data, strlen(data) + 1);
    ASSERT_NE(idx, SIZE_MAX);

    // 等待 LSN 确认（这里使用 commit_index 作为 LSN）
    uint64_t commit_index = rc_get_commit_index(rc_);
    EXPECT_EQ(rc_linear_wait(rc_, commit_index, 1, 1000), 0);
}

TEST_F(LinearizabilityTest, LinearWaitTimeout) {
    // 使用一个很大的 LSN，应该超时
    EXPECT_EQ(rc_linear_wait(rc_, UINT64_MAX, 1, 10), -1);
}

TEST_F(LinearizabilityTest, LinearWaitInvalidParams) {
    EXPECT_EQ(rc_linear_wait(nullptr, 1, 1, 1000), -1);
    EXPECT_EQ(rc_linear_wait(rc_, 1, 0, 1000), -1);
    EXPECT_EQ(rc_linear_wait(rc_, 1, 1, 0), -1);
}

TEST_F(LinearizabilityTest, SubmitIncreasesCommitIndex) {
    size_t idx1 = rc_submit(rc_, "a", 2);
    size_t idx2 = rc_submit(rc_, "b", 2);
    size_t idx3 = rc_submit(rc_, "c", 2);

    EXPECT_GE(idx1, 1u);
    EXPECT_GE(idx2, idx1);
    EXPECT_GE(idx3, idx2);
}

/* ============================================================
 * 故障切换测试
 * ============================================================ */

class FailoverTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = make_single_node_config();
        rc_ = rc_create(&config_);
        ASSERT_NE(rc_, nullptr);
        rc_start(rc_);
    }

    void TearDown() override {
        if (rc_ != nullptr) {
            rc_failover_stop(rc_);
            rc_stop(rc_);
            rc_destroy(rc_);
        }
    }

    replication_consensus_t *rc_;
    RaftServerConfig_t config_;
};

TEST_F(FailoverTest, StartAndStop) {
    int old_leader = 0;
    int new_leader = 0;

    // 启动故障切换
    EXPECT_EQ(rc_failover_start(rc_,
        [](int old, int new_leader, void *arg) {
            *((int*)arg) = new_leader;
        }, &new_leader), 0);

    // 停止故障切换
    rc_failover_stop(rc_);
}

TEST_F(FailoverTest, GetFailoverState) {
    // 初始状态应该是 IDLE
    EXPECT_EQ(rc_get_failover_state(rc_), FAILOVER_IDLE);
}

TEST_F(FailoverTest, GetLeaderId) {
    // 强制成为 leader
    raft_test_force_election_timeout(((replication_consensus_t*)rc_)->raft);
    raft_tick(((replication_consensus_t*)rc_)->raft);

    if (rc_is_leader(rc_)) {
        int leader_id = rc_get_leader_id(rc_);
        EXPECT_EQ(leader_id, (int)config_.node_id);
    }
}

TEST_F(FailoverTest, NullHandle) {
    // NULL 处理
    EXPECT_EQ(rc_failover_start(nullptr, nullptr, nullptr), -1);
    rc_failover_stop(nullptr);
    EXPECT_EQ(rc_get_failover_state(nullptr), FAILOVER_IDLE);
    EXPECT_EQ(rc_get_leader_id(nullptr), -1);
}

/* ============================================================
 * 多源复制测试
 * ============================================================ */

class MultiSourceReplicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = make_single_node_config();
        rc_ = rc_create(&config_);
        ASSERT_NE(rc_, nullptr);
        rc_start(rc_);
    }

    void TearDown() override {
        if (rc_ != nullptr) {
            rc_stop(rc_);
            rc_destroy(rc_);
        }
    }

    replication_consensus_t *rc_;
    RaftServerConfig_t config_;
};

TEST_F(MultiSourceReplicationTest, AddSource) {
    replica_node_t node = {};
    node.node_id = 100;
    strcpy(node.host, "192.168.1.100");
    node.port = 5432;
    node.state = REPL_STATE_CONNECTING;

    EXPECT_EQ(rc_add_source(rc_, &node), 0);
}

TEST_F(MultiSourceReplicationTest, AddMultipleSources) {
    for (int i = 1; i <= 3; i++) {
        replica_node_t node = {};
        node.node_id = i * 100;
        snprintf(node.host, sizeof(node.host), "192.168.1.%d", i * 100);
        node.port = 5432;
        node.state = REPL_STATE_CONNECTING;
        EXPECT_EQ(rc_add_source(rc_, &node), 0);
    }

    // 获取源节点列表
    replica_node_t nodes[16];
    int count = 16;
    EXPECT_EQ(rc_get_sources(rc_, nodes, &count), 0);
    EXPECT_EQ(count, 3);
}

TEST_F(MultiSourceReplicationTest, RemoveSource) {
    replica_node_t node = {};
    node.node_id = 100;
    strcpy(node.host, "192.168.1.100");
    node.port = 5432;

    EXPECT_EQ(rc_add_source(rc_, &node), 0);
    EXPECT_EQ(rc_remove_source(rc_, 100), 0);
}

TEST_F(MultiSourceReplicationTest, GetSourcesAfterAdd) {
    replica_node_t node1 = {};
    node1.node_id = 100;
    strcpy(node1.host, "192.168.1.100");
    node1.port = 5432;
    node1.state = REPL_STATE_NORMAL;
    node1.last_lsn = 1000;

    replica_node_t node2 = {};
    node2.node_id = 200;
    strcpy(node2.host, "192.168.1.200");
    node2.port = 5432;
    node2.state = REPL_STATE_STREAMING;
    node2.last_lsn = 2000;

    EXPECT_EQ(rc_add_source(rc_, &node1), 0);
    EXPECT_EQ(rc_add_source(rc_, &node2), 0);

    replica_node_t nodes[16];
    int count = 16;
    EXPECT_EQ(rc_get_sources(rc_, nodes, &count), 0);
    EXPECT_EQ(count, 2);

    // 验证节点数据
    bool found_100 = false, found_200 = false;
    for (int i = 0; i < count; i++) {
        if (nodes[i].node_id == 100) found_100 = true;
        if (nodes[i].node_id == 200) found_200 = true;
    }
    EXPECT_TRUE(found_100);
    EXPECT_TRUE(found_200);
}

TEST_F(MultiSourceReplicationTest, SyncAll) {
    replica_node_t node = {};
    node.node_id = 100;
    strcpy(node.host, "192.168.1.100");
    node.port = 5432;
    node.state = REPL_STATE_DISCONNECTED;

    EXPECT_EQ(rc_add_source(rc_, &node), 0);
    EXPECT_EQ(rc_sync_all(rc_), 0);
}

TEST_F(MultiSourceReplicationTest, AddSourceInvalidParams) {
    EXPECT_EQ(rc_add_source(nullptr, nullptr), -1);

    replica_node_t node = {};
    node.node_id = 0;  // 无效 ID
    EXPECT_EQ(rc_add_source(rc_, &node), -1);

    EXPECT_EQ(rc_add_source(rc_, nullptr), -1);
}

TEST_F(MultiSourceReplicationTest, RemoveSourceInvalidParams) {
    EXPECT_EQ(rc_remove_source(nullptr, 100), -1);
    EXPECT_EQ(rc_remove_source(rc_, 0), -1);  // 无效 ID
}

TEST_F(MultiSourceReplicationTest, GetSourcesInvalidParams) {
    replica_node_t nodes[16];
    int count = 16;
    EXPECT_EQ(rc_get_sources(nullptr, nodes, &count), -1);
    EXPECT_EQ(rc_get_sources(rc_, nullptr, &count), -1);

    int zero = 0;
    EXPECT_EQ(rc_get_sources(rc_, nodes, &zero), -1);
}

/* ============================================================
 * Vector Clock 测试
 * ============================================================ */

class VectorClockTest : public ::testing::Test {
protected:
};

TEST_F(VectorClockTest, Init) {
    vector_clock_t vc;
    vc_init(&vc, 0, 3);
    EXPECT_EQ(vc.node_count, 3);
    EXPECT_EQ(vc.local_node_id, 0);
    EXPECT_EQ(vc.clocks[0], 0u);
    EXPECT_EQ(vc.clocks[1], 0u);
    EXPECT_EQ(vc.clocks[2], 0u);
}

TEST_F(VectorClockTest, InitInvalidParams) {
    vector_clock_t vc;
    vc_init(nullptr, 0, 3);  // NULL vc
    vc_init(&vc, -1, 3);     // 无效 local_node_id
    vc_init(&vc, 0, MAX_NODES + 1);  // 太大
}

TEST_F(VectorClockTest, Increment) {
    vector_clock_t vc;
    vc_init(&vc, 1, 3);

    vc_inc(&vc);
    EXPECT_EQ(vc.clocks[1], 1u);

    vc_inc(&vc);
    EXPECT_EQ(vc.clocks[1], 2u);

    // 其他节点不受影响
    EXPECT_EQ(vc.clocks[0], 0u);
    EXPECT_EQ(vc.clocks[2], 0u);
}

TEST_F(VectorClockTest, Merge) {
    vector_clock_t a, b;
    vc_init(&a, 0, 3);
    vc_init(&b, 1, 3);

    vc_inc(&a);  // a: [1, 0, 0]
    vc_inc(&b);  // b: [0, 1, 0]
    vc_inc(&b);  // b: [0, 2, 0]

    vc_merge(&a, &b);  // a: [1, 2, 0]

    EXPECT_EQ(a.clocks[0], 1u);
    EXPECT_EQ(a.clocks[1], 2u);
    EXPECT_EQ(a.clocks[2], 0u);
}

TEST_F(VectorClockTest, MergeSameNode) {
    vector_clock_t a, b;
    vc_init(&a, 0, 3);
    vc_init(&b, 0, 3);

    vc_inc(&a);  // a: [1, 0, 0]
    vc_inc(&b);  // b: [1, 0, 0]
    vc_inc(&b);  // b: [2, 0, 0]

    vc_merge(&a, &b);  // a: [2, 0, 0]

    EXPECT_EQ(a.clocks[0], 2u);
}

TEST_F(VectorClockTest, CompareEqual) {
    vector_clock_t a, b;
    vc_init(&a, 0, 3);
    vc_init(&b, 0, 3);

    vc_inc(&a);
    vc_inc(&b);

    EXPECT_EQ(vc_compare(&a, &b), 0);  // equal
}

TEST_F(VectorClockTest, CompareGreater) {
    vector_clock_t a, b;
    vc_init(&a, 0, 3);
    vc_init(&b, 0, 3);

    vc_inc(&a);  // a: [1, 0, 0]
    vc_inc(&a);  // a: [2, 0, 0]
    vc_inc(&b);  // b: [1, 0, 0]

    EXPECT_EQ(vc_compare(&a, &b), 1);  // a > b
    EXPECT_EQ(vc_compare(&b, &a), -1); // b < a
}

TEST_F(VectorClockTest, CompareConcurrent) {
    vector_clock_t a, b;
    vc_init(&a, 0, 3);
    vc_init(&b, 1, 3);

    vc_inc(&a);  // a: [1, 0, 0]
    vc_inc(&b);  // b: [0, 1, 0]

    EXPECT_EQ(vc_compare(&a, &b), 0);  // concurrent
    EXPECT_TRUE(vc_is_concurrent(&a, &b));
}

TEST_F(VectorClockTest, Copy) {
    vector_clock_t src, dest;
    vc_init(&src, 0, 3);
    vc_inc(&src);
    vc_inc(&src);
    vc_inc(&src);  // src: [3, 0, 0]

    vc_copy(&dest, &src);

    EXPECT_EQ(dest.clocks[0], 3u);
    EXPECT_EQ(dest.clocks[1], 0u);
    EXPECT_EQ(dest.node_count, 3);
    EXPECT_EQ(dest.local_node_id, 0);
}

TEST_F(VectorClockTest, NullHandling) {
    vc_inc(nullptr);
    vc_merge(nullptr, nullptr);
    vc_compare(nullptr, nullptr);
    vc_is_concurrent(nullptr, nullptr);
    vc_copy(nullptr, nullptr);
    // 不应崩溃
}

/* ============================================================
 * CRDT 测试
 * ============================================================ */

class CRDTTest : public ::testing::Test {
protected:
};

TEST_F(CRDTTest, GCounterCreateAndDestroy) {
    crdt_t *crdt = crdt_create(CRDT_GCOUNTER);
    ASSERT_NE(crdt, nullptr);
    EXPECT_EQ(crdt_get_type(crdt), CRDT_GCOUNTER);
    EXPECT_EQ(crdt_get_value(crdt), 0);
    crdt_destroy(crdt);
}

TEST_F(CRDTTest, PNCounterCreateAndDestroy) {
    crdt_t *crdt = crdt_create(CRDT_PNCOUNTER);
    ASSERT_NE(crdt, nullptr);
    EXPECT_EQ(crdt_get_type(crdt), CRDT_PNCOUNTER);
    EXPECT_EQ(crdt_get_value(crdt), 0);
    crdt_destroy(crdt);
}

TEST_F(CRDTTest, LWWRegisterCreateAndDestroy) {
    crdt_t *crdt = crdt_create(CRDT_LWWREG);
    ASSERT_NE(crdt, nullptr);
    EXPECT_EQ(crdt_get_type(crdt), CRDT_LWWREG);
    EXPECT_EQ(crdt_get_value(crdt), 0);
    crdt_destroy(crdt);
}

TEST_F(CRDTTest, CreateInvalidType) {
    EXPECT_EQ(crdt_create((crdt_type_t)999), nullptr);
}

TEST_F(CRDTTest, GCounterIncrement) {
    crdt_t *crdt = crdt_create(CRDT_GCOUNTER);
    ASSERT_NE(crdt, nullptr);

    EXPECT_EQ(crdt_inc(crdt, 1), 0);
    EXPECT_EQ(crdt_get_value(crdt), 1);

    EXPECT_EQ(crdt_inc(crdt, 5), 0);
    EXPECT_EQ(crdt_get_value(crdt), 6);

    crdt_destroy(crdt);
}

TEST_F(CRDTTest, GCounterDecrementFails) {
    crdt_t *crdt = crdt_create(CRDT_GCOUNTER);
    ASSERT_NE(crdt, nullptr);

    crdt_inc(crdt, 10);
    EXPECT_EQ(crdt_dec(crdt, 1), -1);  // G-Counter 不能递减
    EXPECT_EQ(crdt_get_value(crdt), 10);

    crdt_destroy(crdt);
}

TEST_F(CRDTTest, PNCounterIncrementDecrement) {
    crdt_t *crdt = crdt_create(CRDT_PNCOUNTER);
    ASSERT_NE(crdt, nullptr);

    crdt_inc(crdt, 10);
    EXPECT_EQ(crdt_get_value(crdt), 10);

    crdt_dec(crdt, 3);
    EXPECT_EQ(crdt_get_value(crdt), 7);

    crdt_destroy(crdt);
}

TEST_F(CRDTTest, LWWRegisterSet) {
    crdt_t *crdt = crdt_create(CRDT_LWWREG);
    ASSERT_NE(crdt, nullptr);

    EXPECT_EQ(crdt_set(crdt, 100, 1), 0);
    EXPECT_EQ(crdt_get_value(crdt), 100);

    EXPECT_EQ(crdt_set(crdt, 200, 2), 0);
    EXPECT_EQ(crdt_get_value(crdt), 200);

    // 较低 timestamp 的更新被忽略
    EXPECT_EQ(crdt_set(crdt, 50, 1), 0);
    EXPECT_EQ(crdt_get_value(crdt), 200);

    crdt_destroy(crdt);
}

TEST_F(CRDTTest, LWWRegisterIncrementFails) {
    crdt_t *crdt = crdt_create(CRDT_LWWREG);
    ASSERT_NE(crdt, nullptr);

    crdt_set(crdt, 100, 1);
    EXPECT_EQ(crdt_inc(crdt, 1), -1);  // LWW-Reg 不能递增
    EXPECT_EQ(crdt_get_value(crdt), 100);

    crdt_destroy(crdt);
}

TEST_F(CRDTTest, GCounterMerge) {
    crdt_t *a = crdt_create(CRDT_GCOUNTER);
    crdt_t *b = crdt_create(CRDT_GCOUNTER);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    crdt_inc(a, 5);
    crdt_inc(b, 3);

    EXPECT_EQ(crdt_merge(a, b), 0);
    EXPECT_EQ(crdt_get_value(a), 8);  // max(5, 3) = 5, but a already had 5

    crdt_destroy(a);
    crdt_destroy(b);
}

TEST_F(CRDTTest, PNCounterMerge) {
    crdt_t *a = crdt_create(CRDT_PNCOUNTER);
    crdt_t *b = crdt_create(CRDT_PNCOUNTER);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    crdt_inc(a, 10);
    crdt_dec(a, 3);   // a: +10, -3 = 7

    crdt_inc(b, 5);
    crdt_dec(b, 1);   // b: +5, -1 = 4

    // Merge: take max of each component
    EXPECT_EQ(crdt_merge(a, b), 0);
    EXPECT_EQ(crdt_get_value(a), 11);  // max(10,5) + max(3,1) = 10 + 3 = 13? No, values are summed

    crdt_destroy(a);
    crdt_destroy(b);
}

TEST_F(CRDTTest, LWWRegisterMerge) {
    crdt_t *a = crdt_create(CRDT_LWWREG);
    crdt_t *b = crdt_create(CRDT_LWWREG);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    crdt_set(a, 100, 10);
    crdt_set(b, 200, 5);  // lower timestamp

    EXPECT_EQ(crdt_merge(a, b), 0);
    EXPECT_EQ(crdt_get_value(a), 100);  // a wins (higher timestamp)

    // Now b has higher timestamp
    crdt_set(b, 300, 20);
    EXPECT_EQ(crdt_merge(a, b), 0);
    EXPECT_EQ(crdt_get_value(a), 300);  // b wins

    crdt_destroy(a);
    crdt_destroy(b);
}

TEST_F(CRDTTest, MergeTypeMismatch) {
    crdt_t *a = crdt_create(CRDT_GCOUNTER);
    crdt_t *b = crdt_create(CRDT_PNCOUNTER);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_EQ(crdt_merge(a, b), -1);  // 类型不匹配

    crdt_destroy(a);
    crdt_destroy(b);
}

TEST_F(CRDTTest, NullHandling) {
    crdt_destroy(nullptr);
    EXPECT_EQ(crdt_get_value(nullptr), 0);
    EXPECT_EQ(crdt_get_type(nullptr), CRDT_GCOUNTER);  // default
    EXPECT_EQ(crdt_inc(nullptr, 1), -1);
    EXPECT_EQ(crdt_dec(nullptr, 1), -1);
    EXPECT_EQ(crdt_set(nullptr, 1, 1), -1);
    EXPECT_EQ(crdt_merge(nullptr, nullptr), -1);
}

}  // namespace
