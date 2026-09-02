# Gap#09 复制一致性系统实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现完整的复制一致性系统（线性一致性+故障切换+多源复制+冲突解决）

**Architecture:** ReplicationConsensus 统一入口，复用 raft.h/replication.h

**Tech Stack:** C 语言，CMake 构建，GTest 单元测试，pthread 线程

## Global Constraints

- 复用现有 `raft.h` (Raft共识算法)
- 复用现有 `replication.h` (主从复制)
- 遵循现有代码风格 (extern "C"、命名下划线分隔)
- 与 Gap#3 Executor Framework 集成

---

### Task 1: ReplicationConsensus 统一入口

**Files:**
- Create: `engineering/include/db/consistency/replication_consensus.h`
- Create: `engineering/src/db/consistency/replication_consensus.c`
- Create: `engineering/src/db/consistency/CMakeLists.txt`

**Interfaces:**
- Produces: replication_consensus_t

- [ ] **Step 1: 创建 replication_consensus.h**

```c
// engineering/include/db/consistency/replication_consensus.h
#ifndef DB_CONSISTENCY_H
#define DB_CONSISTENCY_H

#include "raft.h"
#include "replication.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct replication_consensus replication_consensus_t;

replication_consensus_t *rc_create(const RaftServerConfig_t *raft_cfg);
void rc_destroy(replication_consensus_t *rc);
int rc_start(replication_consensus_t *rc);
void rc_stop(replication_consensus_t *rc);

#ifdef __cplusplus
}
#endif

#endif /* DB_CONSISTENCY_H */
```

- [ ] **Step 2: 创建 replication_consensus.c**

```c
// engineering/src/db/consistency/replication_consensus.c
#include "db/consistency/replication_consensus.h"
#include <stdlib.h>

struct replication_consensus {
    RaftServer_t *raft;
};

replication_consensus_t *rc_create(const RaftServerConfig_t *raft_cfg) {
    replication_consensus_t *rc = calloc(1, sizeof(replication_consensus_t));
    if (!rc) return NULL;
    rc->raft = raft_server_create(raft_cfg);
    return rc;
}

void rc_destroy(replication_consensus_t *rc) {
    if (rc && rc->raft) {
        raft_server_destroy(rc->raft);
    }
    free(rc);
}

int rc_start(replication_consensus_t *rc) {
    return rc && rc->raft ? raft_server_start(rc->raft) : -1;
}

void rc_stop(replication_consensus_t *rc) {
    if (rc && rc->raft) {
        raft_server_stop(rc->raft);
    }
}
```

- [ ] **Step 3: 创建 CMakeLists.txt**

- [ ] **Step 4: 提交**

---

### Task 2: 线性一致性 (Linearizability)

**Files:**
- Modify: `engineering/include/db/consistency/replication_consensus.h`
- Create: `engineering/src/db/consistency/linearizability.c`

**Interfaces:**
- Consumes: raft.h, replication.h
- Produces: linearizability API

- [ ] **Step 1: 添加线性一致性 API**

```c
// 添加到 replication_consensus.h

typedef struct {
    bool strict_linearizability;
    int quorum_size;
    int ack_timeout_ms;
} linearizability_config_t;

int rc_linear_wait(replication_consensus_t *rc, uint64_t lsn,
                  int quorum, int timeout_ms);
bool rc_can_read(replication_consensus_t *rc);
```

- [ ] **Step 2: 实现 linearizability.c**

```c
// linearizability.c
#include "db/consistency/replication_consensus.h"

int rc_linear_wait(replication_consensus_t *rc, uint64_t lsn,
                  int quorum, int timeout_ms) {
    // 等待 WAL 复制到 quorum 节点
    // 使用 replication.h 的 repl_wait_sync
    return 0;
}

bool rc_can_read(replication_consensus_t *rc) {
    // 检查租约是否有效
    return rc && raft_is_leader(rc->raft);
}
```

- [ ] **Step 3: 提交**

---

### Task 3: 自动故障切换 (Auto Failover)

**Files:**
- Create: `engineering/include/db/consistency/failover.h`
- Create: `engineering/src/db/consistency/failover.c`

**Interfaces:**
- Consumes: raft.h
- Produces: failover API

- [ ] **Step 1: 创建 failover.h**

```c
typedef enum {
    FAILOVER_IDLE,
    FAILOVER_DETECTING,
    FAILOVER_ELECTION,
    FAILOVER_PROMOTING,
    FAILOVER_COMPLETE
} failover_state_t;

typedef void (*failover_callback_t)(int old_leader, int new_leader, void *arg);

int rc_failover_start(replication_consensus_t *rc, failover_callback_t cb, void *arg);
void rc_failover_stop(replication_consensus_t *rc);
int rc_get_failover_state(const replication_consensus_t *rc);
```

- [ ] **Step 2: 实现 failover.c**

- [ ] **Step 3: 提交**

---

### Task 4: 多源复制 (Multi-source)

**Files:**
- Create: `engineering/include/db/consistency/multisource.h`
- Create: `engineering/src/db/consistency/multisource.c`

**Interfaces:**
- Consumes: replication.h
- Produces: multisource API

- [ ] **Step 1: 创建 multisource.h**

```c
typedef struct {
    int node_id;
    char host[64];
    int port;
    repl_state_t state;
    uint64_t last_lsn;
} replica_node_t;

int rc_add_source(replication_consensus_t *rc, const replica_node_t *node);
int rc_remove_source(replication_consensus_t *rc, int node_id);
int rc_get_sources(replication_consensus_t *rc, replica_node_t *nodes, int *count);
int rc_sync_all(replication_consensus_t *rc);
```

- [ ] **Step 2: 实现 multisource.c**

- [ ] **Step 3: 提交**

---

### Task 5: 冲突解决 (Conflict Resolution)

**Files:**
- Create: `engineering/include/db/consistency/conflict_resolution.h`
- Create: `engineering/src/db/consistency/conflict_resolution.c`

**Interfaces:**
- Produces: Vector Clock + CRDT API

- [ ] **Step 1: 创建 conflict_resolution.h**

```c
#define MAX_NODES 64

// Vector Clock
typedef struct {
    uint64_t clocks[MAX_NODES];
    int node_count;
    int local_node_id;
} vector_clock_t;

void vc_inc(vector_clock_t *vc);
void vc_merge(vector_clock_t *vc, const vector_clock_t *other);
int vc_compare(const vector_clock_t *a, const vector_clock_t *b);

// CRDT
typedef enum {
    CRDT_GCOUNTER,
    CRDT_PNCOUNTER,
    CRDT_LWWREG
} crdt_type_t;

typedef struct crdt crdt_t;

crdt_t *crdt_create(crdt_type_t type);
void crdt_destroy(crdt_t *crdt);
int crdt_merge(crdt_t *a, const crdt_t *b);
```

- [ ] **Step 2: 实现 conflict_resolution.c**

- [ ] **Step 3: 提交**

---

### Task 6: 单元测试

**Files:**
- Create: `engineering/test/db/consistency/consistency_test.cpp`

- [ ] **Step 1: 创建测试**

```cpp
#include <gtest/gtest.h>
#include "db/consistency/replication_consensus.h"

class ConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        RaftServerConfig_t cfg = {.node_id = 1, .cluster_size = 3,
            .heartbeat_interval_ms = 150,
            .election_timeout_min_ms = 1000,
            .election_timeout_max_ms = 2000};
        rc = rc_create(&cfg);
    }
    void TearDown() override {
        if (rc) rc_destroy(rc);
    }
    replication_consensus_t *rc = nullptr;
};

TEST_F(ConsistencyTest, CreateDestroy) {
    ASSERT_NE(rc, nullptr);
}

TEST_F(ConsistencyTest, Linearizability) {
    EXPECT_TRUE(rc_can_read(rc));
}

TEST_F(ConsistencyTest, FailoverState) {
    EXPECT_EQ(rc_get_failover_state(rc), FAILOVER_IDLE);
}

TEST_F(ConsistencyTest, VectorClock) {
    vector_clock_t vc = {.local_node_id = 1, .node_count = 1};
    vc_inc(&vc);
    EXPECT_EQ(vc.clocks[1], 1);
}
```

- [ ] **Step 2: 提交**

---

## 任务依赖关系

```
Task 1: ReplicationConsensus  ← 基础
Task 2: 线性一致性          ← 依赖 Task 1
Task 3: 自动故障切换        ← 依赖 Task 2
Task 4: 多源复制           ← 依赖 Task 2
Task 5: 冲突解决           ← 无依赖
Task 6: 单元测试          ← 依赖 Task 1-5
```

## 成功标准

- [ ] Task 1: ReplicationConsensus 统一入口
- [ ] Task 2: 线性一致性实现
- [ ] Task 3: 自动故障切换实现
- [ ] Task 4: 多源复制实现
- [ ] Task 5: Vector Clock + CRDT 实现
- [ ] Task 6: 单元测试全部通过
