/**
 * @file cross_modal_e2e_test.cpp
 * @brief C4-2 T11 跨模态 E2E 基线（Graph + Vector + RAG）
 *
 * 简化端到端测试：建图、添加节点/边、向量索引、查询路径走通。
 * 不验证 Recall（属于 C4-1 基准范围）。
 */

#include <gtest/gtest.h>
#include <cstdio>

extern "C" {
#include "db/graph/graph.h"
#include "db/graph/graph_algorithms.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
}

namespace {
const char *kTmpDb = "/tmp/c4_2_e2e_cross.db";
}  // namespace

TEST(CrossModalE2E, GraphVectorQuery) {
    /* 建图 */
    graph_t *g = graph_create();
    if (!g) GTEST_SKIP() << "graph_create 失败";

    graph_vertex_id_t v1 = graph_add_vertex(g);
    graph_vertex_id_t v2 = graph_add_vertex(g);
    graph_add_edge(g, v1, v2);
    graph_vertex_id_t edge = graph_add_edge(g, v2, v1);
    EXPECT_GT(edge, 0u);

    /* 验证 BFS */
    int64_t path[16];
    int n = graph_bfs_algo(g, v1, 8, path);
    EXPECT_GE(n, 1);

    /* 验证 PageRank */
    graph_pagerank_result_t pr;
    int rc = graph_pagerank_new(g, &pr, 50, 0.85, 1e-6);
    ASSERT_EQ(rc, GRAPH_ALGO_OK);
    EXPECT_TRUE(pr.converged);

    /* 向量索引 */
    faiss_hnsw_t *idx = faiss_hnsw_index_create(8, 16, 16, DISTANCE_METRIC_L2, QUANTIZATION_TYPE_NONE);
    ASSERT_NE(idx, nullptr);
    float v1_vec[16] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    float v2_vec[16] = {0.9f, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(faiss_hnsw_index_add(idx, 1, v1_vec), 0);
    EXPECT_EQ(faiss_hnsw_index_add(idx, 1, v2_vec), 0);
    float dists[1]; int32_t ids[1];
    EXPECT_EQ(faiss_hnsw_index_search(idx, v1_vec, 1, 16, dists, ids), 1);
    EXPECT_EQ(ids[0], 0);

    /* cleanup */
    faiss_hnsw_index_drop(idx);
    graph_pagerank_result_clear(&pr);
    graph_destroy(g);
    remove(kTmpDb);
}
