/**
 * @file test_sparse.c
 * @brief 稀疏向量 + BM25 + 混合检索集成测试
 */
#include "db/sparse_vector.h"
#include "db/bm25_index.h"
#include "db/hybrid_retrieval.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>

static void test_sparse_vector_basic(void) {
    printf("=== test_sparse_vector_basic ===\n");
    sparse_vector_t *v = sparse_vector_create(10);
    assert(v != NULL);
    assert(v->dim == 10);
    assert(v->nnz == 0);

    /* set/get */
    assert(sparse_vector_set(v, 2, 3.0f) == 0);
    assert(sparse_vector_set(v, 5, -1.0f) == 0);
    assert(sparse_vector_set(v, 9, 2.5f) == 0);
    assert(v->nnz == 3);
    assert(fabsf(sparse_vector_get(v, 2) - 3.0f) < 1e-6f);
    assert(fabsf(sparse_vector_get(v, 5) - (-1.0f)) < 1e-6f);
    assert(fabsf(sparse_vector_get(v, 0)) < 1e-6f);

    /* 设置为 0 应删除 */
    assert(sparse_vector_set(v, 5, 0.0f) == 0);
    assert(v->nnz == 2);
    assert(fabsf(sparse_vector_get(v, 5)) < 1e-6f);

    /* 越界 */
    assert(sparse_vector_set(v, 10, 1.0f) == -1);

    sparse_vector_free(v);
    printf("PASSED\n");
}

static void test_sparse_dot_product(void) {
    printf("=== test_sparse_dot_product ===\n");
    sparse_vector_t *a = sparse_vector_create(5);
    sparse_vector_t *b = sparse_vector_create(5);

    sparse_vector_set(a, 0, 1.0f);
    sparse_vector_set(a, 1, 2.0f);
    sparse_vector_set(a, 3, 3.0f);

    sparse_vector_set(b, 0, 0.5f);
    sparse_vector_set(b, 2, 1.0f);
    sparse_vector_set(b, 3, 2.0f);

    float dot = sparse_vector_dot_product(a, b);
    /* 1*0.5 + 2*0 + 3*0 + 3*2 = 0.5 + 6 = 6.5 */
    assert(fabsf(dot - 6.5f) < 1e-5f);

    float cos = sparse_vector_cosine_similarity(a, b);
    assert(cos > 0 && cos <= 1.0f);

    sparse_vector_free(a);
    sparse_vector_free(b);
    printf("PASSED\n");
}

static void test_from_dense(void) {
    printf("=== test_from_dense ===\n");
    float dense[] = {0.0f, 1.0f, 0.0f, 2.0f, 0.5f};
    sparse_vector_t *v = sparse_vector_from_dense(dense, 5, 0.1f);
    assert(v != NULL);
    assert(v->nnz == 3);
    assert(fabsf(sparse_vector_get(v, 1) - 1.0f) < 1e-6f);
    assert(fabsf(sparse_vector_get(v, 3) - 2.0f) < 1e-6f);
    assert(fabsf(sparse_vector_get(v, 4) - 0.5f) < 1e-6f);
    sparse_vector_free(v);
    printf("PASSED\n");
}

static void test_bm25_basic(void) {
    printf("=== test_bm25_basic ===\n");
    bm25_config_t cfg = {.k1 = 1.2f, .b = 0.75f};
    bm25_index_t *idx = bm25_index_create(cfg);
    assert(idx != NULL);

    /* 添加更多文档，确保 "cat" 的 IDF 为正 */
    assert(bm25_index_add_document(idx, 1, "the cat sat on the mat today") == 0);
    assert(bm25_index_add_document(idx, 2, "the dog chased the cat in garden") == 0);
    assert(bm25_index_add_document(idx, 3, "the bird flew over the tree high") == 0);
    assert(bm25_index_add_document(idx, 4, "a rabbit jumped near the old barn") == 0);
    assert(bm25_index_add_document(idx, 5, "the fish swam in the deep ocean") == 0);

    /* 重复文档 */
    assert(bm25_index_add_document(idx, 1, "duplicate") == -1);

    /* 单文档评分 - "cat" 在 5 个文档中出现 2 次，IDF = log(4.5/2.5) > 0 */
    float s1 = bm25_score(idx, 1, "cat");
    float s2 = bm25_score(idx, 2, "cat");
    float s3 = bm25_score(idx, 3, "cat");
    assert(s1 > 0);
    assert(s2 > 0);
    assert(s3 == 0); /* 不含 cat 的文档得分应为 0 */
    (void)s1; (void)s2; /* 消除未使用警告 */

    /* 搜索 "cat" - 应该返回包含 cat 的文档（doc 1 和 doc 2），按分数降序 */
    uint64_t results[10];
    float scores[10];
    int count = bm25_search(idx, "cat", 10, results, scores);
    assert(count >= 1);  /* 至少返回包含 cat 的文档 */
    assert(scores[0] >= 0);
    assert(results[0] == 1 || results[0] == 2); /* 第一名应该是 doc 1 或 doc 2 */

    bm25_index_free(idx);
    printf("PASSED\n");
}

static void test_hybrid_search(void) {
    printf("=== test_hybrid_search ===\n");
    bm25_config_t cfg = {.k1 = 1.2f, .b = 0.75f};
    bm25_index_t *idx = bm25_index_create(cfg);
    bm25_index_add_document(idx, 0, "the cat sat on the mat");
    bm25_index_add_document(idx, 1, "the dog chased the cat");
    bm25_index_add_document(idx, 2, "the bird flew over the tree");

    /* 模拟稠密向量 (3 个 4 维) */
    float vectors[] = {
        1.0f, 0.0f, 0.0f, 0.0f,  /* doc 0 */
        0.8f, 0.6f, 0.0f, 0.0f,  /* doc 1 */
        0.0f, 0.0f, 0.0f, 1.0f,  /* doc 2 */
    };
    float query[] = {1.0f, 0.0f, 0.0f, 0.0f};

    hybrid_config_t hc = hybrid_config_default();
    hybrid_result_t results[10];
    uint32_t num_results = 0;

    int rc = hybrid_search(query, 4, NULL, "cat", (void*)vectors, 3, idx, &hc,
                           3, results, &num_results);
    assert(rc == 0);
    assert(num_results > 0);
    printf("  hybrid_search 返回 %u 个结果, 最高分=%.4f\n", num_results, results[0].final_score);

    bm25_index_free(idx);
    printf("PASSED\n");
}

int main(void) {
    test_sparse_vector_basic();
    test_sparse_dot_product();
    test_from_dense();
    test_bm25_basic();
    test_hybrid_search();
    printf("\n=== All sparse/BM25/hybrid tests PASSED ===\n");
    return 0;
}
