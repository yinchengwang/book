# P1 — Design（Raft Multi-Node + Log Replication）

## 1. in-process transport

```c
// engineering/include/db/consensus/raft_transport.h

typedef struct RaftMessage {
    uint64_t from_node;
    uint64_t term;
    RaftMessageType type;
    union {
        struct { uint64_t candidate_id; uint64_t last_log_idx; uint64_t last_log_term; } vote_request;
        struct { bool vote_granted; } vote_response;
        struct { uint64_t leader_id; uint64_t prev_log_idx; uint64_t prev_log_term;
                 RaftLogEntry_t *entries; size_t entry_count;
                 uint64_t leader_commit; } append_request;
        struct { bool success; uint64_t match_idx; } append_response;
    } payload;
} RaftMessage_t;

typedef void (*RaftTransportSendFn)(void *user, uint64_t to_node, const RaftMessage_t *msg);

typedef struct RaftTransport {
    RaftTransportSendFn send;
    void *user;  // 回调 context
} RaftTransport_t;
```

## 2. Cluster API

```c
// engineering/include/db/consensus/raft_cluster.h

typedef struct RaftCluster RaftCluster_t;

RaftCluster_t *raft_cluster_create(uint32_t size);
void raft_cluster_destroy(RaftCluster_t *cl);
RaftServer_t *raft_cluster_get_node(RaftCluster_t *cl, uint64_t node_id);

/* 推进所有节点 tick + 投递消息 */
void raft_cluster_tick(RaftCluster_t *cl, uint64_t delta_ms);

/* 测试钩子 */
void raft_cluster_dump_logs(RaftCluster_t *cl);
```

## 3. AppendEntries 状态机

每收到 AppendEntries：
1. term 检查：< current_term → reject
2. prevLog 检查：log[prevLog_idx].term != prevLog_term → reject + retry
3. 删 conflict entries（从 prevLog_idx+1 起）
4. append 新 entries
5. 更新 commit_index（若 leader_commit > current）

## 4. test 场景

```cpp
TEST(RaftCluster, LeaderElects) {
    auto cluster = raft_cluster_create(3);
    raft_cluster_tick(cluster, 1000);  // 触发选举
    ASSERT_EQ(raft_cluster_count_leaders(cluster), 1);
}

TEST(RaftCluster, LogReplication) {
    auto cluster = raft_cluster_create(3);
    raft_cluster_tick(cluster, 1000);  // 选举
    auto *leader = raft_cluster_get_leader(cluster);
    raft_submit(leader, "x", 2);
    raft_cluster_tick(cluster, 500);   // 复制 + commit
    for (auto *node : nodes) {
        EXPECT_EQ(raft_get_commit_index(node), 1);
    }
}
```

## 5. 文件结构

```
engineering/src/db/consensus/
├── raft.c                  (phase11, 单节点)
├── raft_transport.c        (P1 新)
├── raft_vote.c             (P1 新) — RequestVote RPC
├── raft_append.c           (P1 新) — AppendEntries RPC
├── raft_cluster.c          (P1 新) — 集群管理
└── raft_log_rep_test.cpp   (P1 新) — 测试
```

## 6. 验证 V1-V3

| V | 命令 |
|---|---|
| V1 | `cmake --build engineering/build` |
| V2 | `raft_log_rep_test.exe --gtest_list_tests` |
| V3 | `ctest -R RaftCluster` |

## 7. 不做

- ❌ 持久化
- ❌ 真实网络传输
- ❌ Snapshot
