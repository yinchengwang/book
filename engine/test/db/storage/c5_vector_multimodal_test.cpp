#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "db/index/vector_index/hnsw/faiss_hnsw_segment.h"
#include "db/multimodal_object.h"
#include "db/multimodal_search_v2.h"
}

namespace {
constexpr int kDim = 8;
}  // namespace

TEST(C5Vector, CollectionAddSearch) {
    auto *col = faiss_hnsw_collection_create(8, kDim, 16, DISTANCE_METRIC_INNER_PRODUCT, 32);
    ASSERT_NE(col, nullptr);
    float v1[kDim] = {1, 0, 0, 0, 0, 0, 0, 0};
    float v2[kDim] = {0.9f, 0.1f, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(faiss_hnsw_collection_add(col, 1, v1), 0);
    EXPECT_EQ(faiss_hnsw_collection_add(col, 1, v2), 0);
    EXPECT_EQ(faiss_hnsw_collection_ntotal(col), 2);

    float dists[2]; int32_t ids[2];
    int n = faiss_hnsw_collection_search(col, v1, 2, dists, ids);
    EXPECT_EQ(n, 2);
    EXPECT_EQ(ids[0], 0);  // v1 是 id=0
    faiss_hnsw_collection_destroy(col);
}

TEST(C5Multimodal, RRFSearch) {
    auto *col_text = faiss_hnsw_collection_create(8, kDim, 16,
                                                 DISTANCE_METRIC_INNER_PRODUCT, 32);
    ASSERT_NE(col_text, nullptr);

    /* 3 个文档，每个有文本向量 */
    float docs[3][kDim] = {
        {1, 0, 0, 0, 0, 0, 0, 0},  /* id 0 */
        {0.5f, 0.5f, 0, 0, 0, 0, 0, 0},  /* id 1 */
        {0, 1, 0, 0, 0, 0, 0, 0},  /* id 2 */
    };
    for (int i = 0; i < 3; ++i) faiss_hnsw_collection_add(col_text, 1, docs[i]);

    mm_multimodal_object_t query;
    mm_multimodal_object_init(&query);
    memcpy(query.vectors[0].data, docs[0], sizeof(docs[0]));
    query.vectors[0].dim = kDim;
    query.n_vectors = 1;
    strcpy(query.vectors[0].name, "text");

    void *collections[] = { col_text };
    int32_t ids[3];
    double scores[3];
    int n = mm_multimodal_search_v2(&query, collections, 1, 3, nullptr, ids, scores);
    EXPECT_GE(n, 1);
    EXPECT_EQ(ids[0], 0);  /* 自身最相似 */
    faiss_hnsw_collection_destroy(col_text);
    mm_multimodal_object_free(&query);
}

TEST(C5Vector, SegmentCompaction) {
    auto *col = faiss_hnsw_collection_create(8, kDim, 16,
                                             DISTANCE_METRIC_INNER_PRODUCT, 4);
    ASSERT_NE(col, nullptr);
    float v[kDim] = {0};
    /* 写入 12 个（阈值 4 应触发 3 次 compact） */
    for (int i = 0; i < 12; ++i) {
        v[0] = (float)i;
        EXPECT_EQ(faiss_hnsw_collection_add(col, 1, v), 0);
    }
    /* compact 后 n_total 仍为 12（数据未丢） */
    EXPECT_EQ(faiss_hnsw_collection_ntotal(col), 12);
    faiss_hnsw_collection_destroy(col);
}
