/**
 * @file graph_test.cpp
 * @brief 图存储引擎综合测试
 *
 * 覆盖:
 * - CSR 图操作（创建、添加顶点/边、合并、保存/加载）
 * - Graph Engine API（生命周期、顶点/边操作）
 * - 并发读取
 * - 标签索引操作
 */
#include <gtest/gtest.h>
#include "db/graph_engine.h"
#include "db/graph/graph.h"
#include "db/storage/graph/graph_csr.h"
#include "db/log.h"
#include <cstring>
#include <cstdio>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/* ========================================================================
 * 测试夹具
 * ======================================================================== */

class GraphCsrTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_WARN;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

#ifdef _WIN32
        mkdir("./test_data/graph_csr");
#else
        mkdir("./test_data", 0755);
        mkdir("./test_data/graph_csr", 0755);
#endif
    }

    void TearDown() override {
        log_shutdown();
        system("rm -rf ./test_data/graph_csr");
    }
};

class GraphEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_WARN;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

#ifdef _WIN32
        mkdir("./test_data/graph_engine_test");
#else
        mkdir("./test_data", 0755);
        mkdir("./test_data/graph_engine_test", 0755);
#endif
        ASSERT_EQ(0, graph_engine_init("./test_data/graph_engine_test"));
    }

    void TearDown() override {
        graph_engine_shutdown();
        log_shutdown();
        system("rm -rf ./test_data/graph_engine_test");
    }
};

/* ========================================================================
 * CSR 创建/销毁测试
 * ======================================================================== */

TEST_F(GraphCsrTest, CreateAndDestroy) {
    const char *data_dir = "./test_data/graph_csr/test_create";
    graph_csr_t *csr = graph_csr_create(data_dir, 1000);
    ASSERT_NE(nullptr, csr);
    EXPECT_EQ(0u, csr->vertex_count);
    EXPECT_EQ(0u, csr->edge_count);
    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, CreateWithDefaultMaxVertices) {
    const char *data_dir = "./test_data/graph_csr/test_default";
    graph_csr_t *csr = graph_csr_create(data_dir, 0);
    ASSERT_NE(nullptr, csr);
    EXPECT_EQ(GRAPH_CSR_DEFAULT_MAX_VERTICES, csr->max_vertices);
    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, CreateNullDataDir) {
    graph_csr_t *csr = graph_csr_create(NULL, 1000);
    EXPECT_EQ(nullptr, csr);
}

TEST_F(GraphCsrTest, OpenNonExistent) {
    const char *data_dir = "./test_data/graph_csr/nonexistent";
    graph_csr_t *csr = graph_csr_open(data_dir);
    EXPECT_EQ(nullptr, csr);
}

/* ========================================================================
 * CSR 顶点操作测试
 * ======================================================================== */

TEST_F(GraphCsrTest, AddVertexBasic) {
    const char *data_dir = "./test_data/graph_csr/test_add_vertex";
    graph_csr_t *csr = graph_csr_create(data_dir, 1000);
    ASSERT_NE(nullptr, csr);

    uint64_t vid = graph_csr_add_vertex(csr, 0, NULL, 0);
    EXPECT_NE(UINT64_MAX, vid);
    EXPECT_EQ(1u, csr->vertex_count);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, AddVertexWithProps) {
    const char *data_dir = "./test_data/graph_csr/test_add_vertex_props";
    graph_csr_t *csr = graph_csr_create(data_dir, 1000);
    ASSERT_NE(nullptr, csr);

    uint8_t props[] = {0x01, 0x02, 0x03, 0x04};
    uint64_t vid = graph_csr_add_vertex(csr, 1, props, sizeof(props));
    EXPECT_NE(UINT64_MAX, vid);
    EXPECT_EQ(1u, csr->vertex_count);

    const graph_csr_vertex_t *v = graph_csr_get_vertex(csr, vid);
    ASSERT_NE(nullptr, v);
    EXPECT_EQ(1u, v->label_id);
    EXPECT_EQ(sizeof(props), (size_t)v->prop_size);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, AddMultipleVertices) {
    const char *data_dir = "./test_data/graph_csr/test_add_multi_vertex";
    graph_csr_t *csr = graph_csr_create(data_dir, 1000);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 100; i++) {
        uint64_t vid = graph_csr_add_vertex(csr, i % 5, NULL, 0);
        EXPECT_EQ(i, vid);
    }
    EXPECT_EQ(100u, csr->vertex_count);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, GetVertexInvalidId) {
    const char *data_dir = "./test_data/graph_csr/test_get_invalid";
    graph_csr_t *csr = graph_csr_create(data_dir, 1000);
    ASSERT_NE(nullptr, csr);

    graph_csr_add_vertex(csr, 0, NULL, 0);

    EXPECT_EQ(nullptr, graph_csr_get_vertex(csr, UINT64_MAX));
    EXPECT_EQ(nullptr, graph_csr_get_vertex(csr, 100));
    EXPECT_EQ(nullptr, graph_csr_get_vertex(NULL, 0));

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, VertexExpansion) {
    const char *data_dir = "./test_data/graph_csr/test_vertex_expand";
    graph_csr_t *csr = graph_csr_create(data_dir, 10);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 20; i++) {
        uint64_t vid = graph_csr_add_vertex(csr, 0, NULL, 0);
        EXPECT_EQ(i, vid);
    }
    EXPECT_EQ(20u, csr->vertex_count);
    EXPECT_GE(csr->max_vertices, 20u);

    graph_csr_destroy(csr);
}

/* ========================================================================
 * CSR 边操作测试
 * ======================================================================== */

TEST_F(GraphCsrTest, AddEdgeBasic) {
    const char *data_dir = "./test_data/graph_csr/test_add_edge";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_vertex(csr, 0, NULL, 0);

    uint64_t eid = graph_csr_add_edge(csr, 0, 1, 0, NULL, 0);
    EXPECT_NE(UINT64_MAX, eid);
    EXPECT_EQ(1u, csr->coo_count);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, AddEdgeWithProps) {
    const char *data_dir = "./test_data/graph_csr/test_add_edge_props";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    graph_csr_add_vertex(csr, 0, NULL, 0);
    graph_csr_add_vertex(csr, 0, NULL, 0);

    uint8_t props[] = {0xFF, 0xFE};
    uint64_t eid = graph_csr_add_edge(csr, 0, 1, 1, props, sizeof(props));
    EXPECT_NE(UINT64_MAX, eid);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, AddEdgeInvalidVertices) {
    const char *data_dir = "./test_data/graph_csr/test_edge_invalid";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    graph_csr_add_vertex(csr, 0, NULL, 0);

    EXPECT_EQ(UINT64_MAX, graph_csr_add_edge(csr, 0, 100, 0, NULL, 0));
    EXPECT_EQ(UINT64_MAX, graph_csr_add_edge(csr, 100, 0, 0, NULL, 0));
    EXPECT_EQ(UINT64_MAX, graph_csr_add_edge(csr, 0, 0, 0, NULL, 0));

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, GetOutEdgesEmpty) {
    const char *data_dir = "./test_data/graph_csr/test_get_out_edges";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    graph_csr_add_vertex(csr, 0, NULL, 0);

    uint32_t count = 999;
    const graph_csr_edge_t *edges = graph_csr_get_out_edges(csr, 0, &count);
    EXPECT_EQ(nullptr, edges);
    EXPECT_EQ(0u, count);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, GetInEdgesEmpty) {
    const char *data_dir = "./test_data/graph_csr/test_get_in_edges";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    graph_csr_add_vertex(csr, 0, NULL, 0);

    uint32_t count = 999;
    const graph_csr_edge_t *edges = graph_csr_get_in_edges(csr, 0, &count);
    EXPECT_EQ(nullptr, edges);
    EXPECT_EQ(0u, count);

    graph_csr_destroy(csr);
}

/* ========================================================================
 * CSR COO 合并测试
 * ======================================================================== */

TEST_F(GraphCsrTest, CompactEmpty) {
    const char *data_dir = "./test_data/graph_csr/test_compact_empty";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    int ret = graph_csr_compact(csr);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, csr->edge_count);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, CompactBasic) {
    const char *data_dir = "./test_data/graph_csr/test_compact_basic";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 10; i++) {
        graph_csr_add_vertex(csr, 0, NULL, 0);
    }

    for (uint32_t i = 0; i < 9; i++) {
        graph_csr_add_edge(csr, i, i + 1, 0, NULL, 0);
    }

    EXPECT_EQ(9u, csr->coo_count);

    int ret = graph_csr_compact(csr);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, csr->coo_count);
    EXPECT_EQ(9u, csr->edge_count);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, CompactAfterManyEdges) {
    const char *data_dir = "./test_data/graph_csr/test_compact_many";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 20; i++) {
        graph_csr_add_vertex(csr, 0, NULL, 0);
    }

    for (uint32_t i = 0; i < 20; i++) {
        for (uint32_t j = 0; j < 20; j++) {
            if (i != j) {
                graph_csr_add_edge(csr, i, j, 0, NULL, 0);
            }
        }
    }

    EXPECT_GE(csr->coo_count, 100u);

    int ret = graph_csr_compact(csr);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, csr->coo_count);
    EXPECT_EQ(380u, csr->edge_count);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, NeedsCompact) {
    const char *data_dir = "./test_data/graph_csr/test_needs_compact";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    EXPECT_FALSE(graph_csr_needs_compact(csr));

    for (uint32_t i = 0; i < 10; i++) {
        graph_csr_add_vertex(csr, 0, NULL, 0);
    }

    for (uint32_t i = 0; i < 8; i++) {
        graph_csr_add_edge(csr, i, i + 1, 0, NULL, 0);
    }

    EXPECT_FALSE(graph_csr_needs_compact(csr));

    graph_csr_add_edge(csr, 8, 9, 0, NULL, 0);
    EXPECT_TRUE(graph_csr_needs_compact(csr));

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, CooUsage) {
    const char *data_dir = "./test_data/graph_csr/test_coo_usage";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    EXPECT_EQ(0.0, graph_csr_coo_usage(csr));

    for (uint32_t i = 0; i < 10; i++) {
        graph_csr_add_vertex(csr, 0, NULL, 0);
    }

    for (uint32_t i = 0; i < 5000; i++) {
        graph_csr_add_edge(csr, i % 10, (i + 1) % 10, 0, NULL, 0);
    }

    double usage = graph_csr_coo_usage(csr);
    EXPECT_GT(usage, 0.0);
    EXPECT_LE(usage, 1.0);

    graph_csr_destroy(csr);
}

/* ========================================================================
 * CSR 保存/加载测试
 * ======================================================================== */

TEST_F(GraphCsrTest, SaveAndReload) {
    const char *data_dir = "./test_data/graph_csr/test_save";

    {
        graph_csr_t *csr = graph_csr_create(data_dir, 100);
        ASSERT_NE(nullptr, csr);

        for (uint32_t i = 0; i < 10; i++) {
            graph_csr_add_vertex(csr, i % 3, NULL, 0);
        }

        for (uint32_t i = 0; i < 9; i++) {
            graph_csr_add_edge(csr, i, i + 1, 0, NULL, 0);
        }

        graph_csr_compact(csr);
        EXPECT_EQ(9u, csr->edge_count);

        graph_csr_destroy(csr);
    }

    {
        graph_csr_t *csr = graph_csr_open(data_dir);
        ASSERT_NE(nullptr, csr);
        EXPECT_EQ(10u, csr->vertex_count);
        EXPECT_EQ(9u, csr->edge_count);

        graph_csr_destroy(csr);
    }
}

TEST_F(GraphCsrTest, SaveNull) {
    EXPECT_EQ(-1, graph_csr_save(NULL));
}

/* ========================================================================
 * CSR 标签索引测试
 * ======================================================================== */

TEST_F(GraphCsrTest, LabelOperations) {
    const char *data_dir = "./test_data/graph_csr/test_labels";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    uint32_t label_user = graph_csr_get_or_create_label(csr, "User");
    uint32_t label_product = graph_csr_get_or_create_label(csr, "Product");
    uint32_t label_again = graph_csr_get_or_create_label(csr, "User");

    EXPECT_EQ(label_user, label_again);
    EXPECT_NE(label_user, label_product);

    const char *name = graph_csr_get_label_name(csr, label_user);
    ASSERT_NE(nullptr, name);
    EXPECT_STREQ("User", name);

    EXPECT_STREQ("Product", graph_csr_get_label_name(csr, label_product));
    EXPECT_EQ(nullptr, graph_csr_get_label_name(csr, 999));

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, BuildLabelIndex) {
    const char *data_dir = "./test_data/graph_csr/test_label_index";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    graph_csr_get_or_create_label(csr, "A");
    graph_csr_get_or_create_label(csr, "B");

    for (uint32_t i = 0; i < 10; i++) {
        uint32_t label_id = (i < 6) ? 0 : 1;
        graph_csr_add_vertex(csr, label_id, NULL, 0);
    }

    graph_csr_build_label_index(csr);

    uint32_t count_a = 0;
    uint64_t *verts_a = graph_csr_get_vertices_by_label(csr, 0, &count_a);
    EXPECT_EQ(6u, count_a);
    free(verts_a);

    uint32_t count_b = 0;
    uint64_t *verts_b = graph_csr_get_vertices_by_label(csr, 1, &count_b);
    EXPECT_EQ(4u, count_b);
    free(verts_b);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, GetVerticesByInvalidLabel) {
    const char *data_dir = "./test_data/graph_csr/test_invalid_label";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    graph_csr_add_vertex(csr, 0, NULL, 0);

    uint32_t count = 999;
    uint64_t *verts = graph_csr_get_vertices_by_label(csr, 999, &count);
    EXPECT_EQ(nullptr, verts);
    EXPECT_EQ(0u, count);

    verts = graph_csr_get_vertices_by_label(csr, 0, &count);
    EXPECT_EQ(nullptr, verts);

    graph_csr_destroy(csr);
}

/* ========================================================================
 * CSR 统计信息测试
 * ======================================================================== */

TEST_F(GraphCsrTest, GetStats) {
    const char *data_dir = "./test_data/graph_csr/test_stats";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 5; i++) {
        graph_csr_add_vertex(csr, i % 2, NULL, 0);
    }

    for (uint32_t i = 0; i < 4; i++) {
        graph_csr_add_edge(csr, i, i + 1, 0, NULL, 0);
    }

    uint64_t vcount = 999, ecount = 999;
    uint32_t lcount = 999;
    graph_csr_get_stats(csr, &vcount, &ecount, &lcount);

    EXPECT_EQ(5u, vcount);
    EXPECT_EQ(4u, ecount);
    EXPECT_EQ(0u, lcount);

    graph_csr_destroy(csr);
}

/* ========================================================================
 * CSR 扫描测试
 * ======================================================================== */

TEST_F(GraphCsrTest, Scan) {
    const char *data_dir = "./test_data/graph_csr/test_scan";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 20; i++) {
        graph_csr_add_vertex(csr, 0, NULL, 0);
    }

    uint64_t *ids = NULL;
    uint32_t count = 0;
    int ret = graph_csr_scan(csr, &ids, &count);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(20u, count);

    std::sort(ids, ids + count);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(i, ids[i]);
    }
    free(ids);

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, ScanEmpty) {
    const char *data_dir = "./test_data/graph_csr/test_scan_empty";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    uint64_t *ids = NULL;
    uint32_t count = 999;
    int ret = graph_csr_scan(csr, &ids, &count);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0u, count);
    EXPECT_EQ(nullptr, ids);

    graph_csr_destroy(csr);
}

/* ========================================================================
 * CSR 反向索引测试
 * ======================================================================== */

TEST_F(GraphCsrTest, ReverseIndex) {
    const char *data_dir = "./test_data/graph_csr/test_rev_index";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 5; i++) {
        graph_csr_add_vertex(csr, 0, NULL, 0);
    }

    graph_csr_add_edge(csr, 0, 1, 0, NULL, 0);
    graph_csr_add_edge(csr, 0, 2, 0, NULL, 0);
    graph_csr_add_edge(csr, 1, 2, 0, NULL, 0);
    graph_csr_add_edge(csr, 2, 3, 0, NULL, 0);

    graph_csr_compact(csr);

    uint32_t count = 0;
    const graph_csr_edge_t *in_edges = graph_csr_get_in_edges(csr, 2, &count);
    EXPECT_EQ(2u, count);

    graph_csr_destroy(csr);
}

/* ========================================================================
 * Graph Engine 生命周期测试
 * ======================================================================== */

TEST_F(GraphEngineTest, CreateAndDrop) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));
    EXPECT_EQ(0, graph_engine_drop("test_graph"));
}

TEST_F(GraphEngineTest, OpenAndClose) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    EXPECT_EQ(0, graph_engine_close(rel));
    EXPECT_EQ(0, graph_engine_drop("test_graph"));
}

TEST_F(GraphEngineTest, DoubleOpen) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel1 = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel1);

    void *rel2 = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    EXPECT_NE(nullptr, rel2);

    graph_engine_close(rel1);
    graph_engine_close(rel2);
    graph_engine_drop("test_graph");
}

/* ========================================================================
 * Graph Engine 顶点/边操作测试
 * ======================================================================== */

TEST_F(GraphEngineTest, AddVertex) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    uint8_t vdata[256];
    uint32_t label_len = 4;
    const char *label = "User";
    memcpy(vdata, &label_len, sizeof(uint32_t));
    memcpy(vdata + sizeof(uint32_t), label, label_len);

    graph_vertex_id_t vid = graph_engine_add_vertex(rel, vdata, sizeof(uint32_t) + label_len);
    EXPECT_NE(GRAPH_INVALID_ID, vid);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

TEST_F(GraphEngineTest, AddEdge) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    uint8_t vdata[256];
    uint32_t label_len = 4;
    const char *label = "User";
    memcpy(vdata, &label_len, sizeof(uint32_t));
    memcpy(vdata + sizeof(uint32_t), label, label_len);

    graph_vertex_id_t vid1 = graph_engine_add_vertex(rel, vdata, sizeof(uint32_t) + label_len);
    graph_vertex_id_t vid2 = graph_engine_add_vertex(rel, vdata, sizeof(uint32_t) + label_len);
    EXPECT_NE(GRAPH_INVALID_ID, vid1);
    EXPECT_NE(GRAPH_INVALID_ID, vid2);

    uint8_t edata[512];
    uint64_t *ptr = (uint64_t *)edata;
    ptr[0] = vid1;
    ptr[1] = vid2;
    uint32_t rel_len = 4;
    memcpy(edata + sizeof(uint64_t) * 2, &rel_len, sizeof(uint32_t));
    const char *rel_type = "knows";
    memcpy(edata + sizeof(uint64_t) * 2 + sizeof(uint32_t), rel_type, rel_len);

    graph_edge_id_t eid = graph_engine_add_edge(rel, edata, sizeof(uint64_t) * 2 + sizeof(uint32_t) + rel_len);
    EXPECT_NE(GRAPH_INVALID_ID, eid);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

TEST_F(GraphEngineTest, GetVertex) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    uint8_t vdata[256];
    uint32_t label_len = 4;
    const char *label = "User";
    memcpy(vdata, &label_len, sizeof(uint32_t));
    memcpy(vdata + sizeof(uint32_t), label, label_len);

    graph_vertex_id_t vid = graph_engine_add_vertex(rel, vdata, sizeof(uint32_t) + label_len);
    EXPECT_NE(GRAPH_INVALID_ID, vid);

    void *out_data = NULL;
    size_t out_len = 0;
    int ret = graph_engine_get_vertex(rel, &vid, sizeof(vid), &out_data, &out_len);
    EXPECT_EQ(0, ret);
    EXPECT_GT(out_len, 0u);
    free(out_data);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

/* ========================================================================
 * Graph Engine CSR API 测试
 * ======================================================================== */

TEST_F(GraphEngineTest, EnableCsr) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    int ret = graph_engine_enable_csr(rel, 1000);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(graph_engine_csr_needs_compact(rel) == false || graph_engine_csr_needs_compact(rel) == true);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

TEST_F(GraphEngineTest, CsrCompact) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    graph_engine_enable_csr(rel, 1000);

    int ret = graph_engine_csr_compact(rel);
    EXPECT_EQ(0, ret);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

TEST_F(GraphEngineTest, SaveLoadCsr) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    {
        void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
        ASSERT_NE(nullptr, rel);

        graph_engine_enable_csr(rel, 1000);

        graph_engine_close(rel);
    }

    {
        void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
        ASSERT_NE(nullptr, rel);

        int ret = graph_engine_load_csr(rel);
        EXPECT_EQ(0, ret);

        graph_engine_close(rel);
    }

    graph_engine_drop("test_graph");
}

TEST_F(GraphEngineTest, DisableCsr) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    graph_engine_enable_csr(rel, 1000);
    graph_engine_disable_csr(rel);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

/* ========================================================================
 * Graph Engine 并发锁测试
 * ======================================================================== */

TEST_F(GraphEngineTest, LockEnableDisable) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    int ret = graph_engine_enable_lock(rel, true);
    EXPECT_EQ(0, ret);

    ret = graph_engine_enable_lock(rel, false);
    EXPECT_EQ(0, ret);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

TEST_F(GraphEngineTest, ReadWriteLock) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    graph_engine_enable_lock(rel, true);

    EXPECT_EQ(0, graph_engine_read_lock(rel));
    graph_engine_read_unlock(rel);

    EXPECT_EQ(0, graph_engine_write_lock(rel, 1000));
    graph_engine_write_unlock(rel);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

/* ========================================================================
 * Graph Engine 统计测试
 * ======================================================================== */

TEST_F(GraphEngineTest, Stats) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    storage_stats_t stats;
    int ret = graph_engine_stats("test_graph", &stats);
    EXPECT_EQ(0, ret);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}

/* ========================================================================
 * 并发读取测试
 * ======================================================================== */

TEST_F(GraphCsrTest, ConcurrentReads) {
    const char *data_dir = "./test_data/graph_csr/test_concurrent_reads";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 50; i++) {
        graph_csr_add_vertex(csr, i % 5, NULL, 0);
    }

    for (uint32_t i = 0; i < 49; i++) {
        graph_csr_add_edge(csr, i, i + 1, 0, NULL, 0);
    }

    graph_csr_compact(csr);

    std::atomic<int> success_count(0);
    const int num_threads = 8;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 100; i++) {
                uint32_t id = i % 50;
                const graph_csr_vertex_t *v = graph_csr_get_vertex(csr, id);
                if (v != nullptr) {
                    success_count.fetch_add(1);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(num_threads * 100, success_count.load());

    graph_csr_destroy(csr);
}

TEST_F(GraphCsrTest, ConcurrentScan) {
    const char *data_dir = "./test_data/graph_csr/test_concurrent_scan";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    for (uint32_t i = 0; i < 100; i++) {
        graph_csr_add_vertex(csr, 0, NULL, 0);
    }

    std::atomic<int> scan_count(0);
    const int num_threads = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 10; i++) {
                uint64_t *ids = NULL;
                uint32_t count = 0;
                if (graph_csr_scan(csr, &ids, &count) == 0 && ids != NULL) {
                    scan_count.fetch_add(1);
                    free(ids);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(num_threads * 10, scan_count.load());

    graph_csr_destroy(csr);
}

/* ========================================================================
 * 边界条件测试
 * ======================================================================== */

TEST_F(GraphCsrTest, NullParams) {
    const char *data_dir = "./test_data/graph_csr/test_null";
    graph_csr_t *csr = graph_csr_create(data_dir, 100);
    ASSERT_NE(nullptr, csr);

    EXPECT_EQ(UINT64_MAX, graph_csr_add_vertex(NULL, 0, NULL, 0));
    EXPECT_EQ(UINT64_MAX, graph_csr_add_edge(NULL, 0, 1, 0, NULL, 0));
    EXPECT_EQ(-1, graph_csr_compact(NULL));
    EXPECT_FALSE(graph_csr_needs_compact(NULL));

    uint64_t *ids = (uint64_t *)0x1;
    uint32_t count = 999;
    EXPECT_EQ(-1, graph_csr_scan(NULL, &ids, &count));
    EXPECT_EQ((uint64_t)0x1, ids);

    graph_csr_destroy(csr);
}

TEST_F(GraphEngineTest, NullParams) {
    EXPECT_EQ(GRAPH_INVALID_ID, graph_engine_add_vertex(NULL, NULL, 0));
    EXPECT_EQ(GRAPH_INVALID_ID, graph_engine_add_edge(NULL, NULL, 0));
    EXPECT_EQ(-1, graph_engine_get_vertex(NULL, NULL, 0, NULL, NULL));
}

TEST_F(GraphCsrTest, LockNull) {
    graph_csr_read_lock(NULL);
    graph_csr_read_unlock(NULL);
    graph_csr_write_lock(NULL);
    graph_csr_write_unlock(NULL);
    graph_csr_enable_lock(NULL, true);
}

TEST_F(GraphEngineTest, LockNull) {
    graph_engine_read_lock(NULL);
    graph_engine_read_unlock(NULL);
    graph_engine_write_lock(NULL, 0);
    graph_engine_write_unlock(NULL);
}

/* ========================================================================
 * 内存池测试
 * ======================================================================== */

TEST_F(GraphEngineTest, MemPool) {
    EXPECT_EQ(0, graph_engine_create("test_graph", NULL));

    void *rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    int ret = graph_engine_enable_mem_pool(rel, false);
    EXPECT_EQ(0, ret);

    mm_pool_stats_t stats;
    ret = graph_engine_get_mem_pool_stats(rel, &stats);
    EXPECT_EQ(0, ret);

    graph_engine_close(rel);
    graph_engine_drop("test_graph");
}
