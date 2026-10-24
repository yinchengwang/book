/**
 * @file graph_test.cpp
 * @brief 图数据库单元测试
 */
#include <gtest/gtest.h>
#include "db/graph/graph.h"
#include "db/graph/traverse.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief 清理数据库文件和 WAL 文件
 */
static void cleanup_db(const char *path) {
    remove(path);
    char wal_path[256];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", path);
    remove(wal_path);
}

/* ============================================================
 * 基础 CRUD 测试
 * ============================================================ */

TEST(GraphBasicTest, CreateVertex) {
    const char *db_path = "test_graph_basic.db";
    cleanup_db(db_path);

    graph_t *g = graph_create(db_path);
    ASSERT_NE(g, nullptr);

    graph_prop_t props[2];
    strcpy(props[0].key, "name");
    props[0].type = GRAPH_STRING;
    props[0].value.u.string_val.str = strdup("Alice");
    props[0].value.u.string_val.len = 5;

    strcpy(props[1].key, "age");
    props[1].type = GRAPH_INT;
    props[1].value.u.int_val = 30;

    graph_vertex_id_t vid = graph_vertex_create(g, "Person", props, 2);
    EXPECT_NE(vid, GRAPH_INVALID_ID);
    EXPECT_TRUE(graph_vertex_exists(g, vid));

    graph_vertex_t *vertex = nullptr;
    ASSERT_EQ(graph_vertex_get(g, vid, &vertex), 0);
    EXPECT_EQ(vertex->n_labels, 1);
    free(vertex);

    graph_close(g);
    cleanup_db(db_path);
}

TEST(GraphBasicTest, VertexExists) {
    const char *db_path = "test_graph_exists.db";
    cleanup_db(db_path);

    graph_t *g = graph_create(db_path);
    ASSERT_NE(g, nullptr);

    graph_vertex_id_t vid = graph_vertex_create(g, "Test", nullptr, 0);
    EXPECT_NE(vid, GRAPH_INVALID_ID);
    EXPECT_TRUE(graph_vertex_exists(g, vid));
    EXPECT_FALSE(graph_vertex_exists(g, 9999));

    graph_close(g);
    cleanup_db(db_path);
}

TEST(GraphBasicTest, LabelManagement) {
    const char *db_path = "test_graph_label.db";
    cleanup_db(db_path);

    graph_t *g = graph_create(db_path);
    ASSERT_NE(g, nullptr);

    /* 获取/创建标签 */
    graph_label_id_t label1 = graph_get_or_create_label(g, "Person");
    EXPECT_NE(label1, 0);

    graph_label_id_t label2 = graph_get_or_create_label(g, "Person");
    EXPECT_EQ(label1, label2);

    graph_label_id_t label3 = graph_get_or_create_label(g, "Company");
    EXPECT_NE(label3, label1);

    const char *name = graph_get_label_name(g, label1);
    EXPECT_STREQ(name, "Person");

    graph_close(g);
    cleanup_db(db_path);
}

TEST(GraphBasicTest, RelTypeManagement) {
    const char *db_path = "test_graph_reltype.db";
    cleanup_db(db_path);

    graph_t *g = graph_create(db_path);
    ASSERT_NE(g, nullptr);

    graph_label_id_t type1 = graph_get_or_create_rel_type(g, "KNOWS");
    EXPECT_NE(type1, 0);

    graph_label_id_t type2 = graph_get_or_create_rel_type(g, "KNOWS");
    EXPECT_EQ(type1, type2);

    const char *name = graph_get_rel_type_name(g, type1);
    EXPECT_STREQ(name, "KNOWS");

    graph_close(g);
    cleanup_db(db_path);
}

TEST(GraphBasicTest, Stats) {
    const char *db_path = "test_graph_stats.db";
    cleanup_db(db_path);

    graph_t *g = graph_create(db_path);
    ASSERT_NE(g, nullptr);

    graph_vertex_create(g, "A", nullptr, 0);
    graph_vertex_create(g, "B", nullptr, 0);

    graph_stats_t stats;
    EXPECT_EQ(graph_get_stats(g, &stats), 0);
    EXPECT_EQ(stats.num_vertices, 2u);
    EXPECT_EQ(stats.num_labels, 2u);  // A, B

    graph_close(g);
    cleanup_db(db_path);
}

TEST(GraphBasicTest, DeleteVertex) {
    const char *db_path = "test_graph_delete.db";
    cleanup_db(db_path);

    graph_t *g = graph_create(db_path);
    ASSERT_NE(g, nullptr);

    graph_vertex_id_t vid = graph_vertex_create(g, "Test", nullptr, 0);
    EXPECT_TRUE(graph_vertex_exists(g, vid));

    EXPECT_EQ(graph_vertex_delete(g, vid), 0);
    EXPECT_FALSE(graph_vertex_exists(g, vid));

    graph_close(g);
    cleanup_db(db_path);
}

TEST(GraphBasicTest, ScanVertices) {
    const char *db_path = "test_graph_scan.db";
    cleanup_db(db_path);

    graph_t *g = graph_create(db_path);
    ASSERT_NE(g, nullptr);

    graph_vertex_create(g, "User", nullptr, 0);
    graph_vertex_create(g, "User", nullptr, 0);
    graph_vertex_create(g, "Admin", nullptr, 0);

    std::vector<graph_vertex_id_t> all_vertices;
    auto callback = [](graph_vertex_id_t vid, void *ctx) -> int {
        auto *vec = static_cast<std::vector<graph_vertex_id_t>*>(ctx);
        vec->push_back(vid);
        return 0;
    };

    size_t count = graph_scan_vertices(g, nullptr, callback, &all_vertices);
    EXPECT_EQ(count, 3u);

    std::vector<graph_vertex_id_t> user_vertices;
    count = graph_scan_vertices(g, "User", callback, &user_vertices);
    EXPECT_EQ(count, 2u);

    graph_close(g);
    cleanup_db(db_path);
}

TEST(GraphBasicTest, BFS) {
    const char *db_path = "test_graph_bfs.db";
    cleanup_db(db_path);

    graph_t *g = graph_create(db_path);
    ASSERT_NE(g, nullptr);

    // 创建简单图: 1 -> 2, 1 -> 3
    graph_vertex_id_t v1 = graph_vertex_create(g, "Node", nullptr, 0);
    graph_vertex_id_t v2 = graph_vertex_create(g, "Node", nullptr, 0);
    graph_vertex_id_t v3 = graph_vertex_create(g, "Node", nullptr, 0);

    // 创建边
    graph_edge_id_t eid1 = graph_edge_create(g, v1, v2, "NEXT", nullptr, 0);
    graph_edge_id_t eid2 = graph_edge_create(g, v1, v3, "NEXT", nullptr, 0);

    EXPECT_NE(eid1, GRAPH_INVALID_ID);
    EXPECT_NE(eid2, GRAPH_INVALID_ID);

    // BFS 遍历
    std::vector<graph_vertex_id_t> visited;
    auto bfs_callback = [](graph_vertex_id_t vid, int depth, void *ctx) -> bool {
        (void)depth;
        auto *vec = static_cast<std::vector<graph_vertex_id_t>*>(ctx);
        vec->push_back(vid);
        return true;
    };

    size_t count = graph_bfs(g, v1, 3, bfs_callback, &visited);
    EXPECT_EQ(count, 3u);

    graph_close(g);
    cleanup_db(db_path);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
