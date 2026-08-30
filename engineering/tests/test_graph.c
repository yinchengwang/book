/**
 * @file test_graph.c
 * @brief 图存储模态追赶测试
 *
 * 测试 graph_csr（CSR 存储）
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

/* 头文件 */
#include "db/storage/graph/graph_csr.h"

/* 临时测试目录 */
static const char *TEST_DATA_DIR = "./test_graph_data";

static void ensure_test_dir(void) {
#ifdef _WIN32
    _mkdir(TEST_DATA_DIR);
#else
    mkdir(TEST_DATA_DIR, 0755);
#endif
}

static void remove_dir(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
}

/* ========================================================================
 * CSR 存储测试
 * ======================================================================== */

class GraphCsrTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_test_dir();
        char path[600];
        snprintf(path, sizeof(path), "%s/csr_test", TEST_DATA_DIR);
        csr = graph_csr_create(path, 1000);
    }

    void TearDown() override {
        if (csr) {
            graph_csr_destroy(csr);
            csr = NULL;
        }
    }

    graph_csr_t *csr;
};

TEST_F(GraphCsrTest, CreateDestroy) {
    ASSERT_NE(csr, nullptr);
    EXPECT_EQ(csr->vertex_count, 0u);
    EXPECT_EQ(csr->edge_count, 0u);
    EXPECT_EQ(csr->label_count, 0u);
}

TEST_F(GraphCsrTest, AddVertex) {
    uint64_t id = graph_csr_add_vertex(csr, 0, NULL, 0);
    EXPECT_NE(id, UINT64_MAX);
    EXPECT_EQ(id, 0u);
    EXPECT_EQ(csr->vertex_count, 1u);

    id = graph_csr_add_vertex(csr, 1, NULL, 0);
    EXPECT_EQ(id, 1u);
    EXPECT_EQ(csr->vertex_count, 2u);
}

TEST_F(GraphCsrTest, GetVertex) {
    graph_csr_add_vertex(csr, 0, NULL, 0);

    const graph_csr_vertex_t *v = graph_csr_get_vertex(csr, 0);
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->label_id, 0u);

    /* 越界 */
    EXPECT_EQ(graph_csr_get_vertex(csr, 999), nullptr);
}

TEST_F(GraphCsrTest, GetVertexNull) {
    EXPECT_EQ(graph_csr_get_vertex(NULL, 0), nullptr);
}

TEST_F(GraphCsrTest, AddVertexNull) {
    EXPECT_EQ(graph_csr_add_vertex(NULL, 0, NULL, 0), UINT64_MAX);
}

TEST_F(GraphCsrTest, AddEdgeToCoo) {
    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_vertex(csr, 0, NULL, 0);

    uint64_t eid = graph_csr_add_edge(csr, 0, 1, 0, NULL, 0);
    EXPECT_NE(eid, UINT64_MAX);
    EXPECT_EQ(csr->coo_count, 1u);

    eid = graph_csr_add_edge(csr, 1, 0, 1, NULL, 0);
    EXPECT_NE(eid, UINT64_MAX);
    EXPECT_EQ(csr->coo_count, 2u);
}

TEST_F(GraphCsrTest, AddEdgeInvalidVertex) {
    graph_csr_add_vertex(csr, 0, NULL, 0);
    /* dst 不存在 */
    EXPECT_EQ(graph_csr_add_edge(csr, 0, 999, 0, NULL, 0), UINT64_MAX);
}

TEST_F(GraphCsrTest, CompactAndQueryEdges) {
    /* 添加顶点 */
    for (uint64_t i = 0; i < 5; i++) {
        graph_csr_add_vertex(csr, 0, NULL, 0);
    }

    /* 添加边到 COO */
    graph_csr_add_edge(csr, 0, 1, 0, NULL, 0);
    graph_csr_add_edge(csr, 0, 2, 0, NULL, 0);
    graph_csr_add_edge(csr, 1, 3, 0, NULL, 0);

    EXPECT_EQ(csr->coo_count, 3u);

    /* 合并 */
    int ret = graph_csr_compact(csr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(csr->coo_count, 0u);
    EXPECT_EQ(csr->edge_count, 3u);

    /* 查询出边 */
    uint32_t out_count = 0;
    const graph_csr_edge_t *edges = graph_csr_get_out_edges(csr, 0, &out_count);
    ASSERT_NE(edges, nullptr);
    EXPECT_EQ(out_count, 2u);
    EXPECT_EQ(edges[0].dst, 1u);
    EXPECT_EQ(edges[1].dst, 2u);

    edges = graph_csr_get_out_edges(csr, 1, &out_count);
    ASSERT_NE(edges, nullptr);
    EXPECT_EQ(out_count, 1u);
    EXPECT_EQ(edges[0].dst, 3u);

    /* 无出边的顶点 */
    edges = graph_csr_get_out_edges(csr, 4, &out_count);
    EXPECT_EQ(out_count, 0u);
}

TEST_F(GraphCsrTest, GetOutEdgesNull) {
    uint32_t count = 0;
    EXPECT_EQ(graph_csr_get_out_edges(NULL, 0, &count), nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(GraphCsrTest, LabelManagement) {
    uint32_t id1 = graph_csr_get_or_create_label(csr, "Person");
    EXPECT_NE(id1, UINT32_MAX);

    uint32_t id2 = graph_csr_get_or_create_label(csr, "City");
    EXPECT_NE(id2, UINT32_MAX);
    EXPECT_NE(id1, id2);

    /* 重复创建返回相同 ID */
    uint32_t id3 = graph_csr_get_or_create_label(csr, "Person");
    EXPECT_EQ(id3, id1);

    EXPECT_EQ(csr->label_count, 2u);

    /* 获取标签名 */
    const char *name = graph_csr_get_label_name(csr, id1);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Person");

    name = graph_csr_get_label_name(csr, 999);
    EXPECT_EQ(name, nullptr);
}

TEST_F(GraphCsrTest, LabelIndex) {
    uint32_t person_label = graph_csr_get_or_create_label(csr, "Person");
    uint32_t city_label = graph_csr_get_or_create_label(csr, "City");

    graph_csr_add_vertex(csr, person_label, NULL, 0);
    graph_csr_add_vertex(csr, city_label, NULL, 0);
    graph_csr_add_vertex(csr, person_label, NULL, 0);

    graph_csr_build_label_index(csr);

    uint32_t count = 0;
    uint64_t *ids = graph_csr_get_vertices_by_label(csr, person_label, &count);
    ASSERT_NE(ids, nullptr);
    EXPECT_EQ(count, 2u);
    EXPECT_EQ(ids[0], 0u);
    EXPECT_EQ(ids[1], 2u);
    free(ids);

    ids = graph_csr_get_vertices_by_label(csr, city_label, &count);
    ASSERT_NE(ids, nullptr);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(ids[0], 1u);
    free(ids);
}

TEST_F(GraphCsrTest, Stats) {
    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_get_or_create_label(csr, "Test");

    uint64_t vertices = 0, edges = 0;
    uint32_t labels = 0;
    graph_csr_get_stats(csr, &vertices, &edges, &labels);

    EXPECT_EQ(vertices, 2u);
    EXPECT_EQ(edges, 0u);
    EXPECT_EQ(labels, 1u);
}

TEST_F(GraphCsrTest, CooUsage) {
    double usage = graph_csr_coo_usage(csr);
    EXPECT_DOUBLE_EQ(usage, 0.0);

    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_vertex(csr, 0, NULL, 0);

    /* 添加一些边 */
    for (int i = 0; i < 100; i++) {
        graph_csr_add_edge(csr, 0, 1, 0, NULL, 0);
    }

    usage = graph_csr_coo_usage(csr);
    EXPECT_GT(usage, 0.0);
}

TEST_F(GraphCsrTest, NeedsCompact) {
    EXPECT_FALSE(graph_csr_needs_compact(csr));

    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_vertex(csr, 0, NULL, 0);

    /* 填满 80% 的 COO 缓冲区 (8000 edges) */
    for (uint32_t i = 0; i < 8000; i++) {
        graph_csr_add_edge(csr, 0, 1, 0, NULL, 0);
    }

    EXPECT_TRUE(graph_csr_needs_compact(csr));
}

TEST_F(GraphCsrTest, LockEnableDisable) {
    graph_csr_enable_lock(csr, false);
    EXPECT_FALSE(csr->use_lock);

    graph_csr_enable_lock(csr, true);
    EXPECT_TRUE(csr->use_lock);

    /* 不崩溃即可 */
    graph_csr_read_lock(csr);
    graph_csr_read_unlock(csr);
    graph_csr_write_lock(csr);
    graph_csr_write_unlock(csr);
}

TEST_F(GraphCsrTest, SaveAndOpen) {
    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_get_or_create_label(csr, "Test");

    int ret = graph_csr_save(csr);
    EXPECT_EQ(ret, 0);

    /* 重新打开 */
    graph_csr_t *csr2 = graph_csr_open(csr->data_dir);
    ASSERT_NE(csr2, nullptr);
    EXPECT_EQ(csr2->vertex_count, 2u);
    EXPECT_EQ(csr2->label_count, 1u);

    graph_csr_destroy(csr2);
    csr = NULL;  /* 防止 TearDown double-destroy */
}

TEST_F(GraphCsrTest, OpenNonExistent) {
    EXPECT_EQ(graph_csr_open("/nonexistent/path/csr"), nullptr);
}

TEST_F(GraphCsrTest, CreateNull) {
    EXPECT_EQ(graph_csr_create(NULL, 100), nullptr);
}

TEST_F(GraphCsrTest, NullOperations) {
    EXPECT_FALSE(graph_csr_needs_compact(NULL));
    EXPECT_DOUBLE_EQ(graph_csr_coo_usage(NULL), 0.0);
    EXPECT_EQ(graph_csr_get_or_create_label(NULL, "x"), UINT32_MAX);
    EXPECT_EQ(graph_csr_get_label_name(NULL, 0), nullptr);

    uint32_t count = 0;
    EXPECT_EQ(graph_csr_get_vertices_by_label(NULL, 0, &count), nullptr);
}

/* ========================================================================
 * main
 * ======================================================================== */

int main(int argc, char **argv) {
    ensure_test_dir();
    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    remove_dir(TEST_DATA_DIR);
    return ret;
}
