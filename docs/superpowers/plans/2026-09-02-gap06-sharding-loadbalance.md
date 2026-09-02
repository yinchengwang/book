# Gap#6 自动化分片与负载均衡实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现自动化分片与负载均衡系统，支持阈值触发的再平衡、最小负载调度、可配置的迁移策略。

**Architecture:** 分片协调器（阈值检测/调度决策）+ 负载收集器（指标收集）+ 迁移管理器（数据迁移）+ Executor 集成（分片扫描）

**Tech Stack:** C 语言，CMake 构建，GTest 单元测试，pthread 线程

## Global Constraints

- 复用现有 `shard_router_t`、`shard_config_t`、`shard_info_t`（sharding.h）
- 遵循现有代码风格（extern "C"、命名下划线分隔）
- 所有新文件加入对应 CMakeLists.txt
- 与 Gap#3 ExecNode 集成（executor_framework.h）

---

### Task 1: 平衡配置与头文件定义

**Files:**
- Create: `engineering/include/db/sharding/shard_balance.h`
- Create: `engineering/include/db/sharding/shard_coordinator.h`
- Modify: `engineering/src/db/sharding/CMakeLists.txt`

**Interfaces:**
- Consumes: 现有 shard_config_t, shard_info_t
- Produces: shard_balance_config_t, shard_coordinator_t, load_collector_t, migrate_task_t

- [ ] **Step 1: 创建 shard_balance.h — 平衡配置结构**

```c
// engineering/include/db/sharding/shard_balance.h
#ifndef DB_SHARDING_BALANCE_H
#define DB_SHARDING_BALANCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 迁移策略 */
typedef enum {
    MIGRATE_INCREMENTAL = 0,     /* 增量迁移（Range/List 分片） */
    MIGRATE_VIRTUAL_NODE         /* 虚拟节点迁移（Hash 分片） */
} migrate_strategy_t;

/* 迁移状态 */
typedef enum {
    MIGRATE_STATUS_PENDING = 0,
    MIGRATE_STATUS_RUNNING,
    MIGRATE_STATUS_COMPLETED,
    MIGRATE_STATUS_FAILED
} migrate_status_t;

/* 平衡配置 */
typedef struct shard_balance_config {
    double skew_threshold;        /* 倾斜阈值（默认 1.5） */
    int64_t max_shard_size;      /* 最大分片大小（默认 10GB） */
    int check_interval_ms;       /* 检查间隔（默认 60000ms） */
    migrate_strategy_t strategy; /* 默认迁移策略 */
    bool auto_rebalance;         /* 自动再平衡开关（默认 true） */
} shard_balance_config_t;

/* 迁移任务 */
typedef struct migrate_task {
    int task_id;
    int source_shard;
    int target_shard;
    migrate_strategy_t strategy;
    void *key_range;
    double progress;              /* 0.0-1.0 */
    migrate_status_t status;
} migrate_task_t;

/* 配置默认值 */
#define DEFAULT_SKEW_THRESHOLD 1.5
#define DEFAULT_MAX_SHARD_SIZE (10ULL * 1024 * 1024 * 1024)
#define DEFAULT_CHECK_INTERVAL_MS 60000

/**
 * @brief 创建默认平衡配置
 */
shard_balance_config_t *shard_balance_config_create(void);

/**
 * @brief 销毁平衡配置
 */
void shard_balance_config_destroy(shard_balance_config_t *config);

/**
 * @brief 从配置字符串解析迁移策略
 */
migrate_strategy_t migrate_strategy_from_string(const char *str);

/**
 * @brief 获取迁移策略名称
 */
const char *migrate_strategy_to_string(migrate_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARDING_BALANCE_H */
```

- [ ] **Step 2: 创建 shard_coordinator.h — 协调器接口**

```c
// engineering/include/db/sharding/shard_coordinator.h
#ifndef DB_SHARDING_COORDINATOR_H
#define DB_SHARDING_COORDINATOR_H

#include "shard_balance.h"
#include "sharding.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 负载信息 */
typedef struct shard_load {
    int shard_id;
    uint64_t row_count;
    double qps;
    double latency_ms;
    double cpu_usage;
    int64_t size_bytes;
    time_t last_updated;
} shard_load_t;

/* 负载收集器 */
typedef struct load_collector load_collector_t;

/* 分片协调器 */
typedef struct shard_coordinator shard_coordinator_t;

/**
 * @brief 创建负载收集器
 */
load_collector_t *load_collector_create(int initial_capacity);

/**
 * @brief 销毁负载收集器
 */
void load_collector_destroy(load_collector_t *collector);

/**
 * @brief 更新分片负载信息
 */
int load_collector_update(load_collector_t *collector, const shard_load_t *load);

/**
 * @brief 获取分片负载信息
 */
const shard_load_t *load_collector_get(load_collector_t *collector, int shard_id);

/**
 * @brief 计算倾斜度（max / avg）
 */
double load_collector_calculate_skew(load_collector_t *collector);

/**
 * @brief 创建分片协调器
 */
shard_coordinator_t *shard_coordinator_create(const shard_balance_config_t *config,
                                               shard_router_t *router);

/**
 * @brief 销毁分片协调器
 */
void shard_coordinator_destroy(shard_coordinator_t *coord);

/**
 * @brief 启动协调器（启动后台监控线程）
 */
int shard_coordinator_start(shard_coordinator_t *coord);

/**
 * @brief 停止协调器
 */
void shard_coordinator_stop(shard_coordinator_t *coord);

/**
 * @brief 手动触发再平衡检查
 */
int shard_coordinator_check_and_rebalance(shard_coordinator_t *coord);

/**
 * @brief 选择最小负载的分片
 */
int shard_coordinator_select_least_load(shard_coordinator_t *coord,
                                         const int *candidate_shards,
                                         int count);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARDING_COORDINATOR_H */
```

- [ ] **Step 3: 更新 CMakeLists.txt**

```cmake
# engineering/src/db/sharding/CMakeLists.txt
# 添加 shard_balance.c 和 shard_coordinator.c（Task 2-3 实现）
```

- [ ] **Step 4: 提交**

```bash
git add engineering/include/db/sharding/shard_balance.h \
        engineering/include/db/sharding/shard_coordinator.h \
        engineering/src/db/sharding/CMakeLists.txt
git commit -m "feat(sharding): Add balance config and coordinator headers"
```

---

### Task 2: 负载收集器实现

**Files:**
- Create: `engineering/src/db/sharding/load_collector.c`
- Modify: `engineering/src/db/sharding/CMakeLists.txt`

**Interfaces:**
- Consumes: shard_coordinator.h
- Produces: load_collector_* 函数

- [ ] **Step 1: 创建 load_collector.c**

```c
// engineering/src/db/sharding/load_collector.c
#include "db/sharding/shard_coordinator.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct load_collector {
    shard_load_t *shards;
    int capacity;
    int count;
    pthread_mutex_t mutex;
};

load_collector_t *load_collector_create(int initial_capacity) {
    load_collector_t *c = (load_collector_t *)calloc(1, sizeof(load_collector_t));
    if (!c) return NULL;

    c->shards = (shard_load_t *)calloc(initial_capacity, sizeof(shard_load_t));
    if (!c->shards) {
        free(c);
        return NULL;
    }

    c->capacity = initial_capacity;
    c->count = 0;
    pthread_mutex_init(&c->mutex, NULL);
    return c;
}

void load_collector_destroy(load_collector_t *c) {
    if (!c) return;
    pthread_mutex_destroy(&c->mutex);
    free(c->shards);
    free(c);
}

int load_collector_update(load_collector_t *c, const shard_load_t *load) {
    if (!c || !load) return -1;

    pthread_mutex_lock(&c->mutex);

    // 查找或插入
    for (int i = 0; i < c->count; i++) {
        if (c->shards[i].shard_id == load->shard_id) {
            c->shards[i] = *load;
            pthread_mutex_unlock(&c->mutex);
            return 0;
        }
    }

    // 需要扩容
    if (c->count >= c->capacity) {
        int new_cap = c->capacity * 2;
        shard_load_t *new_shards = (shard_load_t *)realloc(c->shards,
            new_cap * sizeof(shard_load_t));
        if (!new_shards) {
            pthread_mutex_unlock(&c->mutex);
            return -1;
        }
        c->shards = new_shards;
        c->capacity = new_cap;
    }

    c->shards[c->count++] = *load;
    pthread_mutex_unlock(&c->mutex);
    return 0;
}

const shard_load_t *load_collector_get(load_collector_t *c, int shard_id) {
    if (!c) return NULL;

    pthread_mutex_lock(&c->mutex);
    for (int i = 0; i < c->count; i++) {
        if (c->shards[i].shard_id == shard_id) {
            pthread_mutex_unlock(&c->mutex);
            return &c->shards[i];
        }
    }
    pthread_mutex_unlock(&c->mutex);
    return NULL;
}

double load_collector_calculate_skew(load_collector_t *c) {
    if (!c || c->count == 0) return 0.0;

    pthread_mutex_lock(&c->mutex);

    // 计算总行数和最大值
    uint64_t total = 0;
    uint64_t max_rows = 0;
    for (int i = 0; i < c->count; i++) {
        total += c->shards[i].row_count;
        if (c->shards[i].row_count > max_rows) {
            max_rows = c->shards[i].row_count;
        }
    }

    pthread_mutex_unlock(&c->mutex);

    if (total == 0) return 0.0;
    double avg = (double)total / c->count;
    if (avg == 0) return 0.0;
    return (double)max_rows / avg;
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
# 添加 load_collector.c
```

- [ ] **Step 3: 提交**

```bash
git add engineering/src/db/sharding/load_collector.c \
        engineering/src/db/sharding/CMakeLists.txt
git commit -m "feat(sharding): Add load collector implementation"
```

---

### Task 3: 分片协调器实现

**Files:**
- Create: `engineering/src/db/sharding/shard_balance.c`
- Create: `engineering/src/db/sharding/shard_coordinator.c`
- Modify: `engineering/src/db/sharding/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1-2 的头文件
- Produces: shard_coordinator_* 函数

- [ ] **Step 1: 创建 shard_balance.c**

```c
// engineering/src/db/sharding/shard_balance.c
#include "db/sharding/shard_balance.h"
#include <stdlib.h>
#include <string.h>

shard_balance_config_t *shard_balance_config_create(void) {
    shard_balance_config_t *cfg = (shard_balance_config_t *)calloc(1,
        sizeof(shard_balance_config_t));
    if (!cfg) return NULL;

    cfg->skew_threshold = DEFAULT_SKEW_THRESHOLD;
    cfg->max_shard_size = DEFAULT_MAX_SHARD_SIZE;
    cfg->check_interval_ms = DEFAULT_CHECK_INTERVAL_MS;
    cfg->strategy = MIGRATE_INCREMENTAL;  // 默认增量迁移
    cfg->auto_rebalance = true;

    return cfg;
}

void shard_balance_config_destroy(shard_balance_config_t *cfg) {
    free(cfg);
}

migrate_strategy_t migrate_strategy_from_string(const char *str) {
    if (!str) return MIGRATE_INCREMENTAL;
    if (strcmp(str, "virtual-node") == 0 || strcmp(str, "vnode") == 0) {
        return MIGRATE_VIRTUAL_NODE;
    }
    return MIGRATE_INCREMENTAL;
}

const char *migrate_strategy_to_string(migrate_strategy_t s) {
    switch (s) {
        case MIGRATE_VIRTUAL_NODE: return "virtual-node";
        case MIGRATE_INCREMENTAL:
        default: return "incremental";
    }
}
```

- [ ] **Step 2: 创建 shard_coordinator.c**

```c
// engineering/src/db/sharding/shard_coordinator.c
#include "db/sharding/shard_coordinator.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

struct shard_coordinator {
    shard_balance_config_t *config;
    shard_router_t *router;
    load_collector_t *collector;
    bool running;
    pthread_t monitor_thread;
};

static void *monitor_thread_func(void *arg) {
    shard_coordinator_t *coord = (shard_coordinator_t *)arg;

    while (coord->running) {
        sleep(coord->config->check_interval_ms / 1000);

        if (!coord->config->auto_rebalance) continue;

        double skew = load_collector_calculate_skew(coord->collector);
        if (skew > coord->config->skew_threshold) {
            // 阈值超限，触发再平衡检查
            shard_coordinator_check_and_rebalance(coord);
        }
    }
    return NULL;
}

shard_coordinator_t *shard_coordinator_create(const shard_balance_config_t *config,
                                               shard_router_t *router) {
    if (!config || !router) return NULL;

    shard_coordinator_t *coord = (shard_coordinator_t *)calloc(1,
        sizeof(shard_coordinator_t));
    if (!coord) return NULL;

    coord->config = (shard_balance_config_t *)malloc(sizeof(shard_balance_config_t));
    if (!coord->config) {
        free(coord);
        return NULL;
    }
    memcpy(coord->config, config, sizeof(shard_balance_config_t));

    coord->router = router;
    coord->collector = load_collector_create(16);
    if (!coord->collector) {
        free(coord->config);
        free(coord);
        return NULL;
    }

    coord->running = false;
    return coord;
}

void shard_coordinator_destroy(shard_coordinator_t *coord) {
    if (!coord) return;
    shard_coordinator_stop(coord);
    if (coord->collector) load_collector_destroy(coord->collector);
    if (coord->config) free(coord->config);
    free(coord);
}

int shard_coordinator_start(shard_coordinator_t *coord) {
    if (!coord || coord->running) return -1;
    coord->running = true;
    if (pthread_create(&coord->monitor_thread, NULL, monitor_thread_func, coord) != 0) {
        coord->running = false;
        return -1;
    }
    return 0;
}

void shard_coordinator_stop(shard_coordinator_t *coord) {
    if (!coord || !coord->running) return;
    coord->running = false;
    pthread_join(coord->monitor_thread, NULL);
}

int shard_coordinator_check_and_rebalance(shard_coordinator_t *coord) {
    if (!coord) return -1;
    // TODO: Task 4 实现再平衡逻辑
    return 0;
}

int shard_coordinator_select_least_load(shard_coordinator_t *coord,
                                         const int *candidate_shards,
                                         int count) {
    if (!coord || !candidate_shards || count <= 0) return -1;

    int best_shard = -1;
    double min_load = 1e100;  // DBL_MAX 近似

    for (int i = 0; i < count; i++) {
        const shard_load_t *load = load_collector_get(coord->collector,
                                                       candidate_shards[i]);
        if (!load) continue;

        // 负载计算公式：load = 0.4 * row_count + 0.3 * qps + 0.3 * latency_ms
        double load = 0.4 * load->row_count +
                      0.3 * load->qps +
                      0.3 * load->latency_ms;

        if (load < min_load) {
            min_load = load;
            best_shard = candidate_shards[i];
        }
    }

    return best_shard;
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

- [ ] **Step 4: 提交**

---

### Task 4: 迁移管理器实现

**Files:**
- Create: `engineering/src/db/sharding/migrate_manager.c`
- Create: `engineering/include/db/sharding/migrate_manager.h`
- Modify: `engineering/src/db/sharding/CMakeLists.txt`

**Interfaces:**
- Consumes: shard_balance.h, shard_coordinator.h
- Produces: migrate_manager_* 函数

- [ ] **Step 1: 创建 migrate_manager.h**

```c
// engineering/include/db/sharding/migrate_manager.h
#ifndef DB_SHARDING_MIGRATE_MANAGER_H
#define DB_SHARDING_MIGRATE_MANAGER_H

#include "shard_balance.h"
#include "sharding.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct migrate_manager migrate_manager_t;

/**
 * @brief 创建迁移管理器
 */
migrate_manager_t *migrate_manager_create(shard_router_t *router);

/**
 * @brief 销毁迁移管理器
 */
void migrate_manager_destroy(migrate_manager_t *mgr);

/**
 * @brief 创建增量迁移任务
 */
migrate_task_t *migrate_manager_create_incremental(migrate_manager_t *mgr,
                                                    int source_shard,
                                                    int target_shard,
                                                    const void *key_range,
                                                    size_t range_len);

/**
 * @brief 创建虚拟节点迁移任务
 */
migrate_task_t *migrate_manager_create_vnode(migrate_manager_t *mgr,
                                              int source_shard,
                                              int target_shard,
                                              int vnode_id);

/**
 * @brief 执行迁移任务
 */
int migrate_manager_execute(migrate_manager_t *mgr, migrate_task_t *task);

/**
 * @brief 获取迁移任务状态
 */
migrate_status_t migrate_manager_get_status(migrate_manager_t *mgr, int task_id);

/**
 * @brief 取消迁移任务
 */
int migrate_manager_cancel(migrate_manager_t *mgr, int task_id);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARDING_MIGRATE_MANAGER_H */
```

- [ ] **Step 2: 创建 migrate_manager.c（简化为框架，实际迁移逻辑 Task 6）**

- [ ] **Step 3: 提交**

---

### Task 5: 分片扫描算子（Executor 集成）

**Files:**
- Create: `engineering/src/db/executor/operators/shard_scan_exec.c`
- Create: `engineering/include/db/executor/exec_shard.h`
- Modify: `engineering/src/db/executor/operators/CMakeLists.txt`

**Interfaces:**
- Consumes: executor_framework.h, shard_coordinator.h
- Produces: shard_scan_exec.c

- [ ] **Step 1: 创建 exec_shard.h**

```c
// engineering/include/db/executor/exec_shard.h
#ifndef DB_EXECUTOR_EXEC_SHARD_H
#define DB_EXECUTOR_EXEC_SHARD_H

#include "db/executor/exec_node.h"
#include "db/sharding/shard_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建分片扫描 ExecNode
 */
ExecNode *exec_create_shard_scan(shard_coordinator_t *coord,
                                  const void *key, size_t key_len);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_SHARD_H */
```

- [ ] **Step 2: 创建 shard_scan_exec.c**

- [ ] **Step 3: 提交**

---

### Task 6: 增量迁移实现

**Files:**
- Modify: `engineering/src/db/sharding/migrate_manager.c`

- [ ] **Step 1: 实现增量迁移核心逻辑**

```c
int migrate_execute_incremental(migrate_manager_t *mgr, migrate_task_t *task) {
    // 1. 标记任务为 RUNNING
    task->status = MIGRATE_STATUS_RUNNING;

    // 2. 读取源分片数据（key_range 范围内）
    // 3. 写入目标分片
    // 4. 双写期间：同时写入源和目标
    // 5. 验证数据一致性
    // 6. 更新路由表
    // 7. 删除源分片数据

    task->status = MIGRATE_STATUS_COMPLETED;
    task->progress = 1.0;
    return 0;
}
```

- [ ] **Step 2: 提交**

---

### Task 7: 虚拟节点迁移实现

**Files:**
- Modify: `engineering/src/db/sharding/migrate_manager.c`

- [ ] **Step 1: 实现虚拟节点迁移核心逻辑**

```c
int migrate_execute_vnode(migrate_manager_t *mgr, migrate_task_t *task) {
    // 1. 更新一致性哈希环
    // 2. 迁移受影响的 vnode 数据
    // 3. 更新路由映射
    // 4. 验证

    task->status = MIGRATE_STATUS_COMPLETED;
    task->progress = 1.0;
    return 0;
}
```

- [ ] **Step 2: 提交**

---

### Task 8: 单元测试

**Files:**
- Create: `engineering/test/db/sharding/balance_test.cpp`
- Create: `engineering/test/db/sharding/CMakeLists.txt`
- Modify: `engineering/test/db/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1-7 实现
- Produces: 单元测试

- [ ] **Step 1: 创建 balance_test.cpp**

```cpp
#include <gtest/gtest.h>
#include "db/sharding/shard_balance.h"
#include "db/sharding/shard_coordinator.h"

class ShardBalanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = shard_balance_config_create();
    }
    void TearDown() override {
        if (config) shard_balance_config_destroy(config);
    }
    shard_balance_config_t *config = nullptr;
};

// 测试默认配置
TEST_F(ShardBalanceTest, DefaultConfig) {
    EXPECT_EQ(config->skew_threshold, DEFAULT_SKEW_THRESHOLD);
    EXPECT_EQ(config->max_shard_size, DEFAULT_MAX_SHARD_SIZE);
    EXPECT_TRUE(config->auto_rebalance);
}

// 测试迁移策略转换
TEST_F(ShardBalanceTest, MigrateStrategyConversion) {
    EXPECT_EQ(migrate_strategy_from_string("incremental"), MIGRATE_INCREMENTAL);
    EXPECT_EQ(migrate_strategy_from_string("virtual-node"), MIGRATE_VIRTUAL_NODE);
    EXPECT_STREQ(migrate_strategy_to_string(MIGRATE_VIRTUAL_NODE), "virtual-node");
}

// 测试负载收集器
TEST_F(ShardBalanceTest, LoadCollector) {
    load_collector_t *collector = load_collector_create(4);
    ASSERT_NE(collector, nullptr);

    shard_load_t load1 = {.shard_id = 1, .row_count = 100, .qps = 10, .latency_ms = 5};
    EXPECT_EQ(load_collector_update(collector, &load1), 0);

    const shard_load_t *result = load_collector_get(collector, 1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->row_count, 100);

    // 测试倾斜度计算
    shard_load_t load2 = {.shard_id = 2, .row_count = 300, .qps = 10, .latency_ms = 5};
    load_collector_update(collector, &load2);

    double skew = load_collector_calculate_skew(collector);
    EXPECT_GT(skew, 1.0);  // 300/200 = 1.5

    load_collector_destroy(collector);
}

// 测试协调器创建
TEST_F(ShardBalanceTest, CoordinatorCreate) {
    shard_router_t *router = shard_router_create(NULL);  // NULL config for test
    ASSERT_NE(router, nullptr);

    shard_coordinator_t *coord = shard_coordinator_create(config, router);
    EXPECT_NE(coord, nullptr);

    shard_coordinator_destroy(coord);
    shard_router_destroy(router);
}
```

- [ ] **Step 2: 创建 CMakeLists.txt**

- [ ] **Step 3: 更新 test/db/CMakeLists.txt**

- [ ] **Step 4: 编译测试**

```bash
cd build/engineering && cmake --build . --target balance_test -j 4
ctest --test-dir build/engineering -R balance_test --output-on-failure
```

- [ ] **Step 5: 提交**

---

### Task 9: 集成测试

**Files:**
- Create: `engineering/test/db/sharding/coordinator_test.cpp`
- Modify: `engineering/test/db/sharding/CMakeLists.txt`

- [ ] **Step 1: 创建 coordinator_test.cpp**

```cpp
// 测试完整再平衡流程
TEST(CoordinatorIntegration, RebalanceFlow) {
    // 1. 创建配置和协调器
    // 2. 添加分片和负载信息
    // 3. 触发再平衡
    // 4. 验证迁移任务创建
}
```

- [ ] **Step 2: 编译测试并验证**

- [ ] **Step 3: 提交**

---

## 任务依赖关系

```
Task 1: 头文件定义        ← 基础
Task 2: 负载收集器        ← 依赖 Task 1
Task 3: 分片协调器        ← 依赖 Task 1, 2
Task 4: 迁移管理器        ← 依赖 Task 1, 3
Task 5: Executor 集成     ← 依赖 Task 3
Task 6: 增量迁移          ← 依赖 Task 4
Task 7: 虚拟节点迁移      ← 依赖 Task 4
Task 8: 单元测试          ← 依赖 Task 1-7
Task 9: 集成测试          ← 依赖 Task 1-8
```

## 成功标准

- [ ] Task 1: 头文件定义完整
- [ ] Task 2: 负载收集器工作正常
- [ ] Task 3: 协调器可启动/停止
- [ ] Task 4: 迁移管理器框架完成
- [ ] Task 5: 分片扫描算子可工作
- [ ] Task 6: 增量迁移实现
- [ ] Task 7: 虚拟节点迁移实现
- [ ] Task 8: 单元测试全部通过
- [ ] Task 9: 集成测试通过

---

**Plan complete and saved to `docs/superpowers/plans/2026-09-02-gap06-sharding-loadbalance.md`.**

**Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
