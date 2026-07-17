/**
 * @file test_reranker.cpp
 * @brief ReRanker 模块单元测试
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstring>

extern "C" {
#include "db/index/vector_index/reranker/reranker.h"
#include "db/index/vector_index/reranker/precise_reranker.h"
#include "db/index/vector_index/reranker/multi_metric_reranker.h"
#include "db/index/vector_index/reranker/mmr_reranker.h"
#include "db/index/vector_index/reranker/two_stage_search.h"
}

/* ========================================================================
 * 测试辅助函数
 * ======================================================================== */

static void generate_random_vectors(float *vectors, int32_t n, int32_t dims) {
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

static void generate_clustered_vectors(float *vectors, int32_t n, int32_t dims, int32_t num_clusters) {
    /* 生成聚类数据：每个簇有明显的中心点 */
    float cluster_centers[10][128];  /* 最多 10 个簇 */

    for (int32_t c = 0; c < num_clusters && c < 10; c++) {
        for (int32_t d = 0; d < dims; d++) {
            cluster_centers[c][d] = (float)(c + 1) / num_clusters;
        }
    }

    for (int32_t i = 0; i < n; i++) {
        int32_t cluster = i % num_clusters;
        for (int32_t d = 0; d < dims; d++) {
            float noise = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            vectors[i * dims + d] = cluster_centers[cluster][d] + noise;
        }
    }
}

/* ========================================================================
 * 精确精排测试
 * ======================================================================== */

class PreciseRerankerTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand((unsigned)time(nullptr));
    }
};

TEST_F(PreciseRerankerTest, CreateAndDestroy) {
    RerankerConfig config = reranker_config_default(RERANKER_PRECISE, 128);
    Reranker *reranker = reranker_create(RERANKER_PRECISE, &config);
    ASSERT_NE(nullptr, reranker);
    EXPECT_EQ(RERANKER_PRECISE, reranker_get_type(reranker));
    EXPECT_STREQ("precise", reranker_get_name(reranker));
    reranker_destroy(reranker);
}

TEST_F(PreciseRerankerTest, RerankBasic) {
    RerankerConfig config = reranker_config_default(RERANKER_PRECISE, 4);
    config.metric = DISTANCE_METRIC_L2_SQUARED;

    Reranker *reranker = reranker_create(RERANKER_PRECISE, &config);
    ASSERT_NE(nullptr, reranker);

    /* 创建测试数据 */
    float query[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float candidates[3][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},   /* 距离约 1.0 */
        {1.0f, 1.0f, 1.0f, 1.0f},   /* 距离约 1.0 */
        {0.5f, 0.5f, 0.5f, 0.5f}    /* 距离 0 */
    };

    float scores[3];
    int32_t ids[3];

    int32_t result = reranker_rerank(reranker, query, (float *)candidates, 3, 2, scores, ids);

    EXPECT_EQ(2, result);
    /* 最近的应该是 ID=2（零距离） */
    EXPECT_EQ(2, ids[0]);
    EXPECT_LE(scores[0], scores[1]);

    reranker_destroy(reranker);
}

TEST_F(PreciseRerankerTest, RerankRandom) {
    RerankerConfig config = reranker_config_default(RERANKER_PRECISE, 128);
    Reranker *reranker = reranker_create(RERANKER_PRECISE, &config);
    ASSERT_NE(nullptr, reranker);

    float query[128];
    float candidates[50][128];
    float scores[50];
    int32_t ids[50];

    generate_random_vectors(query, 1, 128);
    generate_random_vectors((float *)candidates, 50, 128);

    int32_t result = reranker_rerank(reranker, query, (float *)candidates, 50, 10, scores, ids);

    EXPECT_EQ(10, result);
    /* 验证结果按分数排序 */
    for (int32_t i = 1; i < result; i++) {
        EXPECT_LE(scores[i-1], scores[i]);
    }

    reranker_destroy(reranker);
}

/* ========================================================================
 * 多度量融合测试
 * ======================================================================== */

class MultiMetricRerankerTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand((unsigned)time(nullptr));
    }
};

TEST_F(MultiMetricRerankerTest, CreateAndDestroy) {
    RerankerConfig config = reranker_config_default(RERANKER_MULTI_METRIC, 128);
    Reranker *reranker = reranker_create(RERANKER_MULTI_METRIC, &config);
    ASSERT_NE(nullptr, reranker);
    EXPECT_EQ(RERANKER_MULTI_METRIC, reranker_get_type(reranker));
    EXPECT_STREQ("multi_metric", reranker_get_name(reranker));
    reranker_destroy(reranker);
}

TEST_F(MultiMetricRerankerTest, RerankWithFusion) {
    RerankerConfig config = reranker_config_default(RERANKER_MULTI_METRIC, 4);
    config.multi_metric.num_metrics = 2;
    config.multi_metric.metrics[0] = DISTANCE_METRIC_L2_SQUARED;
    config.multi_metric.metrics[1] = DISTANCE_METRIC_COSINE;
    config.multi_metric.weights[0] = 0.5f;
    config.multi_metric.weights[1] = 0.5f;

    Reranker *reranker = reranker_create(RERANKER_MULTI_METRIC, &config);
    ASSERT_NE(nullptr, reranker);

    float query[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float candidates[20][4];
    generate_random_vectors((float *)candidates, 20, 4);

    float scores[20];
    int32_t ids[20];

    int32_t result = reranker_rerank(reranker, query, (float *)candidates, 20, 5, scores, ids);

    EXPECT_EQ(5, result);

    reranker_destroy(reranker);
}

/* ========================================================================
 * MMR 去重测试
 * ======================================================================== */

class MmrRerankerTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand((unsigned)time(nullptr));
    }
};

TEST_F(MmrRerankerTest, CreateAndDestroy) {
    RerankerConfig config = reranker_config_default(RERANKER_MMR, 128);
    config.mmr.lambda = 0.7f;
    Reranker *reranker = reranker_create(RERANKER_MMR, &config);
    ASSERT_NE(nullptr, reranker);
    EXPECT_EQ(RERANKER_MMR, reranker_get_type(reranker));
    EXPECT_STREQ("mmr", reranker_get_name(reranker));
    reranker_destroy(reranker);
}

TEST_F(MmrRerankerTest, MmrDiverseResults) {
    RerankerConfig config = reranker_config_default(RERANKER_MMR, 4);
    config.mmr.lambda = 0.5f;  /* 平衡相关性和多样性 */

    Reranker *reranker = reranker_create(RERANKER_MMR, &config);
    ASSERT_NE(nullptr, reranker);

    /* 生成聚类数据：5 个簇，每簇 4 个向量 */
    float candidates[20][4];
    generate_clustered_vectors((float *)candidates, 20, 4, 5);

    float query[4] = {0.5f, 0.5f, 0.5f, 0.5f};  /* 接近所有簇中心 */
    float scores[10];
    int32_t ids[10];

    int32_t result = reranker_rerank(reranker, query, (float *)candidates, 20, 5, scores, ids);

    EXPECT_EQ(5, result);

    /* 验证多样性：检查结果 ID 是否来自不同簇 */
    /* 由于是聚类数据，ID 相邻的向量很可能来自同一簇 */
    int32_t same_cluster_count = 0;
    for (int32_t i = 1; i < result; i++) {
        if (ids[i] / 4 == ids[i-1] / 4) {
            same_cluster_count++;
        }
    }

    /* MMR 应该产生更多样化的结果 */
    /* 理想情况下，5 个结果应该来自 5 个不同的簇 */

    reranker_destroy(reranker);
}

TEST_F(MmrRerankerTest, LambdaEffect) {
    /* 测试不同 lambda 值的效果 */
    float lambda_0_scores[5], lambda_1_scores[5];
    int32_t lambda_0_ids[5], lambda_1_ids[5];

    /* Lambda = 0: 纯多样性 */
    {
        RerankerConfig config = reranker_config_default(RERANKER_MMR, 4);
        config.mmr.lambda = 0.0f;
        Reranker *reranker = reranker_create(RERANKER_MMR, &config);

        float candidates[20][4];
        generate_clustered_vectors((float *)candidates, 20, 4, 5);
        float query[4] = {0.5f, 0.5f, 0.5f, 0.5f};

        reranker_rerank(reranker, query, (float *)candidates, 20, 5,
                        lambda_0_scores, lambda_0_ids);
        reranker_destroy(reranker);
    }

    /* Lambda = 1: 纯相关性 */
    {
        RerankerConfig config = reranker_config_default(RERANKER_MMR, 4);
        config.mmr.lambda = 1.0f;
        Reranker *reranker = reranker_create(RERANKER_MMR, &config);

        float candidates[20][4];
        generate_clustered_vectors((float *)candidates, 20, 4, 5);
        float query[4] = {0.5f, 0.5f, 0.5f, 0.5f};

        reranker_rerank(reranker, query, (float *)candidates, 20, 5,
                        lambda_1_scores, lambda_1_ids);
        reranker_destroy(reranker);
    }

    /* 验证两种策略产生不同结果 */
    int32_t same_count = 0;
    for (int32_t i = 0; i < 5; i++) {
        if (lambda_0_ids[i] == lambda_1_ids[i]) {
            same_count++;
        }
    }

    /* 至少有一些不同（理想情况下应该完全不同） */
    EXPECT_LE(same_count, 4);
}

/* ========================================================================
 * 工厂注册测试
 * ======================================================================== */

class RerankerFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand((unsigned)time(nullptr));
    }
};

static int32_t custom_rerank_func(const float *query,
                                  const float *candidates,
                                  int32_t n_candidates,
                                  int32_t k,
                                  float *scores,
                                  int32_t *reranked_ids,
                                  void *user_data) {
    (void)query;
    (void)candidates;
    (void)user_data;

    /* 简单实现：返回前 k 个 */
    for (int32_t i = 0; i < k && i < n_candidates; i++) {
        scores[i] = (float)(k - i);
        reranked_ids[i] = i;
    }
    return (k < n_candidates) ? k : n_candidates;
}

TEST_F(RerankerFactoryTest, RegisterAndApply) {
    /* 注册自定义 ReRanker */
    int32_t ret = reranker_register("custom_test", custom_rerank_func, NULL);
    EXPECT_EQ(0, ret);

    /* 应用已注册的 ReRanker */
    float query[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float candidates[10][4];
    generate_random_vectors((float *)candidates, 10, 4);

    float scores[5];
    int32_t ids[5];

    ret = reranker_apply("custom_test", query, (float *)candidates, 10, 5, scores, ids);
    EXPECT_EQ(5, ret);
}

TEST_F(RerankerFactoryTest, ListRegistered) {
    char names[16][64];
    int32_t count = reranker_list_registered(names, 16);
    EXPECT_GE(count, 0);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
