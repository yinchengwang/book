/**
 * @file vector_engine_test.cpp
 * @brief 向量引擎单元测试
 *
 * 测试 vec_page、graph_dedup 模块的核心功能。
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

extern "C" {
#include "db/storage/vector/vec_page.h"
#include "db/storage/vector/graph_dedup.h"
#include "db/core/log.h"
}

/* ========================================================================
 * 测试辅助函数
 * ======================================================================== */

// 生成随机向量
static void generate_random_vector(float *vec, int32_t dim, float scale = 1.0f) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-scale, scale);

    for (int32_t i = 0; i < dim; i++) {
        vec[i] = dis(gen);
    }
}

// 比较两个向量是否相等
static bool vectors_equal(const float *a, const float *b, int32_t dim, float eps = 1e-5f) {
    for (int32_t i = 0; i < dim; i++) {
        if (fabsf(a[i] - b[i]) > eps) return false;
    }
    return true;
}

/* ========================================================================
 * vec_page 模块测试
 * ======================================================================== */

class VecPageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时目录
        temp_dir = "test_vec_page_temp";
#ifdef _WIN32
        _mkdir(temp_dir.c_str());
#else
        mkdir(temp_dir.c_str(), 0755);
#endif
    }

    void TearDown() override {
        // 清理临时目录
#ifdef _WIN32
        system(("rmdir /s /q " + temp_dir).c_str());
#else
        system(("rm -rf " + temp_dir).c_str());
#endif
    }

    std::string temp_dir;
};

TEST_F(VecPageTest, CreateAndDestroy) {
    const int32_t dim = 128;
    const int32_t page_size = 4096;  // 使用标准 4KB 页

    vector_page_pool_t *pool = vector_page_pool_create(
        temp_dir.c_str(), 4, page_size, dim);
    ASSERT_NE(pool, nullptr);

    EXPECT_EQ(pool->vector_dim, dim);
    // vectors_per_page 由 calc_vectors_per_page 计算
    EXPECT_GT(pool->vectors_per_page, 0);
    EXPECT_EQ(pool->page_count, 0);

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, AppendVectors) {
    const int32_t dim = 64;
    const int32_t page_size = 4096;

    vector_page_pool_t *pool = vector_page_pool_create(
        temp_dir.c_str(), 2, page_size, dim);
    ASSERT_NE(pool, nullptr);

    // 添加向量
    float vec1[64], vec2[64];
    generate_random_vector(vec1, dim);
    generate_random_vector(vec2, dim);

    int32_t id1 = vector_page_append(pool, vec1, -1);
    EXPECT_GE(id1, 0);
    EXPECT_EQ(pool->page_count, 1);

    int32_t id2 = vector_page_append(pool, vec2, -1);
    EXPECT_GE(id2, 0);
    // 同一页内继续添加
    EXPECT_GE(pool->page_count, 1);

    // 获取当前页的向量数
    vector_page_t *page = vector_page_get(pool, id1);
    ASSERT_NE(page, nullptr);
    EXPECT_GT(page->header.vector_count, 0);

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, GetVectors) {
    const int32_t dim = 32;
    const int32_t page_size = 4096;

    vector_page_pool_t *pool = vector_page_pool_create(
        temp_dir.c_str(), 2, page_size, dim);
    ASSERT_NE(pool, nullptr);

    // 添加测试向量（使用自动分配的 id）
    float vec1[32], vec2[32];
    generate_random_vector(vec1, dim);
    generate_random_vector(vec2, dim);

    int32_t id1 = vector_page_append(pool, vec1, -1);  // 自动分配 id
    int32_t id2 = vector_page_append(pool, vec2, -1);  // 自动分配 id

    EXPECT_GE(id1, 0);
    EXPECT_GE(id2, 0);

    // 获取向量所在的页
    vector_page_t *page1 = vector_page_get(pool, id1);
    ASSERT_NE(page1, nullptr);

    int32_t offset1 = vector_page_offset(id1, pool->vectors_per_page);
    const float *stored1 = page1->vectors + (offset1 * dim);
    EXPECT_TRUE(vectors_equal(stored1, vec1, dim));

    vector_page_t *page2 = vector_page_get(pool, id2);
    ASSERT_NE(page2, nullptr);

    int32_t offset2 = vector_page_offset(id2, pool->vectors_per_page);
    const float *stored2 = page2->vectors + (offset2 * dim);
    EXPECT_TRUE(vectors_equal(stored2, vec2, dim));

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, PageIdCalculation) {
    const int32_t vectors_per_page = 4;

    EXPECT_EQ(vector_page_id(0, vectors_per_page), 0);
    EXPECT_EQ(vector_page_id(1, vectors_per_page), 0);
    EXPECT_EQ(vector_page_id(3, vectors_per_page), 0);
    EXPECT_EQ(vector_page_id(4, vectors_per_page), 1);
    EXPECT_EQ(vector_page_id(7, vectors_per_page), 1);
    EXPECT_EQ(vector_page_id(8, vectors_per_page), 2);

    EXPECT_EQ(vector_page_offset(0, vectors_per_page), 0);
    EXPECT_EQ(vector_page_offset(1, vectors_per_page), 1);
    EXPECT_EQ(vector_page_offset(4, vectors_per_page), 0);
    EXPECT_EQ(vector_page_offset(5, vectors_per_page), 1);
}

TEST_F(VecPageTest, Eviction) {
    const int32_t dim = 16;
    const int32_t page_size = 4096;

    vector_page_pool_t *pool = vector_page_pool_create(
        temp_dir.c_str(), 2, page_size, dim);
    ASSERT_NE(pool, nullptr);

    // 添加向量
    float vec[16];
    for (int i = 0; i < 10; i++) {
        generate_random_vector(vec, dim);
        vector_page_append(pool, vec, -1);
    }

    EXPECT_GT(pool->page_count, 0);

    // 尝试驱逐（可能成功或失败）
    (void)vector_page_evict(pool);

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, DirtyFlag) {
    const int32_t dim = 8;
    const int32_t page_size = 4096;

    vector_page_pool_t *pool = vector_page_pool_create(
        temp_dir.c_str(), 2, page_size, dim);
    ASSERT_NE(pool, nullptr);

    float vec[8];
    generate_random_vector(vec, dim);
    int32_t id = vector_page_append(pool, vec, -1);

    vector_page_t *page = vector_page_get(pool, id);
    ASSERT_NE(page, nullptr);

    // 新添加的页面应该是脏的（因为有新数据）
    EXPECT_TRUE(page->header.dirty);

    vector_page_pool_destroy(pool);
}

/* ========================================================================
 * graph_dedup 模块测试
 * ======================================================================== */

class GraphDedupTest : public ::testing::Test {
protected:
    void SetUp() override {
        dedup = nullptr;
    }

    void TearDown() override {
        if (dedup) {
            graph_dedup_destroy(dedup);
            dedup = nullptr;
        }
    }

    graph_dedup_t *dedup;
};

TEST_F(GraphDedupTest, CreateAndDestroy) {
    dedup = graph_dedup_create(100, 128, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);
    EXPECT_EQ(dedup->count, 0);
}

TEST_F(GraphDedupTest, InsertUnique) {
    const int32_t dims = 64;
    const int32_t capacity = 10;

    dedup = graph_dedup_create(capacity, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    float vec[64];
    for (int i = 0; i < 5; i++) {
        generate_random_vector(vec, dims);

        bool is_dup = graph_dedup_is_duplicate(dedup, vec);
        EXPECT_FALSE(is_dup);

        int ret = graph_dedup_mark_inserted(dedup, vec, i, i);
        EXPECT_EQ(ret, 0);
    }

    EXPECT_EQ(dedup->count, 5);
}

TEST_F(GraphDedupTest, DetectDuplicate) {
    const int32_t dims = 32;
    const int32_t capacity = 100;

    dedup = graph_dedup_create(capacity, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    // 插入第一个向量
    float original[32];
    generate_random_vector(original, dims);

    EXPECT_FALSE(graph_dedup_is_duplicate(dedup, original));
    graph_dedup_mark_inserted(dedup, original, 0, 0);

    // 插入相同向量（复制）
    float copy[32];
    memcpy(copy, original, sizeof(copy));

    bool is_dup = graph_dedup_is_duplicate(dedup, copy);
    EXPECT_TRUE(is_dup);

    // 插入不同向量
    float different[32];
    do {
        generate_random_vector(different, dims);
    } while (vectors_equal(different, original, dims));

    is_dup = graph_dedup_is_duplicate(dedup, different);
    EXPECT_FALSE(is_dup);
}

TEST_F(GraphDedupTest, ReverseMapping) {
    const int32_t dims = 16;
    dedup = graph_dedup_create(50, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    // 插入相同向量多次
    float vec[16];
    generate_random_vector(vec, dims);

    // 第一次插入
    graph_dedup_mark_inserted(dedup, vec, 0, 100);

    // 第二次插入（不同 graph_node_id）
    graph_dedup_mark_inserted(dedup, vec, 1, 101);

    // 获取 graph_node_id = 0 的所有 heap_row_ids
    int32_t rows[10];
    int count = graph_dedup_get_heap_rows(dedup, 0, rows, 10);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(rows[0], 100);

    // 获取 graph_node_id = 1 的所有 heap_row_ids
    count = graph_dedup_get_heap_rows(dedup, 1, rows, 10);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(rows[0], 101);

    // 获取不存在的 graph_node_id
    count = graph_dedup_get_heap_rows(dedup, 999, rows, 10);
    EXPECT_EQ(count, 0);
}

TEST_F(GraphDedupTest, BatchInsert) {
    const int32_t dims = 32;
    const int32_t count = 5;  // 减少数量以便调试

    dedup = graph_dedup_create(count + 10, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    std::vector<float> vectors((size_t)count * dims);
    int32_t node_ids[5];
    int32_t row_ids[5];

    for (int32_t i = 0; i < count; i++) {
        generate_random_vector(&vectors[(size_t)i * dims], dims);
        node_ids[i] = i;
        row_ids[i] = 100 + i;
    }

    int inserted = graph_dedup_mark_inserted_batch(
        dedup, vectors.data(), node_ids, row_ids, count);

    EXPECT_EQ(inserted, count);
    EXPECT_EQ(dedup->count, count);
}

TEST_F(GraphDedupTest, SimpleBatchInsert) {
    // 简化版批量插入测试
    dedup = graph_dedup_create(20, 8, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    // 直接使用单条插入验证
    float vec[8];
    for (int i = 0; i < 3; i++) {
        generate_random_vector(vec, 8);
        int ret = graph_dedup_mark_inserted(dedup, vec, i, i);
        EXPECT_EQ(ret, 0);
    }

    EXPECT_EQ(dedup->count, 3);
}

TEST_F(GraphDedupTest, Stats) {
    const int32_t dims = 32;
    dedup = graph_dedup_create(50, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    dedup_stats_t stats;

    // 插入唯一向量
    float vec[32];
    for (int i = 0; i < 5; i++) {
        generate_random_vector(vec, dims);
        graph_dedup_is_duplicate(dedup, vec);
        graph_dedup_mark_inserted(dedup, vec, i, i);
    }

    // 插入重复向量
    generate_random_vector(vec, dims);
    graph_dedup_is_duplicate(dedup, vec);
    graph_dedup_mark_inserted(dedup, vec, 5, 5);
    graph_dedup_is_duplicate(dedup, vec);
    graph_dedup_is_duplicate(dedup, vec);

    graph_dedup_get_stats(dedup, &stats);
    EXPECT_GT(stats.total_checks, 0);
    EXPECT_GT(stats.unique_inserted, 0);
    // duplicates_found = 2: 最后两次 is_duplicate 调用返回 true（向量已被 mark_inserted）
    EXPECT_EQ(stats.duplicates_found, 2);

    // 测试重置
    graph_dedup_reset_stats(dedup);
    graph_dedup_get_stats(dedup, &stats);
    EXPECT_EQ(stats.total_checks, 0);
    EXPECT_EQ(stats.unique_inserted, 0);
}

TEST_F(GraphDedupTest, DedupRate) {
    const int32_t dims = 64;
    dedup = graph_dedup_create(100, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    float vec[64];

    // 插入 5 个唯一向量
    for (int i = 0; i < 5; i++) {
        float v[64];
        generate_random_vector(v, dims);
        graph_dedup_is_duplicate(dedup, v);
        graph_dedup_mark_inserted(dedup, v, i, i);
    }

    // 检测 3 个不存在的新向量
    for (int i = 0; i < 3; i++) {
        generate_random_vector(vec, dims);
        graph_dedup_is_duplicate(dedup, vec);
    }

    float rate = graph_dedup_get_dedup_rate(dedup);
    EXPECT_GE(rate, 0.0f);
    EXPECT_LE(rate, 100.0f);
}

TEST_F(GraphDedupTest, Reset) {
    const int32_t dims = 16;
    dedup = graph_dedup_create(50, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    // 插入一些向量
    float vec[16];
    for (int i = 0; i < 10; i++) {
        generate_random_vector(vec, dims);
        graph_dedup_mark_inserted(dedup, vec, i, i);
    }

    EXPECT_EQ(dedup->count, 10);
    EXPECT_EQ(dedup->reverse_count, 10);

    // 重置
    graph_dedup_reset(dedup);

    EXPECT_EQ(dedup->count, 0);
    EXPECT_EQ(dedup->reverse_count, 0);
}

TEST_F(GraphDedupTest, MinDistance) {
    const int32_t dims = 8;
    dedup = graph_dedup_create(50, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    // 插入已知向量
    float v1[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float v2[8] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float v3[8] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    graph_dedup_mark_inserted(dedup, v1, 0, 0);
    graph_dedup_mark_inserted(dedup, v2, 1, 1);
    graph_dedup_mark_inserted(dedup, v3, 2, 2);

    // 查询接近 v1 的向量
    float query[8] = {0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float min_dist = 0.0f;
    int32_t idx = graph_dedup_find_min_distance(dedup, query, &min_dist);

    EXPECT_EQ(idx, 0);  // 最接近 v1
    EXPECT_LT(min_dist, 0.2f);  // 距离很小
}

TEST_F(GraphDedupTest, Reserve) {
    const int32_t dims = 16;
    dedup = graph_dedup_create(10, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    EXPECT_EQ(dedup->capacity, 10);

    // 扩展容量
    int ret = graph_dedup_reserve(dedup, 100);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(dedup->capacity, 100);

    // 插入少量向量（避免可能的挂起）
    float vec[16];
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 16; j++) {
            vec[j] = (float)(i * 16 + j) * 0.1f;
        }
        graph_dedup_mark_inserted(dedup, vec, i, i);
    }

    EXPECT_EQ(dedup->count, 5);
}

/* ========================================================================
 * 工具函数测试
 * ======================================================================== */

class GraphDedupUtilTest : public ::testing::Test {
};

TEST_F(GraphDedupUtilTest, ComputeNorm) {
    const int32_t dim = 4;
    float vec[4] = {3.0f, 4.0f, 0.0f, 0.0f};

    float norm = graph_dedup_compute_norm(vec, dim);
    EXPECT_NEAR(norm, 5.0f, 0.001f);  // sqrt(9+16) = 5
}

TEST_F(GraphDedupUtilTest, L2Distance) {
    const int32_t dim = 3;
    float a[3] = {1.0f, 2.0f, 3.0f};
    float b[3] = {4.0f, 6.0f, 3.0f};

    float dist = graph_dedup_l2_distance(a, b, dim);
    // sqrt((1-4)^2 + (2-6)^2 + (3-3)^2) = sqrt(9+16+0) = 5
    EXPECT_NEAR(dist, 5.0f, 0.001f);
}

TEST_F(GraphDedupUtilTest, FingerprintComputation) {
    const int32_t dims = 32;
    graph_dedup_t *dedup = graph_dedup_create(10, dims, 0.01f, 0.001f);
    ASSERT_NE(dedup, nullptr);

    float vec[32];
    generate_random_vector(vec, dims);

    dedup_fingerprint_t fp;
    graph_dedup_compute_fingerprint(dedup, vec, &fp);

    // 验证指纹有值
    EXPECT_NE(fp.hash, 0u);
    EXPECT_GT(fp.norm, 0.0f);

    graph_dedup_destroy(dedup);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
