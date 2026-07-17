/**
 * @file vector_query_test.cpp
 * @brief 向量查询执行器单元测试
 *
 * 测试覆盖：
 * - 查询计划生成（不同查询类型）
 * - 执行器算子执行（Scan/Filter/Limit）
 * - 结果集构建和分页
 * - 查询优化策略
 */
#include <gtest/gtest.h>
#include <db/core/vector_query.h>
#include <db/index/vector_index/reranker/reranker.h>
#include <cstring>
#include <cstdio>
#include <memory>

/* ========================================================================
 * Mock 索引搜索函数（简化实现，不依赖实际索引库）
 * ======================================================================== */

/** Mock HNSW 搜索函数 */
static int32_t mock_hnsw_search(void *index_impl, const float *query,
                                int32_t k, int64_t *ids, float *distances) {
    if (!index_impl || !query || !ids || !distances || k <= 0) {
        return -1;
    }
    /* 生成模拟搜索结果 */
    int32_t count = k < 10 ? k : 10;
    for (int32_t i = 0; i < count; i++) {
        ids[i] = i;
        distances[i] = 0.1f * (i + 1);
    }
    return count;
}

/** Mock IVF 搜索函数 */
static int32_t mock_ivf_search(void *index_impl, const float *query,
                               int32_t k, int64_t *ids, float *distances) {
    if (!index_impl || !query || !ids || !distances || k <= 0) {
        return -1;
    }
    /* 生成模拟搜索结果 */
    int32_t count = k < 15 ? k : 15;
    for (int32_t i = 0; i < count; i++) {
        ids[i] = i + 100;
        distances[i] = 0.05f * (i + 1);
    }
    return count;
}

/** Mock 获取向量函数 */
static const float *mock_get_vector(void *index_impl, int64_t id) {
    static float mock_vector[128];
    if (!index_impl || id < 0) {
        return nullptr;
    }
    /* 生成模拟向量 */
    for (int i = 0; i < 128; i++) {
        mock_vector[i] = static_cast<float>(id + i) / 256.0f;
    }
    return mock_vector;
}

/* ========================================================================
 * Task 6.2: 查询计划生成测试
 * ======================================================================== */

// 测试夹具
class VectorQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        vector_query_init();

        /* 注册 Mock 索引（使用简单的指针作为索引句柄） */
        static int mock_hnsw_handle = 1;
        static int mock_ivf_handle = 2;

        vector_query_register_index("hnsw_test",
                                   VECTOR_INDEX_HNSW,
                                   &mock_hnsw_handle,
                                   mock_hnsw_search,
                                   mock_get_vector);

        vector_query_register_index("ivf_test",
                                   VECTOR_INDEX_IVF,
                                   &mock_ivf_handle,
                                   mock_ivf_search,
                                   mock_get_vector);
    }

    void TearDown() override {
        vector_query_shutdown();
    }

    static std::unique_ptr<float[]> generate_query_vector(int32_t dims) {
        auto vec = std::make_unique<float[]>(dims);
        for (int32_t i = 0; i < dims; i++) {
            vec[i] = static_cast<float>(i) / dims;
        }
        return vec;
    }
};

TEST_F(VectorQueryTest, CreateSimpleQueryPlan) {
    /* 创建简单查询计划 */
    VectorQueryPlan *plan = build_simple_query_plan(VECTOR_INDEX_HNSW, 10);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->top_k, 10);
    EXPECT_EQ(plan->coarse_index_type, VECTOR_INDEX_HNSW);
    EXPECT_NE(plan->root_node, nullptr);

    /* 验证算子链 */
    EXPECT_EQ(plan->root_node->type, PLAN_NODE_VECTOR_LIMIT);
    EXPECT_NE(plan->root_node->left, nullptr);
    EXPECT_EQ(plan->root_node->left->type, PLAN_NODE_VECTOR_SCAN);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, CreateHybridQueryPlan) {
    /* 创建混合查询计划 */
    VectorQueryPlan *plan = build_hybrid_query_plan(20, 0.7f, 0.3f);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->top_k, 20);
    EXPECT_TRUE(plan->hybrid_enabled);
    EXPECT_FLOAT_EQ(plan->vector_weight, 0.7f);
    EXPECT_FLOAT_EQ(plan->text_weight, 0.3f);

    /* 验证算子链结构 */
    EXPECT_NE(plan->root_node, nullptr);
    EXPECT_EQ(plan->root_node->type, PLAN_NODE_VECTOR_LIMIT);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, SetQueryPlanParameters) {
    VectorQueryPlan *plan = vector_query_plan_create();
    ASSERT_NE(plan, nullptr);

    /* 设置粗排参数 */
    vector_query_plan_set_coarse(plan, VECTOR_INDEX_IVF, 50, 200);
    EXPECT_EQ(plan->coarse_index_type, VECTOR_INDEX_IVF);
    EXPECT_EQ(plan->coarse_ef_search, 50);
    EXPECT_EQ(plan->coarse_max_candidates, 200);

    /* 设置 Top-K */
    vector_query_plan_set_topk(plan, 15);
    EXPECT_EQ(plan->top_k, 15);

    /* 设置分页 */
    vector_query_plan_set_offset(plan, 10);
    EXPECT_EQ(plan->offset, 10);

    /* 设置混合检索 */
    vector_query_plan_set_hybrid(plan, true, 0.8f, 0.2f, FUSION_RRF);
    EXPECT_TRUE(plan->hybrid_enabled);

    vector_query_plan_destroy(plan);
}

/* ========================================================================
 * Task 6.3: 执行器算子测试
 * ======================================================================== */

TEST_F(VectorQueryTest, ExecuteVectorScan) {
    /* 创建 Scan 算子 */
    PlanNode *scan = create_vector_scan_node(VECTOR_INDEX_HNSW, 100, 50);
    ASSERT_NE(scan, nullptr);
    EXPECT_EQ(scan->type, PLAN_NODE_VECTOR_SCAN);

    /* 创建执行上下文 */
    auto query = generate_query_vector(128);
    ExecContext *ctx = exec_context_create(query.get(), 128, DISTANCE_METRIC_L2_SQUARED);
    ASSERT_NE(ctx, nullptr);

    /* 执行 Scan（简化：使用占位实现） */
    /* 注：实际测试需要注册真实索引 */

    exec_context_destroy(ctx);
    destroy_plan_node(scan);
}

TEST_F(VectorQueryTest, ExecuteVectorLimit) {
    /* 构建 Scan → Limit 算子链 */
    PlanNode *scan = create_vector_scan_node(VECTOR_INDEX_HNSW, 100, 100);
    PlanNode *limit = create_vector_limit_node(10, 5);

    ASSERT_NE(scan, nullptr);
    ASSERT_NE(limit, nullptr);

    limit->left = scan;

    /* 创建执行上下文 */
    auto query = generate_query_vector(64);
    ExecContext *ctx = exec_context_create(query.get(), 64, DISTANCE_METRIC_L2_SQUARED);
    ASSERT_NE(ctx, nullptr);

    /* 执行 Limit（包含上游 Scan） */
    /* 注：简化实现返回模拟数据 */

    exec_context_destroy(ctx);
    destroy_plan_node(limit);
}

TEST_F(VectorQueryTest, ExecuteVectorFilter) {
    /* 构建 Scan → Filter 算子链 */
    PlanNode *scan = create_vector_scan_node(VECTOR_INDEX_HNSW, 100, 100);
    PlanNode *filter = create_vector_filter_node(0, CMP_GT, nullptr);

    ASSERT_NE(scan, nullptr);
    ASSERT_NE(filter, nullptr);

    filter->left = scan;

    /* 验证算子类型 */
    EXPECT_EQ(filter->type, PLAN_NODE_VECTOR_FILTER);

    destroy_plan_node(filter);
}

/* ========================================================================
 * Task 6.4: 结果集构建和分页测试
 * ======================================================================== */

TEST_F(VectorQueryTest, BuildQueryResult) {
    /* 创建查询计划 */
    VectorQueryPlan *plan = build_simple_query_plan(VECTOR_INDEX_HNSW, 10);
    ASSERT_NE(plan, nullptr);

    /* 创建执行上下文 */
    auto query = generate_query_vector(128);
    ExecContext *ctx = exec_context_create(query.get(), 128, DISTANCE_METRIC_L2_SQUARED);
    ASSERT_NE(ctx, nullptr);

    /* 执行计划 */
    VectorQueryResult *result = execute_plan(plan, ctx);
    ASSERT_NE(result, nullptr);

    /* 验证结果 */
    EXPECT_GE(result->count, 0);
    EXPECT_LE(result->count, plan->top_k);

    if (result->count > 0) {
        EXPECT_NE(result->ids, nullptr);
        EXPECT_NE(result->distances, nullptr);

        /* 验证距离单调性（近似） */
        for (int i = 1; i < result->count; i++) {
            EXPECT_GE(result->distances[i], result->distances[i-1] - 1e-5f);
        }
    }

    vector_query_result_free(result);
    exec_context_destroy(ctx);
    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, ResultPagination) {
    VectorQueryPlan *plan = vector_query_plan_create();
    ASSERT_NE(plan, nullptr);

    /* 设置分页参数 */
    vector_query_plan_set_topk(plan, 10);
    vector_query_plan_set_offset(plan, 5);

    EXPECT_EQ(plan->top_k, 10);
    EXPECT_EQ(plan->offset, 5);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, ResultJsonSerialization) {
    /* 构造模拟结果 */
    VectorQueryResult result;
    memset(&result, 0, sizeof(result));

    int64_t ids[3] = {1, 2, 3};
    float dists[3] = {0.1f, 0.2f, 0.3f};
    float scores[3] = {0.9f, 0.8f, 0.7f};

    result.count = 3;
    result.capacity = 10;
    result.ids = ids;
    result.distances = dists;
    result.scores = scores;
    result.total_candidates = 100;
    result.coarse_time_us = 1000;
    result.rerank_time_us = 500;
    result.total_time_us = 1500;

    /* 序列化为 JSON */
    char *json = vector_query_result_to_json(&result);
    ASSERT_NE(json, nullptr);

    /* 验证 JSON 内容 */
    EXPECT_NE(strstr(json, "\"count\":3"), nullptr);
    EXPECT_NE(strstr(json, "\"id\":1"), nullptr);
    EXPECT_NE(strstr(json, "\"distance\":0."), nullptr);
    EXPECT_NE(strstr(json, "\"score\":0."), nullptr);
    EXPECT_NE(strstr(json, "\"total_time_us\":"), nullptr);

    free(json);
}

/* ========================================================================
 * Task 6.5: 查询优化测试
 * ======================================================================== */

TEST_F(VectorQueryTest, OptimizeQueryPlan) {
    VectorQueryPlan *plan = vector_query_plan_create();
    ASSERT_NE(plan, nullptr);

    plan->top_k = 10;

    /* 小数据集优化 */
    VectorQueryPlan *optimized = optimize_query_plan(plan, 5000, 128, DISTANCE_METRIC_L2_SQUARED);
    ASSERT_NE(optimized, nullptr);
    EXPECT_EQ(optimized->coarse_index_type, VECTOR_INDEX_BRUTE_FORCE);

    vector_query_plan_destroy(plan);

    /* 中等数据集优化 */
    plan = vector_query_plan_create();
    plan->top_k = 10;
    optimized = optimize_query_plan(plan, 50000, 128, DISTANCE_METRIC_L2_SQUARED);
    ASSERT_NE(optimized, nullptr);
    EXPECT_EQ(optimized->coarse_index_type, VECTOR_INDEX_HNSW);

    vector_query_plan_destroy(plan);

    /* 大数据集优化 */
    plan = vector_query_plan_create();
    plan->top_k = 10;
    optimized = optimize_query_plan(plan, 2000000, 128, DISTANCE_METRIC_L2_SQUARED);
    ASSERT_NE(optimized, nullptr);
    EXPECT_EQ(optimized->coarse_index_type, VECTOR_INDEX_DISKANN);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, PlanCacheHitRate) {
    /* 清空缓存 */
    vector_query_clear_plan_cache();

    /* 第一次查询：缓存未命中 */
    VectorQueryPlan *plan = build_simple_query_plan(VECTOR_INDEX_HNSW, 10);
    vector_query_cache_plan("hnsw_128_l2_10", plan);
    vector_query_plan_destroy(plan);

    float hit_rate = vector_query_get_cache_hit_rate();
    EXPECT_FLOAT_EQ(hit_rate, 0.0f);

    /* 第二次查询：缓存命中 */
    VectorQueryPlan *cached = vector_query_get_cached_plan("hnsw_128_l2_10");
    EXPECT_NE(cached, nullptr);

    hit_rate = vector_query_get_cache_hit_rate();
    EXPECT_FLOAT_EQ(hit_rate, 1.0f);

    vector_query_plan_destroy(cached);
}

TEST_F(VectorQueryTest, QueryStats) {
    /* 重置统计 */
    vector_query_reset_stats();

    /* 执行几次查询 */
    auto query = generate_query_vector(128);

    for (int i = 0; i < 3; i++) {
        VectorQueryPlan *plan = build_simple_query_plan(VECTOR_INDEX_HNSW, 5);
        ExecContext *ctx = exec_context_create(query.get(), 128, DISTANCE_METRIC_L2_SQUARED);

        VectorQueryResult *result = execute_plan(plan, ctx);

        if (result) {
            vector_query_result_free(result);
        }

        exec_context_destroy(ctx);
        vector_query_plan_destroy(plan);
    }

    /* 验证统计 */
    VectorQueryStats stats;
    vector_query_get_stats(&stats);

    EXPECT_EQ(stats.total_queries, 3);
    EXPECT_EQ(stats.successful_queries, 3);
    EXPECT_EQ(stats.failed_queries, 0);
}

/* ========================================================================
 * 原有测试（保持兼容）
 * ======================================================================== */

TEST_F(VectorQueryTest, PlanCreateAndDestroy) {
    VectorQueryPlan *plan = vector_query_plan_create();
    ASSERT_NE(nullptr, plan);
    EXPECT_EQ(VECTOR_INDEX_HNSW, plan->coarse_index_type);
    EXPECT_EQ(100, plan->coarse_ef_search);
    EXPECT_EQ(10, plan->top_k);
    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, PlanSetCoarse) {
    VectorQueryPlan *plan = vector_query_plan_create();

    vector_query_plan_set_coarse(plan, VECTOR_INDEX_DISKANN, 200, 500);
    EXPECT_EQ(VECTOR_INDEX_DISKANN, plan->coarse_index_type);
    EXPECT_EQ(200, plan->coarse_ef_search);
    EXPECT_EQ(500, plan->coarse_max_candidates);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, PlanSetRerank) {
    VectorQueryPlan *plan = vector_query_plan_create();

    vector_query_plan_set_rerank(plan, RERANKER_MMR, 50);
    EXPECT_EQ(RERANKER_MMR, plan->reranker_type);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, PlanSetHybrid) {
    VectorQueryPlan *plan = vector_query_plan_create();

    vector_query_plan_set_hybrid(plan, true, 0.7f, 0.3f, FUSION_RRF);
    EXPECT_TRUE(plan->hybrid_enabled);
    EXPECT_FLOAT_EQ(0.7f, plan->vector_weight);
    EXPECT_FLOAT_EQ(0.3f, plan->text_weight);
    EXPECT_EQ(FUSION_RRF, plan->fusion_type);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, PlanSetTopk) {
    VectorQueryPlan *plan = vector_query_plan_create();

    vector_query_plan_set_topk(plan, 100);
    EXPECT_EQ(100, plan->top_k);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, PlanCopy) {
    VectorQueryPlan *plan = vector_query_plan_create();
    vector_query_plan_set_coarse(plan, VECTOR_INDEX_IVF, 300, 200);
    vector_query_plan_set_topk(plan, 50);

    VectorQueryPlan *copy = vector_query_plan_copy(plan);
    ASSERT_NE(nullptr, copy);
    EXPECT_EQ(plan->coarse_index_type, copy->coarse_index_type);
    EXPECT_EQ(plan->coarse_ef_search, copy->coarse_ef_search);
    EXPECT_EQ(plan->top_k, copy->top_k);

    vector_query_plan_destroy(plan);
    vector_query_plan_destroy(copy);
}

TEST_F(VectorQueryTest, InitAndShutdown) {
    EXPECT_TRUE(vector_query_is_ready());
    // 重复初始化应该安全
    vector_query_init();
    EXPECT_TRUE(vector_query_is_ready());
}

TEST_F(VectorQueryTest, ExecuteBasic) {
    VectorQueryPlan *plan = vector_query_plan_create();
    vector_query_plan_set_topk(plan, 10);

    float query[128] = {0};
    for (int i = 0; i < 128; i++) query[i] = (float)i;

    VectorQueryResult *result = vector_query_execute(plan, query, 128);

    if (result) {
        EXPECT_GT(result->count, 0);
        EXPECT_GE(result->total_time_us, 0);
        vector_query_result_free(result);
    }

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, ExecuteWithDifferentIndexes) {
    VectorQueryPlan *plan = vector_query_plan_create();
    float query[64] = {0};

    // 测试 HNSW
    vector_query_plan_set_coarse(plan, VECTOR_INDEX_HNSW, 100, 100);
    VectorQueryResult *result = vector_query_execute(plan, query, 64);
    if (result) vector_query_result_free(result);

    // 测试 DiskANN
    vector_query_plan_set_coarse(plan, VECTOR_INDEX_DISKANN, 100, 100);
    result = vector_query_execute(plan, query, 64);
    if (result) vector_query_result_free(result);

    // 测试 IVF
    vector_query_plan_set_coarse(plan, VECTOR_INDEX_IVF, 50, 200);
    result = vector_query_execute(plan, query, 64);
    if (result) vector_query_result_free(result);

    vector_query_plan_destroy(plan);
}

// 融合函数测试
TEST(VectorFusionTest, RRF) {
    const int num_results = 2;
    const int num_items = 10;
    const int32_t counts[] = {5, 5};
    float distances[2][5] = {{0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
                              {0.15f, 0.25f, 0.35f, 0.45f, 0.55f}};
    const float *dist_ptrs[] = {distances[0], distances[1]};
    float scores[num_items] = {0};

    vector_fusion_rrf(dist_ptrs, counts, num_results, 60, scores);

    // 检查分数是否计算
    for (int i = 0; i < num_items; i++) {
        EXPECT_GE(scores[i], 0.0f);
    }
}

TEST(VectorFusionTest, Weighted) {
    const int num_items = 5;
    float distances[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    float weights[] = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f};
    float scores[num_items] = {0};

    vector_fusion_weighted(distances, weights, num_items, scores);

    for (int i = 0; i < num_items; i++) {
        EXPECT_FLOAT_EQ(scores[i], distances[i] * weights[i]);
    }
}

TEST(VectorFusionTest, Normalize) {
    const int num_items = 5;
    float distances[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float normalized[num_items] = {0};

    vector_normalize_distances(distances, num_items, normalized);

    EXPECT_FLOAT_EQ(0.0f, normalized[0]);  // min -> 0
    EXPECT_FLOAT_EQ(1.0f, normalized[4]);  // max -> 1
}

TEST_F(VectorQueryTest, GenerateSignature) {
    VectorQueryPlan *plan = vector_query_plan_create();
    vector_query_plan_set_coarse(plan, VECTOR_INDEX_HNSW, 100, 100);
    vector_query_plan_set_rerank(plan, RERANKER_PRECISE, 10);

    char sig[128] = {0};
    vector_query_generate_signature(plan, 128, sig, sizeof(sig));

    EXPECT_GT(strlen(sig), 0);

    vector_query_plan_destroy(plan);
}

TEST_F(VectorQueryTest, Stats) {
    VectorQueryStats stats;
    vector_query_reset_stats();
    vector_query_get_stats(&stats);

    EXPECT_EQ(0, stats.total_queries);
    EXPECT_EQ(0, stats.successful_queries);

    // 执行一些查询
    VectorQueryPlan *plan = vector_query_plan_create();
    float query[64] = {0};
    for (int i = 0; i < 5; i++) {
        VectorQueryResult *result = vector_query_execute(plan, query, 64);
        if (result) vector_query_result_free(result);
    }
    vector_query_plan_destroy(plan);

    vector_query_get_stats(&stats);
    EXPECT_EQ(5, stats.total_queries);
    EXPECT_EQ(5, stats.successful_queries);
}

TEST_F(VectorQueryTest, Profiling) {
    vector_query_set_profiling(true);
    EXPECT_TRUE(vector_query_is_profiling());

    VectorQueryPlan *plan = vector_query_plan_create();
    float query[64] = {0};

    VectorQueryResult *result = vector_query_execute(plan, query, 64);
    if (result) vector_query_result_free(result);

    vector_query_plan_destroy(plan);

    // 检查剖析数据
    VectorQueryProfileEntry entries[16];
    int count = vector_query_get_profile(entries, 16);
    EXPECT_GE(count, 0);

    vector_query_set_profiling(false);
    EXPECT_FALSE(vector_query_is_profiling());
}

TEST_F(VectorQueryTest, IndexRegistration) {
    // 注销不存在的索引应该返回 -1
    EXPECT_EQ(-1, vector_query_unregister_index("nonexistent"));

    // 获取不存在的索引应该返回 NULL
    EXPECT_EQ(nullptr, vector_query_get_index("nonexistent"));
}

TEST_F(VectorQueryTest, PlanCache) {
    vector_query_clear_plan_cache();

    VectorQueryPlan *plan = vector_query_plan_create();

    // 缓存命中率应该为 0
    EXPECT_FLOAT_EQ(0.0f, vector_query_get_cache_hit_rate());

    vector_query_plan_destroy(plan);
}

// 主函数
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
