/**
 * @file test_tree_yang.c
 * @brief Yang 树模型测试
 *
 * 测试 yang_tree 模块的 NULL 安全检查
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 头文件 */
#include "db/storage/yang/yang_tree.h"
#include "db/storage/yang/yang_engine.h"

/* ========================================================================
 * Yang 树测试夹具
 * ======================================================================== */

class YangTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化 yang 引擎（如果需要） */
    }

    void TearDown() override {
        /* 清理资源 */
    }
};

/* ========================================================================
 * NULL 安全检查测试
 * ======================================================================== */

TEST_F(YangTest, AncestorsNullTree) {
    uint64_t *ids = NULL;
    uint32_t count = 0;
    char *result = yang_sql_ancestors(NULL, "/test");
    EXPECT_EQ(result, nullptr);
}

TEST_F(YangTest, DescendantsNullTree) {
    uint64_t *ids = NULL;
    uint32_t count = 0;
    char *result = yang_sql_descendants(NULL, "/test", -1);
    EXPECT_EQ(result, nullptr);
}

TEST_F(YangTest, AncestorsNullPath) {
    /* 创建一个有效的树实例用于测试 */
    yang_engine_db_t *db = (yang_engine_db_t *)calloc(1, sizeof(yang_engine_db_t));
    db->root = (yang_node_t *)calloc(1, sizeof(yang_node_t));
    db->root->name = strdup("root");
    db->root->path = strdup("/");

    char *result = yang_sql_ancestors(db, NULL);
    EXPECT_EQ(result, nullptr);

    /* 清理 */
    free(db->root->name);
    free(db->root->path);
    free(db->root);
    free(db);
}

TEST_F(YangTest, DescendantsNullPath) {
    /* 创建一个有效的树实例用于测试 */
    yang_engine_db_t *db = (yang_engine_db_t *)calloc(1, sizeof(yang_engine_db_t));
    db->root = (yang_node_t *)calloc(1, sizeof(yang_node_t));
    db->root->name = strdup("root");
    db->root->path = strdup("/");

    char *result = yang_sql_descendants(db, NULL, -1);
    EXPECT_EQ(result, nullptr);

    /* 清理 */
    free(db->root->name);
    free(db->root->path);
    free(db->root);
    free(db);
}
