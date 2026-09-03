// mmdb_graph_test.cpp — Task 9：图模型（节点/边/BFS/最短路径）测试
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sdk/mmdb.h"
#include "sdk/mmdb_graph.h"

namespace {
constexpr const char* kDbPath = "test_mmdb_graph.db";
}

class MmdbGraphTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;
    mmdb_collection_t* coll_ = nullptr;

    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
        mmdb_schema_t s = {MMDB_MODEL_GRAPH, 0, nullptr, 0};
        coll_ = mmdb_collection_create(db_, "graph", &s);
        ASSERT_NE(coll_, nullptr);
    }
    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }
};

TEST_F(MmdbGraphTest, AddAndDeleteNode) {
    mmdb_node_t n = {"n1", "Person", "{}"};
    EXPECT_EQ(mmdb_graph_add_node(coll_, &n), MMDB_OK);
    EXPECT_EQ(mmdb_graph_delete_node(coll_, "n1"), MMDB_OK);
}

TEST_F(MmdbGraphTest, AddEdgeBetweenNodes) {
    mmdb_node_t a = {"a", "P", "{}"};
    mmdb_node_t b = {"b", "P", "{}"};
    ASSERT_EQ(mmdb_graph_add_node(coll_, &a), MMDB_OK);
    ASSERT_EQ(mmdb_graph_add_node(coll_, &b), MMDB_OK);

    mmdb_edge_t e = {"a", "b", "knows", 1.0, "{}"};
    EXPECT_EQ(mmdb_graph_add_edge(coll_, &e), MMDB_OK);
}

TEST_F(MmdbGraphTest, DeleteNodeRemovesRelatedEdges) {
    mmdb_node_t a = {"a", "P", "{}"};
    mmdb_node_t b = {"b", "P", "{}"};
    mmdb_node_t c = {"c", "P", "{}"};
    ASSERT_EQ(mmdb_graph_add_node(coll_, &a), MMDB_OK);
    ASSERT_EQ(mmdb_graph_add_node(coll_, &b), MMDB_OK);
    ASSERT_EQ(mmdb_graph_add_node(coll_, &c), MMDB_OK);

    mmdb_edge_t e1 = {"a", "b", "x", 1.0, "{}"};
    mmdb_edge_t e2 = {"b", "c", "x", 1.0, "{}"};
    ASSERT_EQ(mmdb_graph_add_edge(coll_, &e1), MMDB_OK);
    ASSERT_EQ(mmdb_graph_add_edge(coll_, &e2), MMDB_OK);

    /* 删除 b：a→b、b→c 都应被清理 */
    EXPECT_EQ(mmdb_graph_delete_node(coll_, "b"), MMDB_OK);

    /* 验证 b 已删除，重新插入时应不重复触发 */
    mmdb_node_t b2 = {"b", "P", "{}"};
    EXPECT_EQ(mmdb_graph_add_node(coll_, &b2), MMDB_OK);
}

TEST_F(MmdbGraphTest, DeleteEdge) {
    mmdb_node_t a = {"a", "P", "{}"};
    mmdb_node_t b = {"b", "P", "{}"};
    ASSERT_EQ(mmdb_graph_add_node(coll_, &a), MMDB_OK);
    ASSERT_EQ(mmdb_graph_add_node(coll_, &b), MMDB_OK);

    mmdb_edge_t e = {"a", "b", "knows", 2.5, "{}"};
    ASSERT_EQ(mmdb_graph_add_edge(coll_, &e), MMDB_OK);

    /* 按 (source, target) 删除 */
    EXPECT_EQ(mmdb_graph_delete_edge(coll_, "a", "b", nullptr), MMDB_OK);
}

TEST_F(MmdbGraphTest, BfsFromStart) {
    mmdb_node_t ns[] = {
        {"n1", "P", "{}"}, {"n2", "P", "{}"}, {"n3", "P", "{}"}, {"n4", "P", "{}"},
    };
    for (auto& n : ns) ASSERT_EQ(mmdb_graph_add_node(coll_, &n), MMDB_OK);

    /* n1-n2, n1-n3, n3-n4 */
    mmdb_edge_t es[] = {
        {"n1", "n2", "x", 1.0, "{}"},
        {"n1", "n3", "x", 1.0, "{}"},
        {"n3", "n4", "x", 1.0, "{}"},
    };
    for (auto& e : es) ASSERT_EQ(mmdb_graph_add_edge(coll_, &e), MMDB_OK);

    mmdb_result_t result;
    ASSERT_EQ(mmdb_graph_bfs(coll_, "n1", 10, &result), MMDB_OK);
    EXPECT_GE(result.count, 3u);
    /* 起始节点必须出现在结果中 */
    bool found_n1 = false;
    for (size_t i = 0; i < result.count; i++) {
        if (result.items[i].id_len == 2 &&
            memcmp(result.items[i].id, "n1", 2) == 0) {
            found_n1 = true;
            break;
        }
    }
    EXPECT_TRUE(found_n1);
    mmdb_result_free(&result);
}

TEST_F(MmdbGraphTest, DfsFromStart) {
    mmdb_node_t a = {"a", "P", "{}"};
    mmdb_node_t b = {"b", "P", "{}"};
    ASSERT_EQ(mmdb_graph_add_node(coll_, &a), MMDB_OK);
    ASSERT_EQ(mmdb_graph_add_node(coll_, &b), MMDB_OK);
    mmdb_edge_t e = {"a", "b", "x", 1.0, "{}"};
    ASSERT_EQ(mmdb_graph_add_edge(coll_, &e), MMDB_OK);

    mmdb_result_t result;
    ASSERT_EQ(mmdb_graph_dfs(coll_, "a", 10, &result), MMDB_OK);
    EXPECT_GE(result.count, 1u);
    mmdb_result_free(&result);
}

TEST_F(MmdbGraphTest, BfsNonexistentStartFails) {
    mmdb_result_t result;
    EXPECT_EQ(mmdb_graph_bfs(coll_, "no_such_node", 10, &result), MMDB_ERR_NOT_FOUND);
    EXPECT_EQ(result.count, 0u);
}

TEST_F(MmdbGraphTest, ShortestPathLinear) {
    /* a→b→c→d 链 */
    mmdb_node_t ns[] = {
        {"a", "P", "{}"}, {"b", "P", "{}"}, {"c", "P", "{}"}, {"d", "P", "{}"},
    };
    for (auto& n : ns) ASSERT_EQ(mmdb_graph_add_node(coll_, &n), MMDB_OK);

    mmdb_edge_t es[] = {
        {"a", "b", "x", 1.0, "{}"},
        {"b", "c", "x", 1.0, "{}"},
        {"c", "d", "x", 1.0, "{}"},
    };
    for (auto& e : es) ASSERT_EQ(mmdb_graph_add_edge(coll_, &e), MMDB_OK);

    mmdb_path_t path;
    ASSERT_EQ(mmdb_graph_shortest_path(coll_, "a", "d", &path), MMDB_OK);
    EXPECT_EQ(path.node_count, 4u);
    EXPECT_STREQ(path.nodes[0].node_id, "a");
    EXPECT_STREQ(path.nodes[1].node_id, "b");
    EXPECT_STREQ(path.nodes[2].node_id, "c");
    EXPECT_STREQ(path.nodes[3].node_id, "d");
    mmdb_path_free(&path);
}

TEST_F(MmdbGraphTest, ShortestPathDisconnected) {
    /* 两点之间没有路径 */
    mmdb_node_t a = {"a", "P", "{}"};
    mmdb_node_t b = {"b", "P", "{}"};
    ASSERT_EQ(mmdb_graph_add_node(coll_, &a), MMDB_OK);
    ASSERT_EQ(mmdb_graph_add_node(coll_, &b), MMDB_OK);

    mmdb_path_t path;
    EXPECT_EQ(mmdb_graph_shortest_path(coll_, "a", "b", &path), MMDB_ERR_NOT_FOUND);
    EXPECT_EQ(path.node_count, 0u);
}

TEST_F(MmdbGraphTest, ShortestPathChoosesShorter) {
    /* a→b→d 总和 2.0；a→c→d 总和 1.5；优先 a→c→d */
    mmdb_node_t ns[] = {
        {"a", "P", "{}"}, {"b", "P", "{}"}, {"c", "P", "{}"}, {"d", "P", "{}"},
    };
    for (auto& n : ns) ASSERT_EQ(mmdb_graph_add_node(coll_, &n), MMDB_OK);

    mmdb_edge_t es[] = {
        {"a", "b", "x", 1.0, "{}"},
        {"b", "d", "x", 1.0, "{}"},
        {"a", "c", "x", 0.5, "{}"},
        {"c", "d", "x", 1.0, "{}"},
    };
    for (auto& e : es) ASSERT_EQ(mmdb_graph_add_edge(coll_, &e), MMDB_OK);

    mmdb_path_t path;
    ASSERT_EQ(mmdb_graph_shortest_path(coll_, "a", "d", &path), MMDB_OK);
    ASSERT_EQ(path.node_count, 3u);
    /* 第二跳应为 c */
    EXPECT_STREQ(path.nodes[1].node_id, "c");
    mmdb_path_free(&path);
}

TEST_F(MmdbGraphTest, WrongCollectionModelFails) {
    mmdb_schema_t vs = {MMDB_MODEL_VECTOR, 0, nullptr, 4};
    mmdb_collection_t* v = mmdb_collection_create(db_, "vec", &vs);
    ASSERT_NE(v, nullptr);

    mmdb_node_t n = {"x", "P", "{}"};
    EXPECT_NE(mmdb_graph_add_node(v, &n), MMDB_OK);
}

TEST_F(MmdbGraphTest, AddNodeRejectsNullInputs) {
    mmdb_node_t n = {nullptr, "P", "{}"};
    EXPECT_NE(mmdb_graph_add_node(coll_, &n), MMDB_OK);
    EXPECT_NE(mmdb_graph_add_edge(coll_, nullptr), MMDB_OK);
}