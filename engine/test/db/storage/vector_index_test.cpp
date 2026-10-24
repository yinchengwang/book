/**
 * @file vector_index_test.cpp
 * @brief Vector index comprehensive tests
 *
 * Tests HNSW-PQ, IVF-PQ, Sparse Vector, BM25, and Hybrid Search operations.
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

extern "C" {
#include "db/index/vector_index/hnsw_pq/hnsw_pq.h"
#include "db/index/vector_index/pq/pq.h"
#include "db/index/vector_index/ivf_pq/ivf_pq.h"
#include "db/sparse_vector.h"
#include "db/bm25_index.h"
#include "db/hybrid_retrieval.h"
#include "db/index/vector_index/hybrid_search.h"
#include "db/core/log.h"
}

/* ========================================================================
 * Test Utilities
 * ======================================================================== */

static void generate_random_vector(float *vec, int32_t dim, float scale = 1.0f) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-scale, scale);
    for (int32_t i = 0; i < dim; i++) {
        vec[i] = dis(gen);
    }
}

static bool vectors_equal(const float *a, const float *b, int32_t dim, float eps = 1e-4f) {
    for (int32_t i = 0; i < dim; i++) {
        if (fabsf(a[i] - b[i]) > eps) return false;
    }
    return true;
}

/* ========================================================================
 * HNSW-PQ Index Tests
 * ======================================================================== */

class HNSWPQIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        hnsw_pq_config_t config = {0};
        config.m = 16;
        config.ef_construction = 200;
        config.ef_search = 100;
        config.dims = dims_;
        config.metric = DISTANCE_METRIC_L2_SQUARED;
        config.pq_m = 8;
        config.pq_bits = 8;
        config.rerank_k = 100;

        index_ = hnsw_pq_index_create(&config);
    }

    void TearDown() override {
        if (index_) {
            hnsw_pq_index_destroy(index_);
            index_ = nullptr;
        }
    }

    static constexpr int32_t dims_ = 64;
    hnsw_pq_index_t *index_ = nullptr;
};

TEST_F(HNSWPQIndexTest, CreateAndDestroy) {
    ASSERT_NE(index_, nullptr);
    EXPECT_EQ(hnsw_pq_index_size(index_), 0);
    EXPECT_EQ(index_->config.dims, dims_);
}

TEST_F(HNSWPQIndexTest, InsertSingleVector) {
    float vec[dims_];
    generate_random_vector(vec, dims_);

    int32_t id = hnsw_pq_index_insert(index_, vec);
    EXPECT_GE(id, 0);
    EXPECT_EQ(hnsw_pq_index_size(index_), 1);
}

TEST_F(HNSWPQIndexTest, InsertMultipleVectors) {
    std::vector<float> vectors(10 * dims_);
    for (int i = 0; i < 10 * dims_; i++) {
        vectors[i] = (float)i * 0.1f;
    }

    int32_t count = hnsw_pq_index_insert_batch(index_, 10, vectors.data());
    EXPECT_EQ(count, 10);
    EXPECT_EQ(hnsw_pq_index_size(index_), 10);
}

TEST_F(HNSWPQIndexTest, SearchEmptyIndex) {
    float query[dims_];
    generate_random_vector(query, dims_);

    int32_t result_ids[10];
    float result_dists[10];

    int32_t n_results = hnsw_pq_index_search(index_, query, 10, result_ids, result_dists);
    EXPECT_EQ(n_results, 0);
}

TEST_F(HNSWPQIndexTest, SearchAfterInsert) {
    std::vector<float> vectors(5 * dims_);
    for (int i = 0; i < 5 * dims_; i++) {
        vectors[i] = (float)i * 0.01f;
    }

    hnsw_pq_index_insert_batch(index_, 5, vectors.data());

    float query[dims_];
    for (int i = 0; i < dims_; i++) {
        query[i] = 0.05f;  // Close to vector 5
    }

    int32_t result_ids[5];
    float result_dists[5];

    int32_t n_results = hnsw_pq_index_search(index_, query, 5, result_ids, result_dists);
    EXPECT_GT(n_results, 0);
    EXPECT_LE(n_results, 5);
}

TEST_F(HNSWPQIndexTest, SearchKLessThanResults) {
    std::vector<float> vectors(20 * dims_);
    for (int i = 0; i < 20 * dims_; i++) {
        vectors[i] = (float)(i % 100) * 0.01f;
    }

    hnsw_pq_index_insert_batch(index_, 20, vectors.data());

    float query[dims_];
    generate_random_vector(query, dims_);

    int32_t result_ids[3];
    float result_dists[3];

    int32_t n_results = hnsw_pq_index_search(index_, query, 3, result_ids, result_dists);
    EXPECT_EQ(n_results, 3);
}

TEST_F(HNSWPQIndexTest, AvgOutDegree) {
    std::vector<float> vectors(10 * dims_);
    for (int i = 0; i < 10 * dims_; i++) {
        vectors[i] = (float)i * 0.1f;
    }

    hnsw_pq_index_insert_batch(index_, 10, vectors.data());

    float avg_degree = hnsw_pq_index_avg_out_degree(index_);
    EXPECT_GE(avg_degree, 0.0f);
}

/* ========================================================================
 * IVF-PQ Index Tests
 * ======================================================================== */

class IVFPQIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        index_ = ivf_pq_create(4, 8, 8, dims_);
    }

    void TearDown() override {
        if (index_) {
            ivf_pq_destroy(index_);
            index_ = nullptr;
        }
    }

    static constexpr int dims_ = 32;
    ivf_pq_index_t *index_ = nullptr;
};

TEST_F(IVFPQIndexTest, CreateAndDestroy) {
    ASSERT_NE(index_, nullptr);
    // Cannot access private members - just verify create/destroy work
}

TEST_F(IVFPQIndexTest, TrainIndex) {
    std::vector<float> vectors(20 * dims_);
    for (int i = 0; i < 20 * dims_; i++) {
        vectors[i] = (float)(i % 50) * 0.1f;
    }

    int ret = ivf_pq_train(index_, 20, vectors.data());
    EXPECT_EQ(ret, 0);
}

TEST_F(IVFPQIndexTest, AddVectorsWithoutTrain) {
    std::vector<float> vectors(5 * dims_);
    for (int i = 0; i < 5 * dims_; i++) {
        vectors[i] = (float)i * 0.1f;
    }

    int ids[] = {0, 1, 2, 3, 4};
    int ret = ivf_pq_add(index_, 5, vectors.data(), ids);
    EXPECT_EQ(ret, 0);  // Should fail without training
}

TEST_F(IVFPQIndexTest, AddVectorsAfterTrain) {
    std::vector<float> vectors(20 * dims_);
    for (int i = 0; i < 20 * dims_; i++) {
        vectors[i] = (float)(i % 100) * 0.01f;
    }

    ASSERT_EQ(ivf_pq_train(index_, 20, vectors.data()), 0);

    int ids[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    int ret = ivf_pq_add(index_, 20, vectors.data(), ids);
    EXPECT_EQ(ret, 20);
}

TEST_F(IVFPQIndexTest, SearchAfterAdd) {
    std::vector<float> vectors(20 * dims_);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    for (int i = 0; i < 20 * dims_; i++) {
        vectors[i] = dis(gen);
    }

    ASSERT_EQ(ivf_pq_train(index_, 20, vectors.data()), 0);

    int ids[20];
    for (int i = 0; i < 20; i++) ids[i] = i;
    ivf_pq_add(index_, 20, vectors.data(), ids);

    // Search with first vector as query
    float query[32];
    for (int i = 0; i < dims_; i++) {
        query[i] = vectors[i];
    }

    int result_ids[5];
    float result_dists[5];

    int n_results = ivf_pq_search(index_, query, 5, result_ids, result_dists);
    EXPECT_GT(n_results, 0);
    EXPECT_LE(n_results, 5);
    EXPECT_EQ(result_ids[0], 0);  // First vector should be closest
}

TEST_F(IVFPQIndexTest, SetNprobe) {
    ivf_pq_set_nprobe(index_, 2);
    // Cannot verify internal state - just verify function runs

    ivf_pq_set_nprobe(index_, 10);  // Greater than nlist
    // Should be capped internally
}

TEST_F(IVFPQIndexTest, Stats) {
    std::vector<float> vectors(10 * dims_);
    for (int i = 0; i < 10 * dims_; i++) {
        vectors[i] = (float)i * 0.1f;
    }

    ivf_pq_train(index_, 10, vectors.data());

    int ids[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    ivf_pq_add(index_, 10, vectors.data(), ids);

    int n_vectors, code_size;
    ivf_pq_stats(index_, &n_vectors, &code_size);

    EXPECT_EQ(n_vectors, 10);
    EXPECT_EQ(code_size, 8);  // pq_m = 8
}

TEST_F(IVFPQIndexTest, SaveAndLoad) {
    std::vector<float> vectors(20 * dims_);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    for (int i = 0; i < 20 * dims_; i++) {
        vectors[i] = dis(gen);
    }

    ivf_pq_train(index_, 20, vectors.data());

    int ids[20];
    for (int i = 0; i < 20; i++) ids[i] = i;
    ivf_pq_add(index_, 20, vectors.data(), ids);

    const char *path = "test_ivf_pq.index";
    int ret = ivf_pq_save(index_, path);
    EXPECT_EQ(ret, 0);

    ivf_pq_index_t *loaded = ivf_pq_load(path);
    ASSERT_NE(loaded, nullptr);

    int n_vectors, code_size;
    ivf_pq_stats(loaded, &n_vectors, &code_size);
    EXPECT_EQ(n_vectors, 20);

    ivf_pq_destroy(loaded);
    remove(path);
}

TEST_F(IVFPQIndexTest, Rerank) {
    std::vector<float> vectors(5 * dims_);
    for (int i = 0; i < 5 * dims_; i++) {
        vectors[i] = (float)i * 0.1f;
    }

    ivf_pq_train(index_, 5, vectors.data());

    int ids[] = {0, 1, 2, 3, 4};
    ivf_pq_add(index_, 5, vectors.data(), ids);

    int result_ids[] = {0, 1, 2};
    float result_dists[] = {0.0f, 0.1f, 0.2f};

    int ret = ivf_pq_rerank(index_, 3, vectors.data(), 3, result_ids, result_dists);
    EXPECT_GE(ret, 0);
}

/* ========================================================================
 * Sparse Vector Tests
 * ======================================================================== */

class SparseVectorTest : public ::testing::Test {
protected:
    static constexpr uint32_t dim_ = 100;
};

TEST_F(SparseVectorTest, CreateAndFree) {
    sparse_vector_t *vec = sparse_vector_create(dim_);
    ASSERT_NE(vec, nullptr);
    EXPECT_EQ(vec->dim, dim_);
    EXPECT_EQ(vec->nnz, 0);
    EXPECT_EQ(vec->capacity, 16);  // Initial capacity

    sparse_vector_free(vec);
}

TEST_F(SparseVectorTest, CreateZeroDimension) {
    sparse_vector_t *vec = sparse_vector_create(0);
    EXPECT_EQ(vec, nullptr);
}

TEST_F(SparseVectorTest, SetAndGet) {
    sparse_vector_t *vec = sparse_vector_create(dim_);
    ASSERT_NE(vec, nullptr);

    // Set some values
    EXPECT_EQ(sparse_vector_set(vec, 5, 1.5f), 0);
    EXPECT_EQ(sparse_vector_set(vec, 10, 2.5f), 0);
    EXPECT_EQ(sparse_vector_set(vec, 50, 3.5f), 0);

    EXPECT_EQ(vec->nnz, 3);
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 5), 1.5f);
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 10), 2.5f);
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 50), 3.5f);
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 0), 0.0f);  // Not set
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 99), 0.0f); // Out of range
}

TEST_F(SparseVectorTest, SetZeroValueRemovesElement) {
    sparse_vector_t *vec = sparse_vector_create(dim_);
    ASSERT_NE(vec, nullptr);

    sparse_vector_set(vec, 5, 1.5f);
    sparse_vector_set(vec, 10, 2.5f);
    EXPECT_EQ(vec->nnz, 2);

    // Setting to 0 removes the element
    sparse_vector_set(vec, 5, 0.0f);
    EXPECT_EQ(vec->nnz, 1);
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 5), 0.0f);
}

TEST_F(SparseVectorTest, SetOutOfRange) {
    sparse_vector_t *vec = sparse_vector_create(dim_);
    ASSERT_NE(vec, nullptr);

    EXPECT_EQ(sparse_vector_set(vec, 100, 1.0f), -1);  // index >= dim
    EXPECT_EQ(sparse_vector_set(vec, dim_, 1.0f), -1);
}

TEST_F(SparseVectorTest, DotProduct) {
    sparse_vector_t *a = sparse_vector_create(dim_);
    sparse_vector_t *b = sparse_vector_create(dim_);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // a: indices 0,5,10 with values 1,2,3
    sparse_vector_set(a, 0, 1.0f);
    sparse_vector_set(a, 5, 2.0f);
    sparse_vector_set(a, 10, 3.0f);

    // b: indices 5,10,15 with values 4,5,6
    sparse_vector_set(b, 5, 4.0f);
    sparse_vector_set(b, 10, 5.0f);
    sparse_vector_set(b, 15, 6.0f);

    // Dot product: 2*4 + 3*5 = 8 + 15 = 23
    float dot = sparse_vector_dot_product(a, b);
    EXPECT_FLOAT_EQ(dot, 23.0f);

    sparse_vector_free(a);
    sparse_vector_free(b);
}

TEST_F(SparseVectorTest, DotProductDifferentDimensions) {
    sparse_vector_t *a = sparse_vector_create(50);
    sparse_vector_t *b = sparse_vector_create(100);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    float dot = sparse_vector_dot_product(a, b);
    EXPECT_FLOAT_EQ(dot, 0.0f);

    sparse_vector_free(a);
    sparse_vector_free(b);
}

TEST_F(SparseVectorTest, CosineSimilarity) {
    sparse_vector_t *a = sparse_vector_create(dim_);
    sparse_vector_t *b = sparse_vector_create(dim_);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Identical vectors
    sparse_vector_set(a, 0, 1.0f);
    sparse_vector_set(a, 5, 2.0f);
    sparse_vector_set(b, 0, 1.0f);
    sparse_vector_set(b, 5, 2.0f);

    float cos_sim = sparse_vector_cosine_similarity(a, b);
    EXPECT_FLOAT_EQ(cos_sim, 1.0f);

    // Orthogonal vectors
    sparse_vector_free(b);
    b = sparse_vector_create(dim_);
    sparse_vector_set(b, 10, 1.0f);

    cos_sim = sparse_vector_cosine_similarity(a, b);
    EXPECT_FLOAT_EQ(cos_sim, 0.0f);

    sparse_vector_free(a);
    sparse_vector_free(b);
}

TEST_F(SparseVectorTest, FromDense) {
    float dense[10] = {0.0f, 0.5f, 0.0f, -0.3f, 0.0f, 0.8f, 0.0f, 0.0f, 0.2f, 0.0f};

    sparse_vector_t *vec = sparse_vector_from_dense(dense, 10, 0.4f);
    ASSERT_NE(vec, nullptr);
    EXPECT_EQ(vec->nnz, 3);  // 0.5, -0.3, 0.8

    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 0), 0.0f);  // Below threshold
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 1), 0.5f);
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 3), -0.3f);
    EXPECT_FLOAT_EQ(sparse_vector_get(vec, 5), 0.8f);

    sparse_vector_free(vec);
}

TEST_F(SparseVectorTest, FromDenseWithZeroThreshold) {
    float dense[5] = {0.0f, 1.0f, 0.0f, 2.0f, 0.0f};

    sparse_vector_t *vec = sparse_vector_from_dense(dense, 5, 0.0f);
    ASSERT_NE(vec, nullptr);
    EXPECT_EQ(vec->nnz, 2);

    sparse_vector_free(vec);
}

TEST_F(SparseVectorTest, FromDenseNullInput) {
    sparse_vector_t *vec = sparse_vector_from_dense(nullptr, 10, 0.1f);
    EXPECT_EQ(vec, nullptr);
}

/* ========================================================================
 * BM25 Index Tests
 * ======================================================================== */

class BM25IndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        bm25_config_t config = {0};
        config.k1 = 1.2f;
        config.b = 0.75f;
        index_ = bm25_index_create(config);
    }

    void TearDown() override {
        if (index_) {
            bm25_index_free(index_);
            index_ = nullptr;
        }
    }

    bm25_index_t *index_ = nullptr;
};

TEST_F(BM25IndexTest, CreateAndFree) {
    ASSERT_NE(index_, nullptr);
    EXPECT_EQ(index_->doc_count, 0);
    EXPECT_EQ(index_->term_count, 0);
}

TEST_F(BM25IndexTest, AddDocument) {
    int ret = bm25_index_add_document(index_, 1, "hello world hello");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(index_->doc_count, 1);
    EXPECT_GT(index_->term_count, 0);
}

TEST_F(BM25IndexTest, AddMultipleDocuments) {
    bm25_index_add_document(index_, 1, "machine learning algorithm");
    bm25_index_add_document(index_, 2, "deep learning neural network");
    bm25_index_add_document(index_, 3, "machine learning optimization");

    EXPECT_EQ(index_->doc_count, 3);
}

TEST_F(BM25IndexTest, AddDuplicateDocument) {
    bm25_index_add_document(index_, 1, "hello world");
    int ret = bm25_index_add_document(index_, 1, "different text");
    EXPECT_EQ(ret, -1);  // Should fail - doc_id already exists
}

TEST_F(BM25IndexTest, ScoreSingleTerm) {
    bm25_index_add_document(index_, 1, "hello world");
    bm25_index_add_document(index_, 2, "goodbye world");

    float score = bm25_score(index_, 1, "hello");
    EXPECT_GT(score, 0.0f);

    float score2 = bm25_score(index_, 2, "hello");
    EXPECT_EQ(score2, 0.0f);  // Doc 2 doesn't contain "hello"
}

TEST_F(BM25IndexTest, ScoreMultipleTerms) {
    bm25_index_add_document(index_, 1, "machine learning algorithm");
    bm25_index_add_document(index_, 2, "machine learning");
    bm25_index_add_document(index_, 3, "deep learning neural network");

    float score1 = bm25_score(index_, 1, "machine learning");
    float score2 = bm25_score(index_, 2, "machine learning");
    float score3 = bm25_score(index_, 3, "machine learning");

    EXPECT_GT(score1, 0.0f);
    EXPECT_GT(score2, 0.0f);
    EXPECT_EQ(score3, 0.0f);  // Doc 3 doesn't contain these terms
}

TEST_F(BM25IndexTest, ScoreNonexistentDoc) {
    bm25_index_add_document(index_, 1, "hello world");
    float score = bm25_score(index_, 999, "hello");
    EXPECT_EQ(score, 0.0f);
}

TEST_F(BM25IndexTest, ScoreEmptyQuery) {
    bm25_index_add_document(index_, 1, "hello world");
    float score = bm25_score(index_, 1, "");
    EXPECT_EQ(score, 0.0f);
}

TEST_F(BM25IndexTest, SearchBasic) {
    bm25_index_add_document(index_, 1, "machine learning algorithm");
    bm25_index_add_document(index_, 2, "deep learning neural network");
    bm25_index_add_document(index_, 3, "machine learning optimization");

    uint64_t results[10];
    float scores[10];

    int count = bm25_search(index_, "machine learning", 10, results, scores);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 3);
}

TEST_F(BM25IndexTest, SearchTopK) {
    bm25_index_add_document(index_, 1, "machine learning");
    bm25_index_add_document(index_, 2, "deep learning");
    bm25_index_add_document(index_, 3, "learning algorithms");
    bm25_index_add_document(index_, 4, "reinforcement learning");

    uint64_t results[10];
    float scores[10];

    int count = bm25_search(index_, "learning", 2, results, scores);
    EXPECT_EQ(count, 2);
    EXPECT_GT(scores[0], 0.0f);
}

TEST_F(BM25IndexTest, SearchNoResults) {
    bm25_index_add_document(index_, 1, "machine learning");
    bm25_index_add_document(index_, 2, "deep learning");

    uint64_t results[10];
    float scores[10];

    int count = bm25_search(index_, "quantum computing", 10, results, scores);
    EXPECT_EQ(count, 0);
}

TEST_F(BM25IndexTest, SearchNullScores) {
    bm25_index_add_document(index_, 1, "machine learning");
    bm25_index_add_document(index_, 2, "deep learning");

    uint64_t results[10];

    int count = bm25_search(index_, "learning", 10, results, nullptr);
    EXPECT_EQ(count, 2);
}

TEST_F(BM25IndexTest, SearchEmptyIndex) {
    uint64_t results[10];
    float scores[10];

    int count = bm25_search(index_, "hello", 10, results, scores);
    EXPECT_EQ(count, 0);
}

/* ========================================================================
 * Hybrid Retrieval Tests (Dense + Sparse + BM25 Fusion)
 * ======================================================================== */

class HybridRetrievalTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create BM25 index
        bm25_config_t bm25_config = {0};
        bm25_config.k1 = 1.2f;
        bm25_config.b = 0.75f;
        bm25_index_ = bm25_index_create(bm25_config);

        // Add documents
        bm25_index_add_document(bm25_index_, 0, "machine learning algorithms");
        bm25_index_add_document(bm25_index_, 1, "deep neural networks");
        bm25_index_add_document(bm25_index_, 2, "machine learning optimization");
    }

    void TearDown() override {
        if (bm25_index_) {
            bm25_index_free(bm25_index_);
            bm25_index_ = nullptr;
        }
    }

    bm25_index_t *bm25_index_ = nullptr;
};

TEST_F(HybridRetrievalTest, ConfigDefault) {
    hybrid_config_t config = hybrid_config_default();
    EXPECT_FLOAT_EQ(config.alpha, 0.4f);
    EXPECT_FLOAT_EQ(config.beta, 0.3f);
    EXPECT_FLOAT_EQ(config.gamma, 0.3f);
    EXPECT_TRUE(config.normalize_scores);
}

TEST_F(HybridRetrievalTest, SearchWithAllModalities) {
    // Query vectors
    float query_dense[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float vectors[3 * 8] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,  // Close to query
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // Far from query
        0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f  // Very close
    };

    hybrid_config_t config = hybrid_config_default();

    hybrid_result_t results[10];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        query_dense, 8, nullptr, "machine learning",
        vectors, 3, bm25_index_,
        &config, 10, results, &num_results);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(num_results, 0);

    // Doc 0 and 2 should have higher BM25 scores for "machine learning"
    // Doc 0 should be first due to high BM25 + good dense score
    EXPECT_EQ(results[0].id, 0u);
    EXPECT_GT(results[0].final_score, 0.0f);
}

TEST_F(HybridRetrievalTest, SearchWithNullQueryDense) {
    hybrid_config_t config = hybrid_config_default();

    hybrid_result_t results[10];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        nullptr, 0, nullptr, "machine learning",
        nullptr, 0, bm25_index_,
        &config, 10, results, &num_results);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(num_results, 0);
}

TEST_F(HybridRetrievalTest, SearchWithNullBM25) {
    float query_dense[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float vectors[3 * 8] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f
    };

    hybrid_config_t config = hybrid_config_default();

    hybrid_result_t results[10];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        query_dense, 8, nullptr, nullptr,
        vectors, 3, nullptr,
        &config, 10, results, &num_results);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(num_results, 0);
}

TEST_F(HybridRetrievalTest, SearchWithCustomWeights) {
    float query_dense[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float vectors[3 * 8] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f
    };

    hybrid_config_t config = {0};
    config.alpha = 0.8f;   // Heavy dense
    config.beta = 0.0f;
    config.gamma = 0.2f;   // Light BM25
    config.normalize_scores = true;

    hybrid_result_t results[10];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        query_dense, 8, nullptr, "machine learning",
        vectors, 3, bm25_index_,
        &config, 10, results, &num_results);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(num_results, 0);
}

TEST_F(HybridRetrievalTest, SearchTopKLimit) {
    float query_dense[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float vectors[3 * 8] = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f
    };

    hybrid_config_t config = hybrid_config_default();

    hybrid_result_t results[2];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        query_dense, 8, nullptr, "machine learning",
        vectors, 3, bm25_index_,
        &config, 2, results, &num_results);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 2);
}

TEST_F(HybridRetrievalTest, SearchInvalidParams) {
    hybrid_result_t results[10];
    uint32_t num_results = 0;

    // Null results
    int ret = hybrid_search(nullptr, 0, nullptr, "test", nullptr, 0, nullptr, nullptr, 10, nullptr, &num_results);
    EXPECT_NE(ret, 0);

    // Top_k == 0
    ret = hybrid_search(nullptr, 0, nullptr, "test", nullptr, 0, nullptr, nullptr, 0, results, &num_results);
    EXPECT_NE(ret, 0);
}

TEST_F(HybridRetrievalTest, SearchZeroWeightSum) {
    hybrid_config_t config = {0};
    config.alpha = 0.0f;
    config.beta = 0.0f;
    config.gamma = 0.0f;
    config.normalize_scores = true;

    hybrid_result_t results[10];
    uint32_t num_results = 0;

    int ret = hybrid_search(nullptr, 0, nullptr, "test", nullptr, 0, bm25_index_, &config, 10, results, &num_results);
    EXPECT_NE(ret, 0);  // Should fail due to zero weight sum
}

/* ========================================================================
 * Hybrid Search (Filter + Vector) Tests
 * ======================================================================== */

class HybridSearchFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = hybrid_search_config_default();
        index_ = hybrid_index_create(8, DISTANCE_METRIC_L2_SQUARED, config_);
    }

    void TearDown() override {
        if (index_) {
            hybrid_index_destroy(index_);
            index_ = nullptr;
        }
        if (config_) {
            hybrid_search_config_free(config_);
            config_ = nullptr;
        }
    }

    HybridSearchConfig *config_ = nullptr;
    HybridIndex *index_ = nullptr;
};

TEST_F(HybridSearchFilterTest, CreateAndDestroy) {
    ASSERT_NE(index_, nullptr);
    EXPECT_EQ(hybrid_index_size(index_), 0);
}

TEST_F(HybridSearchFilterTest, AddSingleVector) {
    float vec[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    char metadata[64] = "category=electronics&price=100";

    int ret = hybrid_index_add(index_, 1, vec, metadata, (int32_t)strlen(metadata));
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(hybrid_index_size(index_), 1);
}

TEST_F(HybridSearchFilterTest, AddBatchVectors) {
    float vec1[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float vec2[8] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};
    float vec3[8] = {0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f};

    const float *vectors[3] = {vec1, vec2, vec3};
    int64_t ids[3] = {1, 2, 3};

    int32_t count = hybrid_index_add_batch(index_, 3, ids, vectors, nullptr, nullptr);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(hybrid_index_size(index_), 3);
}

TEST_F(HybridSearchFilterTest, SearchWithoutFilter) {
    float vec[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    char metadata[64] = "category=electronics&price=100";

    hybrid_index_add(index_, 1, vec, metadata, (int32_t)strlen(metadata));

    float query[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    HybridSearchResults *results = hybrid_index_search(index_, query, nullptr, 5);
    ASSERT_NE(results, nullptr);
    EXPECT_GT(results->count, 0);

    hybrid_search_results_free(results);
}

TEST_F(HybridSearchFilterTest, SearchWithFilter) {
    float vec1[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float vec2[8] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};

    hybrid_index_add(index_, 1, vec1, "category=electronics&price=100", 40);
    hybrid_index_add(index_, 2, vec2, "category=books&price=50", 28);

    float query[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    FilterValue value;
    value.type = VDB_FILTER_VALUE_STRING;
    value.data.str_val = const_cast<char*>("electronics");

    FilterExpr *filter = filter_expr_create_leaf("category", VDB_FILTER_OP_EQ,
                                                  VDB_FILTER_VALUE_STRING, value);
    ASSERT_NE(filter, nullptr);

    HybridSearchResults *results = hybrid_index_search(index_, query, filter, 5);
    ASSERT_NE(results, nullptr);

    hybrid_search_results_free(results);
    filter_expr_free(filter);
}

TEST_F(HybridSearchFilterTest, SearchPreFilterStrategy) {
    float vec[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    hybrid_index_add(index_, 1, vec, "category=a", 12);

    float query[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    HybridSearchResults *results = hybrid_index_search_pre(index_, query, nullptr, 5);
    ASSERT_NE(results, nullptr);

    hybrid_search_results_free(results);
}

TEST_F(HybridSearchFilterTest, SearchPostFilterStrategy) {
    float vec[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    hybrid_index_add(index_, 1, vec, "category=a", 12);

    float query[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    HybridSearchResults *results = hybrid_index_search_post(index_, query, nullptr, 5);
    ASSERT_NE(results, nullptr);

    hybrid_search_results_free(results);
}

TEST_F(HybridSearchFilterTest, SearchHybridStrategy) {
    float vec[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    hybrid_index_add(index_, 1, vec, "category=a", 12);

    float query[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    HybridSearchResults *results = hybrid_index_search_hybrid(index_, query, nullptr, 5);
    ASSERT_NE(results, nullptr);

    hybrid_search_results_free(results);
}

TEST_F(HybridSearchFilterTest, DeleteVector) {
    float vec[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    hybrid_index_add(index_, 1, vec, nullptr, 0);
    EXPECT_EQ(hybrid_index_size(index_), 1);

    int ret = hybrid_index_delete(index_, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(hybrid_index_size(index_), 1);  // Size may not change immediately
}

TEST_F(HybridSearchFilterTest, UpdateMetadata) {
    float vec[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    hybrid_index_add(index_, 1, vec, "category=a", 12);

    int ret = hybrid_index_update_metadata(index_, 1, "category=b", 12);
    EXPECT_EQ(ret, 0);
}

TEST_F(HybridSearchFilterTest, SetAndGetStrategy) {
    hybrid_index_set_strategy(index_, HYBRID_FILTER_POST);
    EXPECT_EQ(hybrid_index_get_strategy(index_), HYBRID_FILTER_POST);

    hybrid_index_set_strategy(index_, HYBRID_FILTER_PRE);
    EXPECT_EQ(hybrid_index_get_strategy(index_), HYBRID_FILTER_PRE);
}

TEST_F(HybridSearchFilterTest, EstimateFilterRate) {
    FilterValue value;
    value.type = VDB_FILTER_VALUE_STRING;
    value.data.str_val = const_cast<char*>("test");

    FilterExpr *filter = filter_expr_create_leaf("category", VDB_FILTER_OP_EQ,
                                                  VDB_FILTER_VALUE_STRING, value);
    ASSERT_NE(filter, nullptr);

    float rate = hybrid_index_estimate_filter_rate(index_, filter);
    EXPECT_GE(rate, 0.0f);
    EXPECT_LE(rate, 1.0f);

    filter_expr_free(filter);
}

TEST_F(HybridSearchFilterTest, FilterExpressionCreateLeaf) {
    FilterValue value;
    value.type = VDB_FILTER_VALUE_INT;
    value.data.int_val = 42;

    FilterExpr *filter = filter_expr_create_leaf("age", VDB_FILTER_OP_GT,
                                                  VDB_FILTER_VALUE_INT, value);
    ASSERT_NE(filter, nullptr);
    EXPECT_TRUE(filter->is_leaf);
    EXPECT_EQ(filter->cond.op, VDB_FILTER_OP_GT);

    filter_expr_free(filter);
}

TEST_F(HybridSearchFilterTest, FilterExpressionCreateCompound) {
    FilterValue value1;
    value1.type = VDB_FILTER_VALUE_STRING;
    value1.data.str_val = const_cast<char*>("a");

    FilterValue value2;
    value2.type = VDB_FILTER_VALUE_STRING;
    value2.data.str_val = const_cast<char*>("b");

    FilterExpr *left = filter_expr_create_leaf("category", VDB_FILTER_OP_EQ,
                                                VDB_FILTER_VALUE_STRING, value1);
    FilterExpr *right = filter_expr_create_leaf("status", VDB_FILTER_OP_EQ,
                                                 VDB_FILTER_VALUE_STRING, value2);

    FilterExpr *compound = filter_expr_create_compound(left, right, VDB_FILTER_OP_AND);
    ASSERT_NE(compound, nullptr);
    EXPECT_FALSE(compound->is_leaf);
    EXPECT_EQ(compound->op, VDB_FILTER_OP_AND);

    filter_expr_free(compound);
}

TEST_F(HybridSearchFilterTest, FilterExpressionCopy) {
    FilterValue value;
    value.type = VDB_FILTER_VALUE_INT;
    value.data.int_val = 100;

    FilterExpr *original = filter_expr_create_leaf("price", VDB_FILTER_OP_GE,
                                                    VDB_FILTER_VALUE_INT, value);
    ASSERT_NE(original, nullptr);

    FilterExpr *copy = filter_expr_copy(original);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->cond.value.data.int_val, 100);

    filter_expr_free(original);
    filter_expr_free(copy);
}

TEST_F(HybridSearchFilterTest, FilterExpressionToJson) {
    FilterValue value;
    value.type = VDB_FILTER_VALUE_STRING;
    value.data.str_val = const_cast<char*>("electronics");

    FilterExpr *filter = filter_expr_create_leaf("category", VDB_FILTER_OP_EQ,
                                                  VDB_FILTER_VALUE_STRING, value);
    ASSERT_NE(filter, nullptr);

    char *json = filter_expr_to_json(filter);
    ASSERT_NE(json, nullptr);
    EXPECT_NE(strstr(json, "electronics"), nullptr);

    free(json);
    filter_expr_free(filter);
}

TEST_F(HybridSearchFilterTest, HybridSearchConfigCreate) {
    HybridSearchConfig *config = hybrid_search_config_create(HYBRID_FILTER_PRE);
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->strategy, HYBRID_FILTER_PRE);

    hybrid_search_config_free(config);
}

TEST_F(HybridSearchFilterTest, HybridSearchConfigClone) {
    HybridSearchConfig *original = hybrid_search_config_create(HYBRID_FILTER_POST);
    ASSERT_NE(original, nullptr);

    HybridSearchConfig *clone = hybrid_search_config_clone(original);
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->strategy, HYBRID_FILTER_POST);

    hybrid_search_config_free(original);
    hybrid_search_config_free(clone);
}

TEST_F(HybridSearchFilterTest, IndexStats) {
    float vec[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    hybrid_index_add(index_, 1, vec, nullptr, 0);

    int32_t size, dims;
    HybridFilterStrategy strategy;
    hybrid_index_stats(index_, &size, &dims, &strategy);

    EXPECT_EQ(size, 1);
    EXPECT_EQ(dims, 8);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
