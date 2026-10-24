/**
 * @file sharding_balance_test.cpp
 * @brief Gap#6 分片与负载均衡系统单元测试
 *
 * 测试组件：
 * 1. shard_balance_config - 配置创建/销毁/验证
 * 2. load_collector - 负载更新/查询/倾斜度计算
 * 3. shard_coordinator - 创建/启动/停止/选择最小负载
 * 4. migrate_manager - 任务创建/执行/取消
 */

#include "db/sharding/shard_balance.h"
#include "db/sharding/shard_coordinator.h"
#include "db/sharding/migrate_manager.h"
#include "db/sharding/sharding.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>

/* ============================================================
 * shard_balance_config 测试
 * ============================================================ */

class ShardBalanceConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ShardBalanceConfigTest, CreateAndDestroy) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    // 验证默认值
    EXPECT_DOUBLE_EQ(cfg->skew_threshold, DEFAULT_SKEW_THRESHOLD);
    EXPECT_EQ(cfg->max_shard_size, DEFAULT_MAX_SHARD_SIZE);
    EXPECT_EQ(cfg->check_interval_ms, DEFAULT_CHECK_INTERVAL_MS);
    EXPECT_EQ(cfg->strategy, MIGRATE_INCREMENTAL);
    EXPECT_TRUE(cfg->auto_rebalance);

    shard_balance_config_destroy(cfg);
}

TEST_F(ShardBalanceConfigTest, CustomValues) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    // 修改默认值
    cfg->skew_threshold = 2.0;
    cfg->max_shard_size = 20ULL * 1024 * 1024 * 1024;  // 20GB
    cfg->check_interval_ms = 30000;
    cfg->strategy = MIGRATE_VIRTUAL_NODE;
    cfg->auto_rebalance = false;

    EXPECT_DOUBLE_EQ(cfg->skew_threshold, 2.0);
    EXPECT_EQ(cfg->max_shard_size, 20ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(cfg->check_interval_ms, 30000);
    EXPECT_EQ(cfg->strategy, MIGRATE_VIRTUAL_NODE);
    EXPECT_FALSE(cfg->auto_rebalance);

    shard_balance_config_destroy(cfg);
}

TEST_F(ShardBalanceConfigTest, StrategyStringConversion) {
    // 测试从字符串解析
    EXPECT_EQ(migrate_strategy_from_string("incremental"), MIGRATE_INCREMENTAL);
    EXPECT_EQ(migrate_strategy_from_string("virtual-node"), MIGRATE_VIRTUAL_NODE);
    EXPECT_EQ(migrate_strategy_from_string("vnode"), MIGRATE_VIRTUAL_NODE);
    EXPECT_EQ(migrate_strategy_from_string("invalid"), MIGRATE_INCREMENTAL);
    EXPECT_EQ(migrate_strategy_from_string(NULL), MIGRATE_INCREMENTAL);

    // 测试转换为字符串
    EXPECT_STREQ(migrate_strategy_to_string(MIGRATE_INCREMENTAL), "incremental");
    EXPECT_STREQ(migrate_strategy_to_string(MIGRATE_VIRTUAL_NODE), "virtual-node");
}

TEST_F(ShardBalanceConfigTest, NullDestruction) {
    // 销毁 NULL 配置不应该崩溃
    EXPECT_NO_THROW(shard_balance_config_destroy(NULL));
}

/* ============================================================
 * load_collector 测试
 * ============================================================ */

class LoadCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(LoadCollectorTest, CreateAndDestroy) {
    load_collector_t *c = load_collector_create(16);
    ASSERT_NE(c, nullptr);

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, CreateWithZeroCapacity) {
    // 容量为 0 应该有默认值
    load_collector_t *c = load_collector_create(0);
    EXPECT_NE(c, nullptr);
    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, DestroyNull) {
    // 销毁 NULL 不应该崩溃
    EXPECT_NO_THROW(load_collector_destroy(NULL));
}

TEST_F(LoadCollectorTest, UpdateAndGet) {
    load_collector_t *c = load_collector_create(16);
    ASSERT_NE(c, nullptr);

    shard_load_t load1 = {
        .shard_id = 1,
        .row_count = 1000,
        .qps = 50.0,
        .latency_ms = 10.0,
        .cpu_usage = 0.3,
        .size_bytes = 1024 * 1024,
        .last_updated = time(NULL)
    };

    EXPECT_EQ(load_collector_update(c, &load1), 0);

    const shard_load_t *result = load_collector_get(c, 1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->shard_id, 1);
    EXPECT_EQ(result->row_count, 1000);
    EXPECT_DOUBLE_EQ(result->qps, 50.0);

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, UpdateSameShard) {
    load_collector_t *c = load_collector_create(16);
    ASSERT_NE(c, nullptr);

    shard_load_t load1 = {.shard_id = 1, .row_count = 1000, .qps = 50.0};
    shard_load_t load2 = {.shard_id = 1, .row_count = 2000, .qps = 100.0};

    EXPECT_EQ(load_collector_update(c, &load1), 0);
    EXPECT_EQ(load_collector_update(c, &load2), 0);

    const shard_load_t *result = load_collector_get(c, 1);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->row_count, 2000);  // 应该被更新

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, GetNonExistent) {
    load_collector_t *c = load_collector_create(16);
    ASSERT_NE(c, nullptr);

    EXPECT_EQ(load_collector_get(c, 999), nullptr);

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, GetFromNullCollector) {
    EXPECT_EQ(load_collector_get(NULL, 1), nullptr);
}

TEST_F(LoadCollectorTest, UpdateNullParams) {
    load_collector_t *c = load_collector_create(16);
    ASSERT_NE(c, nullptr);

    shard_load_t load = {.shard_id = 1, .row_count = 1000};

    EXPECT_EQ(load_collector_update(NULL, &load), -1);
    EXPECT_EQ(load_collector_update(c, NULL), -1);

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, CalculateSkewEmpty) {
    load_collector_t *c = load_collector_create(16);
    ASSERT_NE(c, nullptr);

    // 空收集器的倾斜度应该为 0
    EXPECT_DOUBLE_EQ(load_collector_calculate_skew(c), 0.0);

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, CalculateSkewNull) {
    EXPECT_DOUBLE_EQ(load_collector_calculate_skew(NULL), 0.0);
}

TEST_F(LoadCollectorTest, CalculateSkewUniform) {
    load_collector_t *c = load_collector_create(16);
    ASSERT_NE(c, nullptr);

    // 所有分片负载相同，倾斜度应该为 1.0
    for (int i = 0; i < 4; i++) {
        shard_load_t load = {
            .shard_id = i,
            .row_count = 1000,
            .qps = 50.0,
            .latency_ms = 10.0
        };
        load_collector_update(c, &load);
    }

    double skew = load_collector_calculate_skew(c);
    EXPECT_DOUBLE_EQ(skew, 1.0);

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, CalculateSkewImbalanced) {
    load_collector_t *c = load_collector_create(16);
    ASSERT_NE(c, nullptr);

    // 添加严重不均衡的负载
    shard_load_t load1 = {.shard_id = 0, .row_count = 1000};
    shard_load_t load2 = {.shard_id = 1, .row_count = 1000};
    shard_load_t load3 = {.shard_id = 2, .row_count = 1000};
    shard_load_t load4 = {.shard_id = 3, .row_count = 10000};  // 10x others

    load_collector_update(c, &load1);
    load_collector_update(c, &load2);
    load_collector_update(c, &load3);
    load_collector_update(c, &load4);

    double skew = load_collector_calculate_skew(c);
    // max=10000, avg=3250, skew=10000/3250=3.077
    EXPECT_DOUBLE_EQ(skew, 10000.0 / 3250.0);

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, CapacityExpansion) {
    load_collector_t *c = load_collector_create(2);  // 小初始容量
    ASSERT_NE(c, nullptr);

    // 添加超过初始容量的数据，验证能正常扩容
    for (int i = 0; i < 10; i++) {
        shard_load_t load = {
            .shard_id = i,
            .row_count = 1000 * (uint64_t)i,
            .qps = 50.0
        };
        EXPECT_EQ(load_collector_update(c, &load), 0);
    }

    // 验证所有数据都能获取
    for (int i = 0; i < 10; i++) {
        const shard_load_t *result = load_collector_get(c, i);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->row_count, (uint64_t)(1000 * i));
    }

    load_collector_destroy(c);
}

TEST_F(LoadCollectorTest, ConcurrentUpdate) {
    load_collector_t *c = load_collector_create(100);
    ASSERT_NE(c, nullptr);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // 多个线程同时更新
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([c, &success_count, t]() {
            for (int i = 0; i < 100; i++) {
                shard_load_t load = {
                    .shard_id = t * 100 + i,
                    .row_count = 1000,
                    .qps = 50.0
                };
                if (load_collector_update(c, &load) == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    // 至少应该有成功更新
    EXPECT_GT(success_count.load(), 0);

    load_collector_destroy(c);
}

/* ============================================================
 * shard_coordinator 测试
 * ============================================================ */

class ShardCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建分片配置和路由器
        shard_config_t config;
        memset(&config, 0, sizeof(config));
        config.strategy = SHARD_HASH;
        config.key_type = SHARD_KEY_STRING;
        config.num_shards = 4;
        config.replication_factor = 1;
        config.consistent_hashing = false;

        router = shard_router_create(&config);

        // 添加分片
        for (int i = 0; i < 4; i++) {
            shard_info_t shard;
            memset(&shard, 0, sizeof(shard));
            shard.shard_id = i;
            shard.host = "localhost";
            shard.port = (short)(5432 + i);
            shard.is_primary = true;
            shard_router_add(router, &shard);
        }
    }

    void TearDown() override {
        if (router) {
            shard_router_destroy(router);
        }
    }

    shard_router_t *router = nullptr;
};

TEST_F(ShardCoordinatorTest, CreateAndDestroy) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    shard_coordinator_t *coord = shard_coordinator_create(cfg, router);
    ASSERT_NE(coord, nullptr);

    // 验证路由器获取
    EXPECT_EQ(shard_coordinator_get_router(coord), router);

    shard_coordinator_destroy(coord);
    shard_balance_config_destroy(cfg);
}

TEST_F(ShardCoordinatorTest, CreateWithNullParams) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    EXPECT_EQ(shard_coordinator_create(NULL, router), nullptr);
    EXPECT_EQ(shard_coordinator_create(cfg, NULL), nullptr);

    shard_balance_config_destroy(cfg);
}

TEST_F(ShardCoordinatorTest, StartAndStop) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    cfg->check_interval_ms = 100;  // 100ms 间隔
    ASSERT_NE(cfg, nullptr);

    shard_coordinator_t *coord = shard_coordinator_create(cfg, router);
    ASSERT_NE(coord, nullptr);

    // 启动
    EXPECT_EQ(shard_coordinator_start(coord), 0);

    // 停止
    shard_coordinator_stop(coord);

    // 再次启动应该成功
    EXPECT_EQ(shard_coordinator_start(coord), 0);
    shard_coordinator_stop(coord);

    shard_coordinator_destroy(coord);
    shard_balance_config_destroy(cfg);
}

TEST_F(ShardCoordinatorTest, DoubleStart) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    shard_coordinator_t *coord = shard_coordinator_create(cfg, router);
    ASSERT_NE(coord, nullptr);

    EXPECT_EQ(shard_coordinator_start(coord), 0);
    EXPECT_EQ(shard_coordinator_start(coord), -1);  // 重复启动应该失败

    shard_coordinator_stop(coord);
    shard_coordinator_destroy(coord);
    shard_balance_config_destroy(cfg);
}

TEST_F(ShardCoordinatorTest, StopNull) {
    // 停止 NULL 不应该崩溃
    EXPECT_NO_THROW(shard_coordinator_stop(NULL));
}

TEST_F(ShardCoordinatorTest, SelectLeastLoadNoData) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    shard_coordinator_t *coord = shard_coordinator_create(cfg, router);
    ASSERT_NE(coord, nullptr);

    int shards[] = {0, 1, 2, 3};

    // 没有负载数据时，应该返回 -1
    int selected = shard_coordinator_select_least_load(coord, shards, 4);
    EXPECT_EQ(selected, -1);

    shard_coordinator_destroy(coord);
    shard_balance_config_destroy(cfg);
}

TEST_F(ShardCoordinatorTest, SelectLeastLoadNullParams) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    shard_coordinator_t *coord = shard_coordinator_create(cfg, router);
    ASSERT_NE(coord, nullptr);

    int shards[] = {0, 1};

    EXPECT_EQ(shard_coordinator_select_least_load(NULL, shards, 2), -1);
    EXPECT_EQ(shard_coordinator_select_least_load(coord, NULL, 2), -1);
    EXPECT_EQ(shard_coordinator_select_least_load(coord, shards, 0), -1);
    EXPECT_EQ(shard_coordinator_select_least_load(coord, shards, -1), -1);

    shard_coordinator_destroy(coord);
    shard_balance_config_destroy(cfg);
}

TEST_F(ShardCoordinatorTest, CheckAndRebalance) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    shard_coordinator_t *coord = shard_coordinator_create(cfg, router);
    ASSERT_NE(coord, nullptr);

    // 触发再平衡检查不应该崩溃
    EXPECT_EQ(shard_coordinator_check_and_rebalance(coord), 0);
    EXPECT_EQ(shard_coordinator_check_and_rebalance(NULL), -1);

    shard_coordinator_destroy(coord);
    shard_balance_config_destroy(cfg);
}

TEST_F(ShardCoordinatorTest, GetRouter) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    shard_coordinator_t *coord = shard_coordinator_create(cfg, router);
    ASSERT_NE(coord, nullptr);

    EXPECT_EQ(shard_coordinator_get_router(coord), router);
    EXPECT_EQ(shard_coordinator_get_router(NULL), nullptr);

    shard_coordinator_destroy(coord);
    shard_balance_config_destroy(cfg);
}

/* ============================================================
 * migrate_manager 测试
 * ============================================================ */

class MigrateManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建分片配置和路由器
        shard_config_t config;
        memset(&config, 0, sizeof(config));
        config.strategy = SHARD_HASH;
        config.key_type = SHARD_KEY_STRING;
        config.num_shards = 4;
        config.replication_factor = 1;
        config.consistent_hashing = false;

        router = shard_router_create(&config);

        // 添加分片
        for (int i = 0; i < 4; i++) {
            shard_info_t shard;
            memset(&shard, 0, sizeof(shard));
            shard.shard_id = i;
            shard.host = "localhost";
            shard.port = (short)(5432 + i);
            shard.is_primary = true;
            shard_router_add(router, &shard);
        }
    }

    void TearDown() override {
        if (router) {
            shard_router_destroy(router);
        }
    }

    shard_router_t *router = nullptr;
};

TEST_F(MigrateManagerTest, CreateAndDestroy) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, CreateWithNullRouter) {
    // 允许 NULL router
    migrate_manager_t *mgr = migrate_manager_create(NULL);
    EXPECT_NE(mgr, nullptr);
    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, DestroyNull) {
    EXPECT_NO_THROW(migrate_manager_destroy(NULL));
}

TEST_F(MigrateManagerTest, CreateIncrementalTask) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    const char *key_range = "user_1000";
    migrate_task_t *task = migrate_manager_create_incremental(
        mgr, 0, 1, key_range, strlen(key_range));

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->source_shard, 0);
    EXPECT_EQ(task->target_shard, 1);
    EXPECT_EQ(task->strategy, MIGRATE_INCREMENTAL);
    EXPECT_EQ(task->status, MIGRATE_STATUS_PENDING);
    EXPECT_GT(task->task_id, 0);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, CreateIncrementalTaskNullManager) {
    migrate_task_t *task = migrate_manager_create_incremental(NULL, 0, 1, NULL, 0);
    EXPECT_EQ(task, nullptr);
}

TEST_F(MigrateManagerTest, CreateVNodeTask) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    migrate_task_t *task = migrate_manager_create_vnode(mgr, 0, 1, 42);

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->source_shard, 0);
    EXPECT_EQ(task->target_shard, 1);
    EXPECT_EQ(task->strategy, MIGRATE_VIRTUAL_NODE);
    EXPECT_EQ(task->vnode_id, 42);
    EXPECT_EQ(task->status, MIGRATE_STATUS_PENDING);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, CreateVNodeTaskNullManager) {
    migrate_task_t *task = migrate_manager_create_vnode(NULL, 0, 1, 42);
    EXPECT_EQ(task, nullptr);
}

TEST_F(MigrateManagerTest, MultipleTasks) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    // 创建多个任务
    migrate_task_t *task1 = migrate_manager_create_incremental(mgr, 0, 1, "key1", 4);
    migrate_task_t *task2 = migrate_manager_create_vnode(mgr, 1, 2, 10);
    migrate_task_t *task3 = migrate_manager_create_incremental(mgr, 2, 3, "key2", 4);

    ASSERT_NE(task1, nullptr);
    ASSERT_NE(task2, nullptr);
    ASSERT_NE(task3, nullptr);

    // 任务 ID 应该递增
    EXPECT_LT(task1->task_id, task2->task_id);
    EXPECT_LT(task2->task_id, task3->task_id);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, GetStatus) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    migrate_task_t *task = migrate_manager_create_incremental(mgr, 0, 1, "key", 3);
    ASSERT_NE(task, nullptr);

    // 获取刚创建任务的状态
    migrate_status_t status = migrate_manager_get_status(mgr, task->task_id);
    EXPECT_EQ(status, MIGRATE_STATUS_PENDING);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, GetStatusNullManager) {
    migrate_status_t status = migrate_manager_get_status(NULL, 1);
    EXPECT_EQ(status, MIGRATE_STATUS_FAILED);
}

TEST_F(MigrateManagerTest, GetStatusNonExistent) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    migrate_status_t status = migrate_manager_get_status(mgr, 9999);
    EXPECT_EQ(status, MIGRATE_STATUS_FAILED);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, CancelPendingTask) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    migrate_task_t *task = migrate_manager_create_incremental(mgr, 0, 1, "key", 3);
    ASSERT_NE(task, nullptr);

    // 取消待处理任务应该成功
    EXPECT_EQ(migrate_manager_cancel(mgr, task->task_id), 0);

    // 状态应该变为 FAILED
    migrate_status_t status = migrate_manager_get_status(mgr, task->task_id);
    EXPECT_EQ(status, MIGRATE_STATUS_FAILED);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, CancelNullManager) {
    EXPECT_EQ(migrate_manager_cancel(NULL, 1), -1);
}

TEST_F(MigrateManagerTest, CancelNonExistentTask) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    EXPECT_EQ(migrate_manager_cancel(mgr, 9999), -1);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, ExecuteNullParams) {
    EXPECT_EQ(migrate_manager_execute(NULL, NULL), -1);
}

TEST_F(MigrateManagerTest, ExecuteNonExistentTask) {
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    migrate_task_t fake_task;
    memset(&fake_task, 0, sizeof(fake_task));
    fake_task.task_id = 9999;
    fake_task.source_shard = 0;
    fake_task.target_shard = 1;
    fake_task.strategy = MIGRATE_INCREMENTAL;

    // 执行不存在的任务应该失败
    EXPECT_EQ(migrate_manager_execute(mgr, &fake_task), -1);

    migrate_manager_destroy(mgr);
}

TEST_F(MigrateManagerTest, GetShardPath) {
    char path[256];

    // 正常情况
    EXPECT_EQ(migrate_get_shard_path(0, path, sizeof(path)), 0);
    EXPECT_NE(strstr(path, "shard_0.db"), nullptr);

    EXPECT_EQ(migrate_get_shard_path(5, path, sizeof(path)), 0);
    EXPECT_NE(strstr(path, "shard_5.db"), nullptr);

    // 边界情况
    EXPECT_EQ(migrate_get_shard_path(0, NULL, 0), -1);
    EXPECT_EQ(migrate_get_shard_path(0, path, 0), -1);
}

/* ============================================================
 * 集成测试
 * ============================================================ */

class ShardingBalanceIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        shard_config_t config;
        memset(&config, 0, sizeof(config));
        config.strategy = SHARD_HASH;
        config.key_type = SHARD_KEY_STRING;
        config.num_shards = 4;
        config.replication_factor = 1;
        config.consistent_hashing = false;

        router = shard_router_create(&config);

        for (int i = 0; i < 4; i++) {
            shard_info_t shard;
            memset(&shard, 0, sizeof(shard));
            shard.shard_id = i;
            shard.host = "localhost";
            shard.port = (short)(5432 + i);
            shard.is_primary = true;
            shard_router_add(router, &shard);
        }
    }

    void TearDown() override {
        if (router) {
            shard_router_destroy(router);
        }
    }

    shard_router_t *router = nullptr;
};

TEST_F(ShardingBalanceIntegrationTest, FullWorkflow) {
    // 1. 创建配置
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);
    cfg->strategy = MIGRATE_VIRTUAL_NODE;

    // 2. 创建协调器
    shard_coordinator_t *coord = shard_coordinator_create(cfg, router);
    ASSERT_NE(coord, nullptr);

    // 3. 创建迁移管理器
    migrate_manager_t *mgr = migrate_manager_create(router);
    ASSERT_NE(mgr, nullptr);

    // 4. 创建负载收集器并添加负载数据
    load_collector_t *collector = load_collector_create(16);
    ASSERT_NE(collector, nullptr);

    for (int i = 0; i < 4; i++) {
        shard_load_t load;
        memset(&load, 0, sizeof(load));
        load.shard_id = i;
        load.row_count = 1000 * (uint64_t)(i + 1);
        load.qps = 50.0;
        load.latency_ms = 10.0;
        load_collector_update(collector, &load);
    }

    // 5. 计算倾斜度
    double skew = load_collector_calculate_skew(collector);
    EXPECT_GT(skew, 1.0);  // 应该有倾斜

    // 6. 创建迁移任务
    migrate_task_t *task = migrate_manager_create_vnode(mgr, 0, 1, 10);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->status, MIGRATE_STATUS_PENDING);

    // 7. 取消任务
    EXPECT_EQ(migrate_manager_cancel(mgr, task->task_id), 0);
    EXPECT_EQ(migrate_manager_get_status(mgr, task->task_id), MIGRATE_STATUS_FAILED);

    // 8. 清理
    load_collector_destroy(collector);
    migrate_manager_destroy(mgr);
    shard_coordinator_destroy(coord);
    shard_balance_config_destroy(cfg);
}

TEST_F(ShardingBalanceIntegrationTest, StressTest) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    migrate_manager_t *mgr = migrate_manager_create(router);

    // 创建大量迁移任务
    const int num_tasks = 100;
    std::vector<migrate_task_t*> tasks;

    for (int i = 0; i < num_tasks; i++) {
        migrate_task_t *task;
        if (i % 2 == 0) {
            task = migrate_manager_create_incremental(mgr, i % 4, (i + 1) % 4, "key", 3);
        } else {
            task = migrate_manager_create_vnode(mgr, i % 4, (i + 1) % 4, i);
        }
        if (task) {
            tasks.push_back(task);
        }
    }

    // 验证任务数量
    EXPECT_GE(tasks.size(), 90u);  // 允许一些失败

    // 清理
    for (auto task : tasks) {
        if (task->status == MIGRATE_STATUS_PENDING) {
            migrate_manager_cancel(mgr, task->task_id);
        }
    }

    migrate_manager_destroy(mgr);
    shard_balance_config_destroy(cfg);
}

TEST_F(ShardingBalanceIntegrationTest, RouterWorkflow) {
    // 测试路由器基本工作流程
    shard_config_t config;
    memset(&config, 0, sizeof(config));
    config.strategy = SHARD_HASH;
    config.key_type = SHARD_KEY_STRING;
    config.num_shards = 4;
    config.replication_factor = 1;

    shard_router_t *test_router = shard_router_create(&config);
    ASSERT_NE(test_router, nullptr);

    // 添加分片
    for (int i = 0; i < 4; i++) {
        shard_info_t shard;
        memset(&shard, 0, sizeof(shard));
        shard.shard_id = i;
        shard.host = "localhost";
        shard.port = (short)(5432 + i);
        shard.is_primary = true;
        EXPECT_EQ(shard_router_add(test_router, &shard), 0);
    }

    EXPECT_EQ(shard_count(test_router), 4);

    // 测试路由
    const char *key = "test_key_123";
    int shard_id = shard_route(test_router, key, strlen(key));
    EXPECT_GE(shard_id, 0);
    EXPECT_LT(shard_id, 4);

    // 相同键应该路由到相同分片
    EXPECT_EQ(shard_route(test_router, key, strlen(key)), shard_id);

    shard_router_destroy(test_router);
}
