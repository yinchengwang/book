/**
 * @file test_vector.c
 * @brief 向量存储模态追赶测试
 *
 * 测试 doc_vector 模块：嵌入存储、语义搜索、混合检索、分数融合、SQL函数
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* 头文件 */
#include "db/storage/doc/doc_vector.h"

/* ========================================================================
 * 嵌入存储测试
 * ======================================================================== */

class DocEmbeddingStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        store = doc_embedding_store_create(NULL);
    }

    void TearDown() override {
        doc_embedding_store_destroy(store);
    }

    DocEmbeddingStore *store;
};

TEST_F(DocEmbeddingStoreTest, CreateDestroy) {
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(doc_embedding_store_count(store), 0u);
}

TEST_F(DocEmbeddingStoreTest, AddAndGet) {
    float vec[] = {1.0f, 2.0f, 3.0f};
    int ret = doc_embedding_store_add(store, "doc1", "embedding", vec, 3, 100);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(doc_embedding_store_count(store), 1u);

    DocEmbedding *emb = doc_embedding_store_get(store, "doc1", "embedding");
    ASSERT_NE(emb, nullptr);
    EXPECT_STREQ(emb->doc_id, "doc1");
    EXPECT_STREQ(emb->field_name, "embedding");
    EXPECT_EQ(emb->dimension, 3);
    EXPECT_EQ(emb->doc_size, 100u);
    EXPECT_FLOAT_EQ(emb->vector[0], 1.0f);
    EXPECT_FLOAT_EQ(emb->vector[1], 2.0f);
    EXPECT_FLOAT_EQ(emb->vector[2], 3.0f);
}

TEST_F(DocEmbeddingStoreTest, AddUpdate) {
    float vec1[] = {1.0f, 2.0f};
    float vec2[] = {4.0f, 5.0f};

    doc_embedding_store_add(store, "doc1", "emb", vec1, 2, 50);
    doc_embedding_store_add(store, "doc1", "emb", vec2, 2, 60);

    EXPECT_EQ(doc_embedding_store_count(store), 1u);
    DocEmbedding *emb = doc_embedding_store_get(store, "doc1", "emb");
    ASSERT_NE(emb, nullptr);
    EXPECT_FLOAT_EQ(emb->vector[0], 4.0f);
    EXPECT_EQ(emb->doc_size, 60u);
}

TEST_F(DocEmbeddingStoreTest, Remove) {
    float vec[] = {1.0f};
    doc_embedding_store_add(store, "doc1", "emb", vec, 1, 10);
    doc_embedding_store_add(store, "doc2", "emb", vec, 1, 20);
    EXPECT_EQ(doc_embedding_store_count(store), 2u);

    int ret = doc_embedding_store_remove(store, "doc1", "emb");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(doc_embedding_store_count(store), 1u);
    EXPECT_EQ(doc_embedding_store_get(store, "doc1", "emb"), nullptr);
    EXPECT_NE(doc_embedding_store_get(store, "doc2", "emb"), nullptr);
}

TEST_F(DocEmbeddingStoreTest, RemoveNotFound) {
    float vec[] = {1.0f};
    doc_embedding_store_add(store, "doc1", "emb", vec, 1, 10);
    int ret = doc_embedding_store_remove(store, "nonexistent", "emb");
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(doc_embedding_store_count(store), 1u);
}

TEST_F(DocEmbeddingStoreTest, GetNotFound) {
    EXPECT_EQ(doc_embedding_store_get(store, "no", "no"), nullptr);
}

TEST_F(DocEmbeddingStoreTest, AddNull) {
    float vec[] = {1.0f};
    EXPECT_EQ(doc_embedding_store_add(NULL, "doc1", "emb", vec, 1, 10), -1);
    EXPECT_EQ(doc_embedding_store_add(store, NULL, "emb", vec, 1, 10), -1);
    EXPECT_EQ(doc_embedding_store_add(store, "doc1", NULL, vec, 1, 10), -1);
    EXPECT_EQ(doc_embedding_store_add(store, "doc1", "emb", NULL, 1, 10), -1);
}

/* ========================================================================
 * 语义搜索测试
 * ======================================================================== */

class DocSemanticSearchTest : public ::testing::Test {
protected:
    void SetUp() override {
        store = doc_embedding_store_create(NULL);

        /* 添加测试数据 */
        float v1[] = {1.0f, 0.0f, 0.0f};
        float v2[] = {0.0f, 1.0f, 0.0f};
        float v3[] = {0.0f, 0.0f, 1.0f};
        float v4[] = {0.5f, 0.5f, 0.0f};

        doc_embedding_store_add(store, "d1", "emb", v1, 3, 100);
        doc_embedding_store_add(store, "d2", "emb", v2, 3, 200);
        doc_embedding_store_add(store, "d3", "emb", v3, 3, 150);
        doc_embedding_store_add(store, "d4", "emb", v4, 3, 120);
    }

    void TearDown() override {
        doc_embedding_store_destroy(store);
    }

    DocEmbeddingStore *store;
};

TEST_F(DocSemanticSearchTest, SearchBasic) {
    float query[] = {1.0f, 0.0f, 0.0f};
    DocVectorSearchOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.top_k = 3;
    opts.include_snippets = false;

    DocSemanticSearchResult *result = doc_semantic_search(store, query, 3, &opts);
    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->num_results, 0u);

    /* 最相似的应该是 d1 */
    EXPECT_STREQ(result->results[0].doc_id, "d1");

    doc_semantic_search_free(result);
}

TEST_F(DocSemanticSearchTest, SearchWithSnippet) {
    float query[] = {0.0f, 1.0f, 0.0f};
    DocVectorSearchOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.top_k = 2;
    opts.include_snippets = true;

    DocSemanticSearchResult *result = doc_semantic_search(store, query, 3, &opts);
    ASSERT_NE(result, nullptr);

    /* 检查第一个结果有 snippet（至少一个有效结果） */
    EXPECT_GT(result->num_results, 0u);
    EXPECT_NE(result->results[0].snippet, nullptr);

    doc_semantic_search_free(result);
}

TEST_F(DocSemanticSearchTest, SearchWithMinScore) {
    float query[] = {1.0f, 0.0f, 0.0f};
    DocVectorSearchOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.top_k = 10;
    opts.min_score = 0.9f;  /* 很高阈值，只有精确匹配 */
    opts.include_snippets = false;

    DocSemanticSearchResult *result = doc_semantic_search(store, query, 3, &opts);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->num_results, 1u);
    EXPECT_STREQ(result->results[0].doc_id, "d1");

    doc_semantic_search_free(result);
}

TEST_F(DocSemanticSearchTest, SearchNull) {
    DocVectorSearchOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.top_k = 3;

    EXPECT_EQ(doc_semantic_search(NULL, NULL, 3, &opts), nullptr);
}

/* ========================================================================
 * 混合检索测试
 * ======================================================================== */

class DocHybridSearchTest : public ::testing::Test {
protected:
    void SetUp() override {
        store = doc_embedding_store_create(NULL);

        float v1[] = {1.0f, 0.0f, 0.0f};
        float v2[] = {0.0f, 1.0f, 0.0f};
        float v3[] = {0.0f, 0.0f, 1.0f};

        doc_embedding_store_add(store, "d1", "emb", v1, 3, 100);
        doc_embedding_store_add(store, "d2", "emb", v2, 3, 200);
        doc_embedding_store_add(store, "d3", "emb", v3, 3, 150);

        /* 设置 BM25 分数 */
        store->embeddings[0].bm25_score = 0.8;
        store->embeddings[1].bm25_score = 0.3;
        store->embeddings[2].bm25_score = 0.5;
    }

    void TearDown() override {
        doc_embedding_store_destroy(store);
    }

    DocEmbeddingStore *store;
};

TEST_F(DocHybridSearchTest, CreateDestroy) {
    DocHybridSearcher *searcher = doc_hybrid_searcher_create(store, NULL);
    ASSERT_NE(searcher, nullptr);
    EXPECT_EQ(searcher->config.mode, DOC_HYBRID_RRF);
    EXPECT_DOUBLE_EQ(searcher->config.vector_weight, 0.5);
    EXPECT_DOUBLE_EQ(searcher->config.bm25_weight, 0.5);
    doc_hybrid_searcher_destroy(searcher);
}

TEST_F(DocHybridSearchTest, CreateWithConfig) {
    DocHybridConfig config;
    memset(&config, 0, sizeof(config));
    config.mode = DOC_HYBRID_WEIGHTED;
    config.vector_weight = 0.7;
    config.bm25_weight = 0.3;

    DocHybridSearcher *searcher = doc_hybrid_searcher_create(store, &config);
    ASSERT_NE(searcher, nullptr);
    EXPECT_EQ(searcher->config.mode, DOC_HYBRID_WEIGHTED);
    EXPECT_DOUBLE_EQ(searcher->config.vector_weight, 0.7);
    doc_hybrid_searcher_destroy(searcher);
}

TEST_F(DocHybridSearchTest, HybridSearchWeighted) {
    DocHybridConfig config;
    memset(&config, 0, sizeof(config));
    config.mode = DOC_HYBRID_WEIGHTED;
    config.vector_weight = 0.6;
    config.bm25_weight = 0.4;

    DocHybridSearcher *searcher = doc_hybrid_searcher_create(store, &config);
    ASSERT_NE(searcher, nullptr);

    float query[] = {1.0f, 0.0f, 0.0f};
    DocVectorSearchOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.top_k = 3;
    opts.include_snippets = false;

    DocSemanticSearchResult *result = doc_hybrid_search(searcher, query, 3, NULL, &opts);
    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->num_results, 0u);

    doc_semantic_search_free(result);
    doc_hybrid_searcher_destroy(searcher);
}

TEST_F(DocHybridSearchTest, HybridSearchRRF) {
    DocHybridConfig config;
    memset(&config, 0, sizeof(config));
    config.mode = DOC_HYBRID_RRF;
    config.rrf_k = 60;

    DocHybridSearcher *searcher = doc_hybrid_searcher_create(store, &config);
    ASSERT_NE(searcher, nullptr);

    float query[] = {1.0f, 0.0f, 0.0f};
    DocVectorSearchOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.top_k = 3;
    opts.include_snippets = false;

    DocSemanticSearchResult *result = doc_hybrid_search(searcher, query, 3, NULL, &opts);
    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->num_results, 0u);

    doc_semantic_search_free(result);
    doc_hybrid_searcher_destroy(searcher);
}

TEST_F(DocHybridSearchTest, SetBm25Index) {
    DocHybridSearcher *searcher = doc_hybrid_searcher_create(store, NULL);
    ASSERT_NE(searcher, nullptr);

    int ret = doc_hybrid_set_bm25_index(searcher, (void*)0x1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(searcher->bm25_index, (void*)0x1);

    doc_hybrid_searcher_destroy(searcher);
}

TEST_F(DocHybridSearchTest, HybridSearchNull) {
    DocVectorSearchOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.top_k = 3;
    EXPECT_EQ(doc_hybrid_search(NULL, NULL, 0, NULL, &opts), nullptr);
}

/* ========================================================================
 * 分数融合测试
 * ======================================================================== */

class DocScoreFusionTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(DocScoreFusionTest, RrfFusion) {
    double list1[] = {0.9, 0.7, 0.5};
    double list2[] = {0.6, 0.8, 0.4};
    const double *doc_scores[] = {list1, list2};
    double out[3];

    doc_score_fusion_rrf(doc_scores, 2, 60, out, 3);

    /* RRF 分数应该是正数 */
    for (int i = 0; i < 3; i++) {
        EXPECT_GT(out[i], 0.0);
    }
}

TEST_F(DocScoreFusionTest, WeightedFusion) {
    double list1[] = {1.0, 2.0, 3.0};
    double list2[] = {3.0, 2.0, 1.0};
    const double *doc_scores[] = {list1, list2};
    double weights[] = {0.5, 0.5};
    double out[3];

    doc_score_fusion_weighted(doc_scores, weights, 2, out, 3);

    EXPECT_DOUBLE_EQ(out[0], 2.0);  /* 0.5*1 + 0.5*3 */
    EXPECT_DOUBLE_EQ(out[1], 2.0);  /* 0.5*2 + 0.5*2 */
    EXPECT_DOUBLE_EQ(out[2], 2.0);  /* 0.5*3 + 0.5*1 */
}

TEST_F(DocScoreFusionTest, NormalizeScores) {
    double scores[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    doc_normalize_scores(scores, 5);

    EXPECT_DOUBLE_EQ(scores[0], 0.0);   /* min */
    EXPECT_DOUBLE_EQ(scores[4], 1.0);   /* max */
    EXPECT_DOUBLE_EQ(scores[2], 0.5);   /* mid */
}

TEST_F(DocScoreFusionTest, NormalizeScoresSameValue) {
    double scores[] = {2.0, 2.0, 2.0};
    doc_normalize_scores(scores, 3);
    /* 范围为0时应保持原值 */
    EXPECT_DOUBLE_EQ(scores[0], 2.0);
}

TEST_F(DocScoreFusionTest, RankResults) {
    DocVectorSearchResult results[3];
    memset(results, 0, sizeof(results));

    strcpy(results[0].doc_id, "d1");
    results[0].vector_score = 0.3;
    results[0].bm25_score = 0.9;

    strcpy(results[1].doc_id, "d2");
    results[1].vector_score = 0.8;
    results[1].bm25_score = 0.2;

    strcpy(results[2].doc_id, "d3");
    results[2].vector_score = 0.5;
    results[2].bm25_score = 0.5;

    DocHybridConfig config;
    memset(&config, 0, sizeof(config));
    config.vector_weight = 0.5;
    config.bm25_weight = 0.5;

    doc_rank_results(results, 3, &config);

    /* 按综合分数排序：d3=0.5, d2=0.5, d1=0.6 → d1最高 */
    EXPECT_STREQ(results[0].doc_id, "d1");
    EXPECT_EQ(results[0].rank, 1);
}

/* ========================================================================
 * SQL 函数测试
 * ======================================================================== */

class DocVectorSqlTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(DocVectorSqlTest, SqlEmbed) {
    float *vector = NULL;
    int dim = 0;
    int ret = doc_sql_embed("{\"text\":\"hello\"}", "content", "default", &vector, &dim);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(dim, 128);
    ASSERT_NE(vector, nullptr);

    free(vector);
}

TEST_F(DocVectorSqlTest, SqlEmbedNull) {
    EXPECT_EQ(doc_sql_embed(NULL, NULL, NULL, NULL, NULL), -1);
}

TEST_F(DocVectorSqlTest, SqlSearch) {
    DocVectorSearchResult *results = NULL;
    size_t count = doc_sql_search("test", "[1.0, 0.0, 0.0]", "hello", 10, &results);

    /* 没有数据，应该返回 0 */
    EXPECT_EQ(count, 0u);
}

/* ========================================================================
 * main
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
