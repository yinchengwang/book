# Phase11 Design —— Engineering Raft Module

## 1. 模块位置

```
engineering/
├── include/db/consensus/
│   └── raft.h          # 公共 ABI
└── src/db/consensus/
    ├── CMakeLists.txt
    └── raft.c          # 实现
```

## 2. raft.h 公共 API（最小集）

```c
// 角色
typedef enum RaftRole {
    RAFT_ROLE_FOLLOWER = 0,
    RAFT_ROLE_CANDIDATE,
    RAFT_ROLE_LEADER,
};

// 服务器
typedef struct RaftServer RaftServer;
typedef struct RaftServerConfig {
    uint64_t node_id;                   // 唯一节点 id
    uint32_t cluster_size;              // 总节点数
    uint32_t heartbeat_interval_ms;     // 心跳间隔
    uint32_t election_timeout_min_ms;   // 选举超时下限
    uint32_t election_timeout_max_ms;   // 选举超时上限
} RaftServerConfig;

// 生命周期
RaftServer *raft_server_create(const RaftServerConfig *cfg);
void raft_server_destroy(RaftServer *srv);
int raft_server_start(RaftServer *srv);
int raft_server_stop(RaftServer *srv);

// 查询
RaftRole raft_get_role(const RaftServer *srv);
uint64_t raft_get_current_term(const RaftServer *srv);
uint64_t raft_get_leader_id(const RaftServer *srv);
bool raft_is_leader(const RaftServer *srv);

// 时间推进（测试驱动）
void raft_tick(RaftServer *srv);

// 日志
int raft_submit(RaftServer *srv, const void *data, size_t size);
int raft_get_committed_count(const RaftServer *srv);
size_t raft_get_committed(const RaftServer *srv, size_t idx, void *buf, size_t cap);
```

## 3. raft.c 实现要点

**核心数据结构**：
```c
struct RaftServer {
    RaftServerConfig config;
    RaftRole role;
    uint64_t current_term;
    uint64_t voted_for;        // UINT64_MAX 表示未投票
    uint64_t commit_index;
    uint64_t last_applied;
    uint64_t leader_id;          // UINT64_MAX 表示未知
    
    // 状态机（最小）
    RaftEntry *log;
    size_t log_size;
    size_t log_capacity;
    
    // 时序
    uint64_t election_deadline_ms;
    uint64_t last_heartbeat_ms;
    uint32_t votes_received;
    
    pthread_mutex_t lock;
};
```

**raft_tick()**：
1. 检查 `now - last_heartbeat > election_timeout`
2. 若超时：role → CANDIDATE，`current_term++`，投自己一票
3. Follower: 收到 leader heartbeat 重置 deadline
4. Leader: 每次 tick 不做事（实际应定期心跳；smoke 测试不需要）

**raft_submit()**：
1. 仅 leader 接受（其他返回 -1）
2. append log，`commit_index` 由后续 tick 推进（smoke 测试足够）

## 4. raft_test.cpp Smoke

```cpp
TEST(RaftSmoke, CreateAndDestroy) {
    RaftServerConfig cfg = {.node_id = 1, .cluster_size = 3};
    RaftServer *s = raft_server_create(&cfg);
    EXPECT_NE(s, nullptr);
    raft_server_destroy(s);
}

TEST(RaftSmoke, InitialRoleFollower) {
    RaftServerConfig cfg = {.node_id = 1, .cluster_size = 3};
    RaftServer *s = raft_server_create(&cfg);
    EXPECT_EQ(raft_get_role(s), RAFT_ROLE_FOLLOWER);
    raft_server_destroy(s);
}

TEST(RaftSmoke, TickTriggersElection) {
    RaftServerConfig cfg = {.node_id = 1, .cluster_size = 3,
                            .election_timeout_min_ms = 100,
                            .election_timeout_max_ms = 100};
    RaftServer *s = raft_server_create(&cfg);
    raft_server_start(s);
    // 多次 tick 直到选举超时
    for (int i = 0; i < 1000 && raft_get_role(s) != RAFT_ROLE_CANDIDATE; i++) {
        raft_tick(s);
        usleep(200);  // 模拟时间推进
    }
    EXPECT_EQ(raft_get_role(s), RAFT_ROLE_CANDIDATE);
    raft_server_destroy(s);
}
```

## 5. CMakeLists.txt 注册

```cmake
# engineering/src/db/consensus/CMakeLists.txt
add_library(raft_consensus STATIC raft.c)
target_include_directories(raft_consensus PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/db/core
)
target_link_libraries(raft_consensus PUBLIC db_core)
set_target_properties(raft_consensus PROPERTIES FOLDER "Libraries")
```

```cmake
# engineering/src/db/CMakeLists.txt (添加)
add_subdirectory(consensus)
```

```cmake
# engineering/test/db/consensus/CMakeLists.txt
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(raft_test ${TEST_SOURCES})
target_link_libraries(raft_test PRIVATE raft_consensus gtest gtest_main)
include(GoogleTest)
gtest_discover_tests(raft_test)
```

```cmake
# engineering/test/CMakeLists.txt (添加)
add_subdirectory(db/consensus)
```

## 6. 验证 V1-V3

| V | 命令 | 期望 |
|---|---|---|
| V1 | `cmake --build engineering/build` | raft_consensus 库生成 |
| V2 | `find engineering/build -name raft_test.exe` | 存在 |
| V3 | `engineering/build/test/db/consensus/raft_test.exe` | 3 个测试通过 |

## 7. 不做

- ❌ 集群网络通信（loop 仅占位）
- ❌ 持久化（WAL 集成）
- ❌ Joint Consensus
- ❌ Snapshots
- ❌ 学习层副本（已决策：phase10+）
