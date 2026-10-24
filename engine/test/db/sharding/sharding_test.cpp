/**
 * @file sharding_test.cpp
 * @brief 分片路由测试
 */
#include <gtest/gtest.h>
#include "db/sharding/sharding.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * 路由器创建/销毁测试
 * ============================================================ */

TEST(ShardingTest, CreateAndDestroy) {
    shard_config_t config = {
        .strategy = SHARD_HASH,
        .key_type = SHARD_KEY_STRING,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    shard_router_destroy(router);
}

TEST(ShardingTest, AddShard) {
    shard_config_t config = {
        .strategy = SHARD_HASH,
        .key_type = SHARD_KEY_STRING,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    shard_info_t shard = {
        .shard_id = 0,
        .shard_name = "shard0",
        .host = "localhost",
        .port = 5432,
        .is_primary = true
    };

    int rc = shard_router_add(router, &shard);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(shard_count(router), 1);

    shard_router_destroy(router);
}

TEST(ShardingTest, RemoveShard) {
    shard_config_t config = {
        .strategy = SHARD_HASH,
        .key_type = SHARD_KEY_STRING,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    shard_info_t shard = {
        .shard_id = 0,
        .shard_name = "shard0",
        .host = "localhost",
        .port = 5432,
        .is_primary = true
    };

    shard_router_add(router, &shard);
    EXPECT_EQ(shard_count(router), 1);

    shard_router_remove(router, 0);
    EXPECT_EQ(shard_count(router), 0);

    shard_router_destroy(router);
}

/* ============================================================
 * Hash 分片测试
 * ============================================================ */

TEST(ShardingTest, HashRouting) {
    shard_config_t config = {
        .strategy = SHARD_HASH,
        .key_type = SHARD_KEY_STRING,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    // 添加 3 个分片
    for (int i = 0; i < 3; i++) {
        shard_info_t shard = {
            .shard_id = i,
            .shard_name = NULL,
            .host = "localhost",
            .port = 5432 + i,
            .is_primary = true
        };
        shard_router_add(router, &shard);
    }

    // 测试字符串键路由
    const char *key1 = "user_123";
    int shard1 = shard_route(router, key1, strlen(key1));
    EXPECT_GE(shard1, 0);
    EXPECT_LT(shard1, 3);

    const char *key2 = "user_456";
    int shard2 = shard_route(router, key2, strlen(key2));
    EXPECT_GE(shard2, 0);
    EXPECT_LT(shard2, 3);

    const char *key3 = "user_789";
    int shard3 = shard_route(router, key3, strlen(key3));
    EXPECT_GE(shard3, 0);
    EXPECT_LT(shard3, 3);

    // 相同键应该路由到相同分片
    EXPECT_EQ(shard_route(router, key1, strlen(key1)), shard1);

    shard_router_destroy(router);
}

TEST(ShardingTest, HashDistribution) {
    shard_config_t config = {
        .strategy = SHARD_HASH,
        .key_type = SHARD_KEY_STRING,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    // 添加 3 个分片
    for (int i = 0; i < 3; i++) {
        shard_info_t shard = {
            .shard_id = i,
            .shard_name = NULL,
            .host = "localhost",
            .port = 5432 + i,
            .is_primary = true
        };
        shard_router_add(router, &shard);
    }

    // 统计分布
    int distribution[3] = {0, 0, 0};
    char key[32];

    for (int i = 0; i < 1000; i++) {
        snprintf(key, sizeof(key), "user_%d", i);
        int shard = shard_route(router, key, strlen(key));
        distribution[shard]++;
    }

    // 验证分布大致均匀（允许 20% 误差）
    for (int i = 0; i < 3; i++) {
        EXPECT_GE(distribution[i], 200);  // 至少 200 个
        EXPECT_LE(distribution[i], 600);  // 最多 600 个
    }

    shard_router_destroy(router);
}

/* ============================================================
 * Range 分片测试
 * ============================================================ */

TEST(ShardingTest, RangeRouting) {
    shard_config_t config = {
        .strategy = SHARD_RANGE,
        .key_type = SHARD_KEY_INT,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    // 添加 3 个分片（按范围划分）
    for (int i = 0; i < 3; i++) {
        shard_info_t shard = {
            .shard_id = i,
            .shard_name = NULL,
            .host = "localhost",
            .port = 5432 + i,
            .min_value = i * 1000,
            .max_value = (i + 1) * 1000 - 1,
            .is_primary = true
        };
        shard_router_add(router, &shard);
    }

    // 测试范围路由
    int64_t key1 = 500;
    EXPECT_EQ(shard_route(router, &key1, sizeof(key1)), 0);

    int64_t key2 = 1500;
    EXPECT_EQ(shard_route(router, &key2, sizeof(key2)), 1);

    int64_t key3 = 2500;
    EXPECT_EQ(shard_route(router, &key3, sizeof(key3)), 2);

    shard_router_destroy(router);
}

/* ============================================================
 * 边界条件测试
 * ============================================================ */

TEST(ShardingTest, NullRouter) {
    int shard = shard_route(NULL, "key", 3);
    EXPECT_EQ(shard, -1);
}

TEST(ShardingTest, NullKey) {
    shard_config_t config = {
        .strategy = SHARD_HASH,
        .key_type = SHARD_KEY_STRING,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    int shard = shard_route(router, NULL, 3);
    EXPECT_EQ(shard, -1);

    shard_router_destroy(router);
}

TEST(ShardingTest, EmptyRouter) {
    shard_config_t config = {
        .strategy = SHARD_HASH,
        .key_type = SHARD_KEY_STRING,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    // 没有添加任何分片
    int shard = shard_route(router, "key", 3);
    EXPECT_EQ(shard, -1);

    shard_router_destroy(router);
}

TEST(ShardingTest, HashCalculation) {
    shard_config_t config = {
        .strategy = SHARD_HASH,
        .key_type = SHARD_KEY_STRING,
        .num_shards = 3,
        .replication_factor = 1,
        .consistent_hashing = false
    };

    shard_router_t *router = shard_router_create(&config);
    ASSERT_NE(router, nullptr);

    const char *key = "test_key";
    uint64_t hash = shard_calculate_hash(router, key, strlen(key));

    // 哈希值应该大于 0
    EXPECT_GT(hash, 0);

    // 相同键的哈希值应该相同
    EXPECT_EQ(shard_calculate_hash(router, key, strlen(key)), hash);

    shard_router_destroy(router);
}
