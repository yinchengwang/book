/**
 * @file graph_scan_test.cpp
 * @brief 图扫描算子测试
 *
 * 测试 Volcano 迭代器协议的全图扫描、BFS/DFS 遍历。
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" {
#include "db/executor/graph/graph_scan.h"
#include "db/graph_engine.h"
#include "db/graph/graph.h"
#include "db/graph/traverse.h"
#include "db/sql/sql_executor.h"
}

/** 测试数据目录 */
#define TEST_DATA_DIR "test_data/graph_scan_test"

class GraphScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 清理测试目录 */
        std::string cmd = "rm -rf " TEST_DATA_DIR;
        system(cmd.c_str());

        /* 初始化图引擎 */
        int ret = graph_engine_init(TEST_DATA_DIR);
        ASSERT_EQ(ret, 0);

        /* 创建图 */
        ret = graph_engine_create("test_graph", NULL);
        ASSERT_EQ(ret, 0);

        /* 打开图 */
        rel = graph_engine_open("test_graph", ACCESS_MODE_READ_WRITE);
        ASSERT_NE(rel, nullptr);

        graph_engine_db_t *db = (graph_engine_db_t *)rel;

        /* 插入 5 个顶点，形成链式结构: 0-1-2-3-4 */
        /* 使用 graph_vertex_create 直接创建，避免 graph_engine_add_vertex 的序列化限制 */
        for (int i = 0; i < 5; i++) {
            char label[32];
            snprintf(label, sizeof(label), "vertex_%d", i);
            graph_vertex_id_t vid = graph_vertex_create(db->graph, label, NULL, 0);
            ASSERT_NE(vid, GRAPH_INVALID_ID);
            vertex_ids[i] = vid;
        }

        /* 添加边形成链式结构: 0->1, 1->2, 2->3, 3->4 */
        for (int i = 0; i < 4; i++) {
            graph_edge_id_t eid = graph_edge_create(db->graph, vertex_ids[i], vertex_ids[i + 1], "knows", NULL, 0);
            ASSERT_NE(eid, GRAPH_INVALID_ID);
        }

        /* 关闭连接，让扫描算子独立打开新连接 */
        graph_engine_close(rel);
        rel = nullptr;
    }

    void TearDown() override {
        if (rel) {
            graph_engine_close(rel);
            rel = nullptr;
        }
        graph_engine_drop("test_graph");
        graph_engine_shutdown();

        /* 清理测试目录 */
        std::string cmd = "rm -rf " TEST_DATA_DIR;
        system(cmd.c_str());
    }

    void *rel;
    graph_vertex_id_t vertex_ids[5];
};

/**
 * @brief 测试 1：InitAndClose — 直接 init 后 close，不调用 next
 */
TEST_F(GraphScanTest, InitAndClose) {
    GraphScanState *state = exec_graph_scan_init(NULL, NULL, -1, NULL, "test_graph");
    ASSERT_NE(state, nullptr);

    /* 不调用 next，直接 close */
    exec_graph_scan_close(state);
}

/**
 * @brief 测试 3：BfsTraversal — BFS 从第一个顶点开始遍历，验证深度
 */
TEST_F(GraphScanTest, BfsTraversal) {
    graph_vertex_id_t *start_vid = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t));
    ASSERT_NE(start_vid, nullptr);
    *start_vid = vertex_ids[0];

    graph_traverse_mode_t mode = GRAPH_TRAVERSE_BFS;

    GraphScanState *state = exec_graph_scan_init(NULL, start_vid, -1, &mode, "test_graph");
    ASSERT_NE(state, nullptr);

    int count = 0;
    int prev_depth = -1;
    TupleTableSlot *slot;
    while ((slot = exec_graph_scan_next(state)) != NULL) {
        graph_vertex_id_t vid = (graph_vertex_id_t)(uintptr_t)slot->tts_values[0];
        int depth = *(int32_t *)&slot->tts_values[1];

        /* 验证深度递增（BFS 按层遍历） */
        EXPECT_GE(depth, prev_depth) << "BFS 深度应递增";
        prev_depth = depth;

        /* 验证顶点在链上 */
        bool found = false;
        for (int i = 0; i < 5; i++) {
            if (vid == vertex_ids[i]) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);

        exec_drop_tuple_slot(slot);
        count++;
    }

    /* BFS 从顶点 0 开始，链式结构应访问所有 5 个顶点 */
    EXPECT_EQ(count, 5);

    exec_graph_scan_close(state);
}

/**
 * @brief 测试 4：DfsTraversal — DFS 遍历
 */
TEST_F(GraphScanTest, DfsTraversal) {
    graph_vertex_id_t *start_vid = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t));
    ASSERT_NE(start_vid, nullptr);
    *start_vid = vertex_ids[0];

    graph_traverse_mode_t mode = GRAPH_TRAVERSE_DFS;

    GraphScanState *state = exec_graph_scan_init(NULL, start_vid, -1, &mode, "test_graph");
    ASSERT_NE(state, nullptr);

    int count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_graph_scan_next(state)) != NULL) {
        graph_vertex_id_t vid = (graph_vertex_id_t)(uintptr_t)slot->tts_values[0];
        bool found = false;
        for (int i = 0; i < 5; i++) {
            if (vid == vertex_ids[i]) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);

        exec_drop_tuple_slot(slot);
        count++;
    }

    /* DFS 从顶点 0 开始，链式结构应访问所有 5 个顶点 */
    EXPECT_EQ(count, 5);

    exec_graph_scan_close(state);
}

/**
 * @brief 测试 5：EmptyGraph — 空图全图扫描，验证返回 0 行
 */
TEST_F(GraphScanTest, EmptyGraph) {
    /* 创建空图 */
    graph_engine_create("empty_graph", NULL);
    void *empty_rel = graph_engine_open("empty_graph", ACCESS_MODE_READ);
    ASSERT_NE(empty_rel, nullptr);

    graph_engine_db_t *db = (graph_engine_db_t *)empty_rel;
    graph_t *g = db->graph;
    ASSERT_NE(g, nullptr);

    /* 使用 graph_scan_vertices 直接扫描空图 */
    int count = 0;
    graph_scan_vertices(g, NULL,
        [](graph_vertex_id_t vid, void *ctx) -> int {
            (void)vid;
            (*(int *)ctx)++;
            return 0;
        }, &count);

    EXPECT_EQ(count, 0) << "空图应返回 0 个顶点";

    graph_engine_close(empty_rel);
    graph_engine_drop("empty_graph");
}

/**
 * @brief 测试 6：DepthLimit — BFS 深度限制，验证只返回深度 <= max_depth 的顶点
 */
TEST_F(GraphScanTest, DepthLimit) {
    /* 重新打开图，因为 SetUp 已关闭 rel */
    void *open_rel = graph_engine_open("test_graph", ACCESS_MODE_READ);
    ASSERT_NE(open_rel, nullptr);
    graph_engine_db_t *db = (graph_engine_db_t *)open_rel;
    graph_t *g = db->graph;

    /* 先获取 vertex_ids（从磁盘重新读取时 ID 仍为 1-5） */
    graph_vertex_id_t local_vids[5];
    for (int i = 0; i < 5; i++) {
        local_vids[i] = (graph_vertex_id_t)(i + 1);
    }

    /* 使用 graph_bfs 便捷函数测试深度限制 */
    int count = 0;
    graph_bfs(g, local_vids[0], 2,
        [](graph_vertex_id_t vid, int depth, void *ctx) -> bool {
            (void)vid;
            (void)depth;
            (*(int *)ctx)++;
            return true;
        }, &count);

    /* 链式结构 0-1-2-3-4，深度 2 应访问 3 个顶点 (0, 1, 2) */
    EXPECT_EQ(count, 3);

    /* 通过 Volcano 算子验证深度限制 */
    graph_vertex_id_t *start_vid = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t));
    ASSERT_NE(start_vid, nullptr);
    *start_vid = local_vids[0];

    graph_traverse_mode_t mode = GRAPH_TRAVERSE_BFS;

    GraphScanState *state = exec_graph_scan_init(NULL, start_vid, 2, &mode, "test_graph");
    ASSERT_NE(state, nullptr);

    int scan_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_graph_scan_next(state)) != NULL) {
        int depth = *(int32_t *)&slot->tts_values[1];
        EXPECT_LE(depth, 2) << "深度不应超过 2";
        scan_count++;
        exec_drop_tuple_slot(slot);
    }

    EXPECT_EQ(scan_count, 3) << "深度限制 2 应返回 3 个顶点";

    exec_graph_scan_close(state);
    graph_engine_close(open_rel);
}