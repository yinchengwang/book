# Gap#09 复制一致性系统设计

> **日期:** 2026-09-03
> **状态:** 待批准

## 1. 目标

实现完整的复制一致性系统，支持：
- 强一致性保证 (Linearizability)
- 自动故障切换 (Auto Failover)
- 多源复制 (Multi-source Replication)
- 冲突解决 (Conflict Resolution)

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                   ReplicationConsensus                        │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐  │
│  │  Linearizability│  │ Auto Failover │  │ Multi-source  │  │
│  │  (强一致性)    │  │ (自动切换)    │  │ (多源复制)    │  │
│  └────────────────┘  └────────────────┘  └────────────────┘  │
│  ┌────────────────┐                                          │
│  │ ConflictResolution│                                         │
│  │ (CRDT/VectorClock)│                                       │
│  └────────────────┘                                          │
└─────────────────────────────────────────────────────────────┘
          │                    │                    │
          ▼                    ▼                    ▼
┌─────────────────────────────────────────────────────────────┐
│           Existing: replication.h + raft.h                    │
└─────────────────────────────────────────────────────────────┘
```

## 3. Phase 1: 强一致性保证 (Linearizability)

### 3.1 现有代码集成

- 复用 `raft.h` 的 RaftServer
- 复用 `replication.h` 的 repl_manager_t

### 3.2 线性一致性实现

```c
typedef struct linearizability_config {
    bool strict_linearizability;  /* 严格线性一致性 */
    int quorum_size;             /*  quorum 大小 */
    int ack_timeout_ms;          /* 确认超时 */
} linearizability_config_t;

/**
 * @brief 等待复制确认（线性一致性）
 *
 * 确保数据已复制到 quorum 节点后才返回
 */
int linearizability_wait_ack(repl_manager_t *mgr, uint64_t lsn,
                           int quorum, int timeout_ms);

/**
 * @brief 检查是否可以安全读取（租约）
 */
bool linearizability_can_read(const repl_manager_t *mgr);
```

## 4. Phase 2: 自动故障切换 (Auto Failover)

### 4.1 故障检测

```c
typedef enum {
    FAILOVER_IDLE,
    FAILOVER_DETECTING,
    FAILOVER_ELECTION,
    FAILOVER_PROMOTING,
    FAILOVER_COMPLETE
} failover_state_t;

typedef struct failover_config {
    int heartbeat_timeout_ms;    /* 心跳超时 */
    int election_timeout_ms;      /* 选举超时 */
    int max_retry;               /* 最大重试 */
    bool auto_promote;           /* 自动提升从节点 */
} failover_config_t;

/**
 * @brief 故障检测回调
 */
typedef void (*failover_callback_t)(int old_leader, int new_leader, void *arg);
```

### 4.2 自动切换流程

```
1. Leader 心跳超时 → 检测故障
2. 从节点发起选举 (Raft)
3. 获得 quorum 投票 → 成为新 Leader
4. 通知应用层切换
5. 重路由客户端请求
```

## 5. Phase 3: 多源复制 (Multi-source)

### 5.1 多源配置

```c
typedef struct replica_node {
    int node_id;
    char host[64];
    int port;
    repl_state_t state;
    uint64_t last_lsn;
    int64_t lag_ms;
} replica_node_t;

typedef struct multisource_config {
    replica_node_t *sources;
    int source_count;
    bool sync_all;              /* 同步所有源 */
    int min_sources;            /* 最少源数量 */
} multisource_config_t;
```

### 5.2 多源API

```c
/**
 * @brief 添加复制源
 */
int multisource_add_source(repl_manager_t *mgr, const replica_node_t *node);

/**
 * @brief 移除复制源
 */
int multisource_remove_source(repl_manager_t *mgr, int node_id);

/**
 * @brief 获取所有源状态
 */
int multisource_get_status(repl_manager_t *mgr, replica_node_t *nodes, int *count);

/**
 * @brief 同步所有源
 */
int multisource_sync_all(repl_manager_t *mgr);
```

## 6. Phase 4: 冲突解决 (Conflict Resolution)

### 6.1 Vector Clock

```c
#define MAX_NODES 64

typedef struct vector_clock {
    uint64_t clocks[MAX_NODES];  /* 每节点计数器 */
    int node_count;
    int local_node_id;
} vector_clock_t;

/**
 * @brief 增加本地时钟
 */
void vector_clock_inc(vector_clock_t *vc);

/**
 * @brief 合并两个时钟
 */
void vector_clock_merge(vector_clock_t *vc, const vector_clock_t *other);

/**
 * @brief 比较两个时钟
 * @return -1: vc < other, 0: 并发, 1: vc > other
 */
int vector_clock_compare(const vector_clock_t *vc, const vector_clock_t *other);
```

### 6.2 CRDT 实现

```c
typedef enum {
    CRDT_GCOUNTER,    /* 只增计数器 */
    CRDT_PNCOUNTER,   /* 正负计数器 */
    CRDT_LWWREG,      /* 最后写入胜出 */
    CRDT_ORSET,       /* 添加-删除集合 */
    CRDT_MVREG        /* 多值寄存器 */
} crdt_type_t;

typedef struct crdt {
    crdt_type_t type;
    void *state;
    vector_clock_t *vc;
} crdt_t;

/**
 * @brief 合并两个 CRDT
 */
int crdt_merge(crdt_t *a, const crdt_t *b);

/**
 * @brief 创建 CRDT
 */
crdt_t *crdt_create(crdt_type_t type);

/**
 * @brief 销毁 CRDT
 */
void crdt_destroy(crdt_t *crdt);
```

## 7. ReplicationConsensus 统一入口

```c
typedef struct replication_consensus {
    // Raft
    RaftServer_t *raft;

    // 复制
    repl_manager_t *repl;

    // 配置
    linearizability_config_t linear_cfg;
    failover_config_t failover_cfg;
    multisource_config_t multisource_cfg;

    // 冲突解决
    crdt_t **crdts;
    int crdt_count;

    pthread_rwlock_t rwlock;
} replication_consensus_t;
```

## 8. 文件结构

```
engineering/
├── include/db/consistency/
│   ├── replication_consensus.h   # 统一入口
│   ├── linearizability.h       # 线性一致性
│   ├── failover.h              # 自动故障切换
│   ├── multisource.h           # 多源复制
│   └── conflict_resolution.h    # 冲突解决 (CRDT/VC)
├── src/db/consistency/
│   ├── replication_consensus.c
│   ├── linearizability.c
│   ├── failover.c
│   ├── multisource.c
│   └── conflict_resolution.c
└── test/db/consistency/
    └── consistency_test.cpp
```

## 9. 实现顺序

| Phase | 内容 | 依赖 |
|-------|------|------|
| 1 | 线性一致性 | raft.h, replication.h |
| 2 | 自动故障切换 | Phase 1 |
| 3 | 多源复制 | Phase 1 |
| 4 | 冲突解决 | 无 |

## 10. 成功标准

- [ ] 线性一致性：数据复制到 quorum 后才返回
- [ ] 自动故障切换：Leader 故障后自动选举新 Leader
- [ ] 多源复制：支持多个从节点同步
- [ ] 冲突解决：Vector Clock + CRDT 实现
- [ ] 与现有 raft.h/replication.h 集成
- [ ] 单元测试覆盖
