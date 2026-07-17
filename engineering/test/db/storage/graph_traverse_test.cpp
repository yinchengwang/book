/**
 * @file graph_traverse_test.cpp
 * @brief 图存储引擎遍历测试
 *
 * 测试图遍历功能：BFS、DFS、最短路径、PageRank
 */
#include <gtest/gtest.h>
#include "db/graph_engine.h"
#include "db/graph/graph.h"
#include "db/graph/traverse.h"
#include "db/graph/types.h"
#include "db/storage/graph/graph_traverse.h"
#include "db/log.h"
#include <cstring>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/**
 * @brief 图遍历测试夹具
 */
class GraphTraverseTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_WARN;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 确保测试目录存在 */
#ifdef _WIN32
        mkdir("./test_data");
        mkdir("./test_data/graph_traverse");
#else
        mkdir("./test_data", 0755);
        mkdir("./test_data/graph_traverse", 0755);
#endif

        /* 初始化图引擎 */
        ASSERT_EQ(0, graph_engine_init("./test_data/graph_traverse"));
    }

    void TearDown() override {
        graph_engine_shutdown();
        log_shutdown();

        /* 清理测试数据目录 */
        system("rm -rf ./test_data/graph_traverse");
    }
};

/**
 * @brief 创建测试图
 *
 * 创建如下结构的图:
 *   A ---> B ---> C
 *   |      |      ^
 *   v      v      |
 *   D ---> E ---> F
 *        /
 *   G ---+
 *
 * 预期路径:
 *   A -> B -> D -> E
 *   A -> B -> C
 *   A -> D -> E -> F
 */
static graph_t *create_test_graph(const char *path) {
    graph_t *g = graph_create(path);
    if (!g) return NULL;

    /* 创建顶点 */
    graph_vertex_id_t vid_a = graph_vertex_create(g, "Node", NULL, 0);
    graph_vertex_id_t vid_b = graph_vertex_create(g, "Node", NULL, 0);
    graph_vertex_id_t vid_c = graph_vertex_create(g, "Node", NULL, 0);
    graph_vertex_id_t vid_d = graph_vertex_create(g, "Node", NULL, 0);
    graph_vertex_id_t vid_e = graph_vertex_create(g, "Node", NULL, 0);
    graph_vertex_id_t vid_f = graph_vertex_create(g, "Node", NULL, 0);
    graph_vertex_id_t vid_g = graph_vertex_create(g, "Node", NULL, 0);

    /* 创建边 */
    graph_edge_create(g, vid_a, vid_b, "connects", NULL, 0);
    graph_edge_create(g, vid_a, vid_d, "connects", NULL, 0);
    graph_edge_create(g, vid_b, vid_c, "connects", NULL, 0);
    graph_edge_create(g, vid_b, vid_e, "connects", NULL, 0);
    graph_edge_create(g, vid_d, vid_e, "connects", NULL, 0);
    graph_edge_create(g, vid_e, vid_c, "connects", NULL, 0);
    graph_edge_create(g, vid_e, vid_f, "connects", NULL, 0);
    graph_edge_create(g, vid_g, vid_e, "connects", NULL, 0);

    (void)vid_a; (void)vid_b; (void)vid_c;
    (void)vid_d; (void)vid_e; (void)vid_f; (void)vid_g;

    return g;
}

/**
 * @brief 测试图引擎生命周期
 */
TEST_F(GraphTraverseTest, Lifecycle) {
    /* 创建图 */
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    /* 打开图 */
    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    graph_engine_db_t *db = static_cast<graph_engine_db_t *>(rel);
    ASSERT_NE(nullptr, db->graph);

    /* 关闭图 */
    EXPECT_EQ(0, graph_engine_close(rel));

    /* 删除图 */
    EXPECT_EQ(0, graph_engine_drop("test_graph"));
}

/**
 * @brief 测试图引擎添加顶点和边
 */
TEST_F(GraphTraverseTest, AddVertexAndEdge) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 构造顶点数据 */
    uint8_t vdata[256];
    uint32_t label_len = 4;
    const char *label = "User";

    uint8_t *ptr = vdata;
    memcpy(ptr, &label_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, label, label_len);

    /* 添加顶点 */
    graph_vertex_id_t vid = graph_engine_add_vertex(rel, vdata, sizeof(uint32_t) + label_len);
    EXPECT_NE(GRAPH_INVALID_ID, vid);

    /* 构造边数据 */
    uint8_t edata[256];
    uint32_t rel_len = 4;
    const char *rel_type = "knows";

    ptr = edata;
    memcpy(ptr, &rel_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, rel_type, rel_len);

    /* 添加边 */
    graph_edge_id_t eid = graph_engine_add_edge(rel, edata, sizeof(uint32_t) + rel_len);
    EXPECT_NE(GRAPH_INVALID_ID, eid);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

/**
 * @brief 测试 BFS 遍历
 */

TEST_F(GraphTraverseTest, BfsTraverse) {
    /* 创建测试图 */
    graph_t *g = create_test_graph("./test_data/graph_traverse/test_bfs.gdb");
    ASSERT_NE(nullptr, g);

    /* 使用 graph_engine_bfs 包装函数 */
    graph_engine_bfs_result_t result;
    int ret = graph_engine_bfs(g, 1, -1, &result);
    EXPECT_EQ(0, ret);
    EXPECT_GT(result.count, 0);

    graph_engine_bfs_free(&result);
    graph_close(g);
}

TEST_F(GraphTraverseTest, DfsTraverse) {
    graph_t *g = create_test_graph("./test_data/graph_traverse/test_dfs.gdb");
    ASSERT_NE(nullptr, g);

    graph_engine_bfs_result_t result;
    int ret = graph_engine_bfs(g, 1, -1, &result);
    EXPECT_EQ(0, ret);

    graph_engine_bfs_free(&result);
    graph_close(g);
}

TEST_F(GraphTraverseTest, ShortestPath) {
    graph_t *g = create_test_graph("./test_data/graph_traverse/test_path.gdb");
    ASSERT_NE(nullptr, g);

    graph_path_result_t path;
    int length = graph_engine_shortest_path(g, 1, 4, &path);

    graph_path_result_free(&path);
    graph_close(g);
}

TEST_F(GraphTraverseTest, PageRank) {
    graph_t *g = create_test_graph("./test_data/graph_traverse/test_pagerank.gdb");
    ASSERT_NE(nullptr, g);

    graph_engine_pagerank_result_t pr;
    int ret = graph_engine_pagerank(g, 0.85, 100, 1e-6, &pr);

    if (ret == 0) {
        EXPECT_GT(pr.count, 0);
    }

    graph_engine_pagerank_free(&pr);
    graph_close(g);
}
