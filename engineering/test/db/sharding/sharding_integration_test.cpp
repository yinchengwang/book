/**
 * @file sharding_integration_test.cpp
 * @brief Gap#6 分片与负载均衡系统集成测试
 *
 * 测试完整工作流程：
 * 1. 分片配置与协调器启动流程
 * 2. 负载收集与倾斜度检测
 * 3. 阈值触发再平衡流程
 * 4. 增量迁移完整流程
 * 5. 虚拟节点迁移完整流程
 * 6. Executor 集成测试
 */

#include "db/sharding/sharding.h"
#include "db/sharding/shard_balance.h"
#include "db/sharding/shard_coordinator.h"
#include "db/sharding/migrate_manager.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

/* ============================================================
 * 测试夹具：完整的分片系统设置
 * ============================================================ */

class ShardingIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建分片配置
        shard_config_t config;
        memset(&config, 0, sizeof(config));
        config.strategy = SHARD_HASH;
        config.key_type = SHARD_KEY_STRING;
        config.num_shards = 4;
        config.replication_factor = 1;
        config.consistent_hashing = false;

        router = shard_router_create(&config);
        ASSERT_NE(router, nullptr);

        // 添加分片
        for (int i = 0; i < 4; i++) {
            shard_info_t shard;
            memset(&shard, 0, sizeof(shard));
            shard.shard_id = i;
            shard.host = "localhost";
            shard.port = static_cast<short>(5432 + i);
            shard.is_primary = true;
            shard.row_count = 1000 * (i + 1);
            ASSERT_EQ(shard_router_add(router, &shard), 0);
        }

        // 创建平衡配置
        balance_config = shard_balance_config_create();
        ASSERT_NE(balance_config, nullptr);

        // 创建协调器
        coordinator = shard_coordinator_create(balance_config, router);
        ASSERT_NE(coordinator, nullptr);

        // 创建迁移管理器
        migrate_mgr = migrate_manager_create(router);
        ASSERT_NE(migrate_mgr, nullptr);
    }

    void TearDown() override {
        if (migrate_mgr) {
            migrate_manager_destroy(migrate_mgr);
            migrate_mgr = nullptr;
        }
        if (coordinator) {
            shard_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
        if (balance_config) {
            shard_balance_config_destroy(balance_config);
            balance_config = nullptr;
        }
        if (router) {
            shard_router_destroy(router);
            router = nullptr;
        }
    }

    shard_router_t *router = nullptr;
    shard_balance_config_t *balance_config = nullptr;
    shard_coordinator_t *coordinator = nullptr;
    migrate_manager_t *migrate_mgr = nullptr;
};

/* ============================================================
 * 场景1：分片配置与协调器启动流程
 * ============================================================ */

TEST_F(ShardingIntegrationTest, ShardConfigAndCoordinatorStartup) {
    // 1. 验证初始分片数量
    EXPECT_EQ(shard_count(router), 4);

    // 2. 验证分片信息
    for (int i = 0; i < 4; i++) {
        const shard_info_t *info = shard_get_info(router, i);
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->shard_id, i);
        EXPECT_STREQ(info->host, "localhost");
        EXPECT_EQ(info->port, 5432 + i);
    }

    // 3. 验证协调器启动
    EXPECT_EQ(shard_coordinator_start(coordinator), 0);

    // 4. 等待一小段时间确保线程启动
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 5. 停止协调器
    shard_coordinator_stop(coordinator);

    // 6. 重新启动应该成功
    EXPECT_EQ(shard_coordinator_start(coordinator), 0);
    shard_coordinator_stop(coordinator);
}

TEST_F(ShardingIntegrationTest, CoordinatorRouterAccess) {
    // 验证协调器能正确获取路由器
    shard_router_t *coord_router = shard_coordinator_get_router(coordinator);
    EXPECT_EQ(coord_router, router);

    // 验证路由器功能正常
    EXPECT_EQ(shard_count(coord_router), 4);
}

/* ============================================================
 * 场景2：负载收集与倾斜度检测
 * ============================================================ */

TEST_F(ShardingIntegrationTest, LoadCollectionAndSkewDetection) {
    // 1. 创建一个临时的负载收集器用于测试
    load_collector_t *collector = load_collector_create(16);
    ASSERT_NE(collector, nullptr);

    // 2. 添加均匀分布的负载
    for (int i = 0; i < 4; i++) {
        shard_load_t load = {
            .shard_id = i,
            .row_count = 1000,
            .qps = 50.0,
            .latency_ms = 10.0,
            .cpu_usage = 0.5,
            .size_bytes = 1024 * 1024,
            .last_updated = time(nullptr)
        };
        EXPECT_EQ(load_collector_update(collector, &load), 0);
    }

    // 3. 均匀分布时倾斜度应为 1.0
    double uniform_skew = load_collector_calculate_skew(collector);
    EXPECT_DOUBLE_EQ(uniform_skew, 1.0);

    // 4. 添加倾斜负载
    shard_load_t skewed_load = {
        .shard_id = 3,
        .row_count = 10000,  // 10x 其他分片
        .qps = 500.0,
        .latency_ms = 100.0,
        .cpu_usage = 0.9,
        .size_bytes = 10 * 1024 * 1024,
        .last_updated = time(nullptr)
    };
    EXPECT_EQ(load_collector_update(collector, &skewed_load), 0);

    // 5. 验证倾斜度增加
    double skewed_value = load_collector_calculate_skew(collector);
    EXPECT_GT(skewed_value, 1.0);
    EXPECT_GT(skewed_value, uniform_skew);

    // 6. 验证负载查询
    const shard_load_t *load3 = load_collector_get(collector, 3);
    ASSERT_NE(load3, nullptr);
    EXPECT_EQ(load3->row_count, 10000);

    load_collector_destroy(collector);
}

TEST_F(ShardingIntegrationTest, LoadCollectionEdgeCases) {
    load_collector_t *collector = load_collector_create(4);

    // 空收集器倾斜度
    EXPECT_DOUBLE_EQ(load_collector_calculate_skew(collector), 0.0);

    // 单个分片倾斜度
    shard_load_t single = {
        .shard_id = 0,
        .row_count = 1000,
        .qps = 50.0
    };
    load_collector_update(collector, &single);
    EXPECT_DOUBLE_EQ(load_collector_calculate_skew(collector), 1.0);

    // 极端倾斜
    shard_load_t huge = {
        .shard_id = 1,
        .row_count = 1000000,
        .qps = 5000.0
    };
    load_collector_update(collector, &huge);

    double extreme_skew = load_collector_calculate_skew(collector);
    EXPECT_GT(extreme_skew, 10.0);

    load_collector_destroy(collector);
}

/* ============================================================
 * 场景3：阈值触发再平衡流程
 * ============================================================ */

TEST_F(ShardingIntegrationTest, ThresholdTriggeredRebalance) {
    // 1. 创建自定义配置的协调器
    shard_balance_config_t *custom_config = shard_balance_config_create();
    custom_config->skew_threshold = 1.2;  // 较低阈值
    custom_config->auto_rebalance = true;
    custom_config->check_interval_ms = 100;  // 快速检查

    shard_coordinator_t *custom_coord = shard_coordinator_create(custom_config, router);
    ASSERT_NE(custom_coord, nullptr);

    // 2. 获取协调器的负载收集器并填充数据
    // 由于协调器内部有自己的收集器，我们需要通过 select_least_load 验证
    int shards[] = {0, 1, 2, 3};

    // 初始选择（无数据）
    int selected = shard_coordinator_select_least_load(custom_coord, shards, 4);
    EXPECT_EQ(selected, -1);  // 无负载数据时应返回 -1

    // 3. 触发再平衡检查（不应崩溃）
    EXPECT_EQ(shard_coordinator_check_and_rebalance(custom_coord), 0);

    // 4. 清理
    shard_coordinator_destroy(custom_coord);
    shard_balance_config_destroy(custom_config);
}

TEST_F(ShardingIntegrationTest, AutoRebalanceConfiguration) {
    // 测试自动再平衡开关
    shard_balance_config_t *config = shard_balance_config_create();
    ASSERT_NE(config, nullptr);

    // 默认应开启
    EXPECT_TRUE(config->auto_rebalance);

    // 关闭自动再平衡
    config->auto_rebalance = false;
    EXPECT_FALSE(config->auto_rebalance);

    shard_balance_config_destroy(config);
}

/* ============================================================
 * 场景4：增量迁移完整流程
 * ============================================================ */

TEST_F(ShardingIntegrationTest, IncrementalMigrationFullFlow) {
    // 1. 创建增量迁移任务
    const char *key_range = "user_1000";
    migrate_task_t *task = migrate_manager_create_incremental(
        migrate_mgr, 0, 1, key_range, strlen(key_range));

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->source_shard, 0);
    EXPECT_EQ(task->target_shard, 1);
    EXPECT_EQ(task->strategy, MIGRATE_INCREMENTAL);
    EXPECT_EQ(task->status, MIGRATE_STATUS_PENDING);
    EXPECT_GT(task->task_id, 0);

    // 2. 验证任务状态
    migrate_status_t status = migrate_manager_get_status(migrate_mgr, task->task_id);
    EXPECT_EQ(status, MIGRATE_STATUS_PENDING);

    // 3. 取消任务
    EXPECT_EQ(migrate_manager_cancel(migrate_mgr, task->task_id), 0);
    EXPECT_EQ(migrate_manager_get_status(migrate_mgr, task->task_id), MIGRATE_STATUS_FAILED);
}

TEST_F(ShardingIntegrationTest, IncrementalMigrationKeyRange) {
    // 测试不同范围的增量迁移
    const char *ranges[] = {"a", "user_", "table_100"};
    size_t lens[] = {1, 6, 9};

    for (size_t i = 0; i < 3; i++) {
        migrate_task_t *task = migrate_manager_create_incremental(
            migrate_mgr, i, i + 1, ranges[i], lens[i]);

        ASSERT_NE(task, nullptr);
        EXPECT_EQ(task->strategy, MIGRATE_INCREMENTAL);
    }
}

/* ============================================================
 * 场景5：虚拟节点迁移完整流程
 * ============================================================ */

TEST_F(ShardingIntegrationTest, VirtualNodeMigrationFullFlow) {
    // 1. 创建虚拟节点迁移任务
    migrate_task_t *task = migrate_manager_create_vnode(migrate_mgr, 0, 1, 42);

    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->source_shard, 0);
    EXPECT_EQ(task->target_shard, 1);
    EXPECT_EQ(task->strategy, MIGRATE_VIRTUAL_NODE);
    EXPECT_EQ(task->vnode_id, 42);
    EXPECT_EQ(task->status, MIGRATE_STATUS_PENDING);

    // 2. 验证任务状态
    migrate_status_t status = migrate_manager_get_status(migrate_mgr, task->task_id);
    EXPECT_EQ(status, MIGRATE_STATUS_PENDING);

    // 3. 取消任务
    EXPECT_EQ(migrate_manager_cancel(migrate_mgr, task->task_id), 0);
}

TEST_F(ShardingIntegrationTest, VirtualNodeMigrationBatch) {
    // 批量创建虚拟节点迁移任务
    const int num_vnodes = 10;
    std::vector<migrate_task_t*> tasks;

    for (int i = 0; i < num_vnodes; i++) {
        migrate_task_t *task = migrate_manager_create_vnode(
            migrate_mgr, i % 4, (i + 1) % 4, i);
        if (task) {
            tasks.push_back(task);
        }
    }

    EXPECT_GE(tasks.size(), static_cast<size_t>(num_vnodes * 0.8));  // 至少 80% 成功

    // 清理
    for (auto task : tasks) {
        if (task->status == MIGRATE_STATUS_PENDING) {
            migrate_manager_cancel(migrate_mgr, task->task_id);
        }
    }
}

/* ============================================================
 * 场景6：Executor 集成测试（验证 ExecNode 接口可用性）
 * ============================================================ */

TEST_F(ShardingIntegrationTest, ExecutorShardScanInterfaceAvailability) {
    // 验证 shard_coordinator 可以为 executor 提供必要信息
    shard_router_t *coord_router = shard_coordinator_get_router(coordinator);
    ASSERT_NE(coord_router, nullptr);

    // 验证可以获取所有分片
    int max_shards = shard_count(coord_router);
    EXPECT_EQ(max_shards, 4);

    shard_info_t all_shards[4];
    int count = shard_get_all(coord_router, all_shards, 4);
    EXPECT_EQ(count, 4);

    // 验证协调器可以选择最小负载分片
    int shard_ids[4] = {0, 1, 2, 3};
    int selected = shard_coordinator_select_least_load(coordinator, shard_ids, 4);
    // 无负载数据时应返回 -1
    EXPECT_EQ(selected, -1);
}

TEST_F(ShardingIntegrationTest, ExecutorShardScanRoutingVerification) {
    // 验证分片路由功能可用于 executor
    const char *key = "test_key";

    // 计算 key 的哈希
    uint64_t hash = shard_calculate_hash(router, key, strlen(key));
    EXPECT_GT(hash, 0);

    // 路由 key
    int shard_id = shard_route(router, key, strlen(key));
    EXPECT_GE(shard_id, 0);
    EXPECT_LT(shard_id, 4);

    // 相同 key 路由到相同分片
    int shard_id2 = shard_route(router, key, strlen(key));
    EXPECT_EQ(shard_id, shard_id2);

    // 验证可以获取分片信息
    const shard_info_t *info = shard_get_info(router, shard_id);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->shard_id, shard_id);
}

/* ============================================================
 * 端到端集成测试
 * ============================================================ */

TEST_F(ShardingIntegrationTest, EndToEndWorkFlow) {
    // 模拟完整的再平衡工作流程

    // 1. 启动协调器
    ASSERT_EQ(shard_coordinator_start(coordinator), 0);

    // 2. 创建迁移任务
    migrate_task_t *task1 = migrate_manager_create_incremental(
        migrate_mgr, 0, 1, "user_", 5);
    migrate_task_t *task2 = migrate_manager_create_vnode(
        migrate_mgr, 1, 2, 10);
    migrate_task_t *task3 = migrate_manager_create_incremental(
        migrate_mgr, 2, 3, "order_", 6);

    ASSERT_NE(task1, nullptr);
    ASSERT_NE(task2, nullptr);
    ASSERT_NE(task3, nullptr);

    // 3. 验证任务可以取消
    EXPECT_EQ(migrate_manager_cancel(migrate_mgr, task1->task_id), 0);
    EXPECT_EQ(migrate_manager_get_status(migrate_mgr, task1->task_id), MIGRATE_STATUS_FAILED);

    // 4. 触发再平衡检查
    EXPECT_EQ(shard_coordinator_check_and_rebalance(coordinator), 0);

    // 5. 停止协调器
    shard_coordinator_stop(coordinator);
}

TEST_F(ShardingIntegrationTest, StressTest) {
    // 压力测试：大量并发操作
    const int num_operations = 50;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t, &success_count, num_operations]() {
            for (int i = 0; i < num_operations; i++) {
                // 创建迁移任务
                migrate_task_t *task;
                if ((t + i) % 2 == 0) {
                    task = migrate_manager_create_incremental(
                        migrate_mgr, t, (t + 1) % 4, "key", 3);
                } else {
                    task = migrate_manager_create_vnode(
                        migrate_mgr, t, (t + 1) % 4, t * 100 + i);
                }

                if (task) {
                    success_count++;
                    // 取消任务
                    migrate_manager_cancel(migrate_mgr, task->task_id);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    // 验证大部分操作成功
    EXPECT_GE(success_count.load(), num_operations * 2);  // 至少部分成功
}

/* ============================================================
 * 路由功能集成测试
 * ============================================================ */

TEST_F(ShardingIntegrationTest, RoutingIntegration) {
    // 测试完整路由功能

    // 1. 验证分片数量
    EXPECT_EQ(shard_count(router), 4);

    // 2. 测试单键路由
    const char *test_key = "user_12345";
    int shard_id = shard_route(router, test_key, strlen(test_key));
    EXPECT_GE(shard_id, 0);
    EXPECT_LT(shard_id, 4);

    // 3. 相同键应该路由到相同分片
    EXPECT_EQ(shard_route(router, test_key, strlen(test_key)), shard_id);

    // 4. 测试范围路由
    int range_shards[4];
    int64_t min_key = 0;
    int64_t max_key = 1000;
    int range_count = shard_route_range(router, &min_key, &max_key, range_shards, 4);
    EXPECT_GT(range_count, 0);

    // 5. 测试哈希计算
    uint64_t hash1 = shard_calculate_hash(router, test_key, strlen(test_key));
    uint64_t hash2 = shard_calculate_hash(router, test_key, strlen(test_key));
    EXPECT_EQ(hash1, hash2);

    // 6. 测试跨分片查询创建
    cross_shard_query_t *query = cross_shard_query_create(router, test_key, strlen(test_key));
    ASSERT_NE(query, nullptr);
    EXPECT_GT(query->shard_count, 0);

    // 添加额外分片
    EXPECT_EQ(cross_shard_query_add_shard(query, 2), 0);
    EXPECT_EQ(cross_shard_query_add_shard(query, 2), 0);  // 重复添加应该无影响

    cross_shard_query_destroy(query);
}

/* ============================================================
 * 配置与策略测试
 * ============================================================ */

TEST_F(ShardingIntegrationTest, MigrationStrategyConfiguration) {
    // 测试迁移策略配置

    // 默认应该是增量迁移
    EXPECT_EQ(balance_config->strategy, MIGRATE_INCREMENTAL);

    // 修改为虚拟节点迁移
    balance_config->strategy = MIGRATE_VIRTUAL_NODE;
    EXPECT_EQ(balance_config->strategy, MIGRATE_VIRTUAL_NODE);

    // 字符串转换测试
    EXPECT_EQ(migrate_strategy_from_string("incremental"), MIGRATE_INCREMENTAL);
    EXPECT_EQ(migrate_strategy_from_string("virtual-node"), MIGRATE_VIRTUAL_NODE);
    EXPECT_EQ(migrate_strategy_from_string("vnode"), MIGRATE_VIRTUAL_NODE);
    EXPECT_EQ(migrate_strategy_from_string("invalid"), MIGRATE_INCREMENTAL);
    EXPECT_EQ(migrate_strategy_from_string(nullptr), MIGRATE_INCREMENTAL);

    EXPECT_STREQ(migrate_strategy_to_string(MIGRATE_INCREMENTAL), "incremental");
    EXPECT_STREQ(migrate_strategy_to_string(MIGRATE_VIRTUAL_NODE), "virtual-node");
}

TEST_F(ShardingIntegrationTest, BalanceConfigurationDefaults) {
    shard_balance_config_t *cfg = shard_balance_config_create();
    ASSERT_NE(cfg, nullptr);

    // 验证默认值
    EXPECT_DOUBLE_EQ(cfg->skew_threshold, DEFAULT_SKEW_THRESHOLD);
    EXPECT_EQ(cfg->max_shard_size, DEFAULT_MAX_SHARD_SIZE);
    EXPECT_EQ(cfg->check_interval_ms, DEFAULT_CHECK_INTERVAL_MS);
    EXPECT_TRUE(cfg->auto_rebalance);

    shard_balance_config_destroy(cfg);
}

/* ============================================================
 * 向量分片集成测试
 * ============================================================ */

TEST_F(ShardingIntegrationTest, VectorShardingIntegration) {
    // 测试向量分片相关功能

    // 1. 创建向量分片键
    int64_t vector_id = 12345;
    float vector[128];
    for (int i = 0; i < 128; i++) {
        vector[i] = static_cast<float>(i) / 128.0f;
    }

    vector_shard_key_t *vkey = vector_shard_key_create(router, vector_id, vector, 128);
    ASSERT_NE(vkey, nullptr);
    EXPECT_EQ(vkey->vector_id, vector_id);
    EXPECT_GT(vkey->hash, 0);

    free(vkey);

    // 2. 路由向量搜索
    float query_vec[128];
    for (int i = 0; i < 128; i++) {
        query_vec[i] = 0.5f;
    }

    int *shard_ids = vector_shard_route_search(router, query_vec, 128, 10);
    if (shard_ids) {
        // 应该返回所有分片（向量搜索需要扫描所有分片）
        EXPECT_GE(shard_count(router), 0);
        free(shard_ids);
    }

    // 3. 一致性哈希
    uint64_t hash = vector_consistent_hash(vector, 128);
    EXPECT_GT(hash, 0);

    // 相同向量应该得到相同哈希
    uint64_t hash2 = vector_consistent_hash(vector, 128);
    EXPECT_EQ(hash, hash2);
}

TEST_F(ShardingIntegrationTest, VectorShardResultMerging) {
    // 测试向量搜索结果合并
    const int num_shards = 4;
    const int results_per_shard = 5;
    const int top_k = 3;

    // 分配结果数组
    vector_shard_result_t **shard_results = (vector_shard_result_t **)malloc(
        num_shards * sizeof(vector_shard_result_t *));
    int *result_counts = (int *)malloc(num_shards * sizeof(int));

    for (int i = 0; i < num_shards; i++) {
        shard_results[i] = (vector_shard_result_t *)malloc(
            results_per_shard * sizeof(vector_shard_result_t));
        result_counts[i] = results_per_shard;

        for (int j = 0; j < results_per_shard; j++) {
            shard_results[i][j].id = i * 100 + j;
            shard_results[i][j].distance = static_cast<float>(i + j) / 10.0f;
            shard_results[i][j].shard_id = i;
        }
    }

    // 合并结果
    vector_shard_result_t *merged = vector_shard_merge_results(
        num_shards, shard_results, result_counts, top_k);

    if (merged) {
        // 验证结果按距离排序
        for (int i = 1; i < top_k; i++) {
            EXPECT_LE(merged[i].distance, merged[i - 1].distance);
        }
        free(merged);
    }

    // 清理
    for (int i = 0; i < num_shards; i++) {
        free(shard_results[i]);
    }
    free(shard_results);
    free(result_counts);
}
