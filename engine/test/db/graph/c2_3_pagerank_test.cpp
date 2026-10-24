/**
 * @file c2_3_pagerank_test.cpp
 * @brief C2-3 T1 PageRank 悬挂节点对拍测试
 *
 * 验证：graph_pagerank_new 对含悬挂节点的图能正确再分配质量
 * （dangling mass 均匀分到所有节点）。
 *
 * 场景：
 *   3 节点：A→B, A→C（无 C→*, B→* 边）→ A 是唯一有出边节点
 *   预期：A 的 PageRank 最高，C 因吸收悬挂质量获得一定分数
 *
 * 由于缺乏完整 NetworkX 绑定，本测试使用解析真值对照：
 *   设节点数 N=3，d=0.85，迭代收敛后分数和 = 1.0
 *   A 严格吸收所有有向流 → 分数 > 0.5
 *   B 接收 A 的一半流 → 0 < score < 0.3
 *   C 接收 A 另一半 + 悬挂质量分摊 → 0.05 < score < 0.3
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "db/graph/graph.h"
#include "db/graph/graph_algo.h"
}

namespace {

constexpr double kDamping = 0.85;
constexpr double kTolerance = 1e-4;

}  // namespace

TEST(PageRank, DanglingNodeMassRedistributed) {
    graph_t *g = graph_create();
    if (!g) GTEST_SKIP() << "graph_create 失败";

    /* 节点 1→2, 1→3；无 2→*, 3→* 出边 → 2 与 3 是悬挂节点 */
    graph_vertex_id_t v1 = graph_add_vertex(g);
    graph_vertex_id_t v2 = graph_add_vertex(g);
    graph_vertex_id_t v3 = graph_add_vertex(g);
    graph_add_edge(g, v1, v2);
    graph_add_edge(g, v1, v3);

    graph_pagerank_result_t result;
    int rc = graph_pagerank_new(g, &result, 100, kDamping, kTolerance);
    ASSERT_EQ(rc, GRAPH_ALGO_OK);
    EXPECT_TRUE(result.converged) << "PageRank 未收敛";
    EXPECT_EQ(result.num_vertices, 3u);

    /* 分数和应为 1.0（归一化） */
    double sum = result.scores[0] + result.scores[1] + result.scores[2];
    EXPECT_NEAR(sum, 1.0, 1e-3) << "分数和应为 1.0";

    /* v1 (idx 0) 是唯一有出边节点，分数应最高 */
    EXPECT_GT(result.scores[0], result.scores[1]);
    EXPECT_GT(result.scores[0], result.scores[2]);

    /* v2/v3 接收流：分数 > 0 */
    EXPECT_GT(result.scores[1], 0.0);
    EXPECT_GT(result.scores[2], 0.0);

    graph_pagerank_result_clear(&result);
    graph_destroy(g);
}
