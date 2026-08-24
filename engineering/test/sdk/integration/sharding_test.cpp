/**
 * @file sharding_test.cpp
 * @brief 一致性哈希分片测试
 *
 * 覆盖：
 *   1. InitAndRoute - 初始化和路由测试
 *   2. RouteConsistency - 路由一致性测试
 *   3. ShardInfo - 分片信息查询测试
 *   4. Stats - 统计信息测试
 *   5. AddShard - 动态添加分片测试
 *   6. RemoveShard - 动态移除分片测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_sharding.h"
}

#include <cstdio>
#include <cstring>
#include <set>

namespace {

constexpr const char* kDbPath = "test_sharding.db";
constexpr const char* kShards = R"([
    {"id":1,"addr":"shard1:5432"},
    {"id":2,"addr":"shard2:5432"},
    {"id":3,"addr":"shard3:5432"}
])";

void cleanup_db() {
    std::remove(kDbPath);
}

}  // namespace

class ShardingTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;

    void SetUp() override {
        cleanup_db();
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
    }

    void TearDown() override {
        mmdb_sharding_stop(db_);
        if (db_) mmdb_close(db_);
        cleanup_db();
    }
};

/* 测试 1: 初始化和路由 */
TEST_F(ShardingTest, InitAndRoute) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);

    /* 路由一个键 */
    uint32_t shard_id = 0;
    ASSERT_EQ(mmdb_sharding_route(db_, "test_key", &shard_id), MMDB_OK);
    ASSERT_GT(shard_id, 0u);
    ASSERT_LE(shard_id, 3u);
}

/* 测试 2: 路由一致性（相同键总是路由到相同分片） */
TEST_F(ShardingTest, RouteConsistency) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);

    const char* key = "consistent_key";
    uint32_t first_shard = 0;

    /* 多次路由同一个键 */
    for (int i = 0; i < 10; i++) {
        uint32_t shard_id = 0;
        ASSERT_EQ(mmdb_sharding_route(db_, key, &shard_id), MMDB_OK);
        if (i == 0) {
            first_shard = shard_id;
        } else {
            ASSERT_EQ(shard_id, first_shard);
        }
    }
}

/* 测试 3: 分布均匀性（1000 个键分布到 3 个分片） */
TEST_F(ShardingTest, DistributionUniformity) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);

    uint32_t counts[4] = {0};  /* 分片 ID 1-3 */
    const int total = 1000;

    for (int i = 0; i < total; i++) {
        char key[64];
        snprintf(key, sizeof(key), "key_%d", i);
        uint32_t shard_id = 0;
        ASSERT_EQ(mmdb_sharding_route(db_, key, &shard_id), MMDB_OK);
        counts[shard_id]++;
    }

    /* 验证每个分片至少有 20% 的键（理想值 33.3%） */
    for (int i = 1; i <= 3; i++) {
        double ratio = (double)counts[i] / total;
        ASSERT_GT(ratio, 0.2) << "分片 " << i << " 分布过少: " << ratio;
        ASSERT_LT(ratio, 0.5) << "分片 " << i << " 分布过多: " << ratio;
    }
}

/* 测试 4: 分片信息查询 */
TEST_F(ShardingTest, ShardInfo) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);

    mmdb_shard_info_t info;
    ASSERT_EQ(mmdb_sharding_info(db_, 1, &info), MMDB_OK);
    ASSERT_EQ(info.shard_id, 1u);
    ASSERT_STREQ(info.addr, "shard1:5432");
    ASSERT_TRUE(info.alive);

    /* 查询不存在的分片 */
    ASSERT_EQ(mmdb_sharding_info(db_, 99, &info), MMDB_ERR_NOT_FOUND);
}

/* 测试 5: 统计信息 */
TEST_F(ShardingTest, Stats) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);

    mmdb_sharding_stats_t stats;
    ASSERT_EQ(mmdb_sharding_stats(db_, &stats), MMDB_OK);
    ASSERT_EQ(stats.shard_count, 3u);
    ASSERT_EQ(stats.total_keys, 0u);
}

/* 测试 6: 动态添加分片 */
TEST_F(ShardingTest, AddShard) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);

    /* 添加新分片 */
    const char* new_shard = R"({"id":4,"addr":"shard4:5432"})";
    ASSERT_EQ(mmdb_sharding_add(db_, new_shard), MMDB_OK);

    /* 验证分片数量 */
    mmdb_sharding_stats_t stats;
    ASSERT_EQ(mmdb_sharding_stats(db_, &stats), MMDB_OK);
    ASSERT_EQ(stats.shard_count, 4u);

    /* 验证新分片可路由 */
    uint32_t shard_id = 0;
    ASSERT_EQ(mmdb_sharding_route(db_, "new_key", &shard_id), MMDB_OK);
}

/* 测试 7: 动态移除分片 */
TEST_F(ShardingTest, RemoveShard) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);

    /* 移除分片 3 */
    ASSERT_EQ(mmdb_sharding_remove(db_, 3), MMDB_OK);

    /* 验证分片数量 */
    mmdb_sharding_stats_t stats;
    ASSERT_EQ(mmdb_sharding_stats(db_, &stats), MMDB_OK);
    ASSERT_EQ(stats.shard_count, 2u);

    /* 验证分片 3 不存在 */
    mmdb_shard_info_t info;
    ASSERT_EQ(mmdb_sharding_info(db_, 3, &info), MMDB_ERR_NOT_FOUND);
}

/* 测试 8: 停止分片服务 */
TEST_F(ShardingTest, StopSharding) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);
    ASSERT_EQ(mmdb_sharding_stop(db_), MMDB_OK);

    /* 重复停止应成功 */
    ASSERT_EQ(mmdb_sharding_stop(db_), MMDB_OK);
}

/* 测试 9: 无效参数 */
TEST_F(ShardingTest, InvalidParams) {
    /* NULL 参数 */
    ASSERT_EQ(mmdb_sharding_init(nullptr, kShards), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_sharding_init(db_, nullptr), MMDB_ERR_INVALID);

    uint32_t shard_id = 0;
    ASSERT_EQ(mmdb_sharding_route(nullptr, "key", &shard_id), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_sharding_route(db_, nullptr, &shard_id), MMDB_ERR_INVALID);
    ASSERT_EQ(mmdb_sharding_route(db_, "key", nullptr), MMDB_ERR_INVALID);
}

/* 测试 10: 重复初始化 */
TEST_F(ShardingTest, DoubleInit) {
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_OK);
    ASSERT_EQ(mmdb_sharding_init(db_, kShards), MMDB_ERR_INTERNAL);
}
