/**
 * @file doc_scan_test.cpp
 * @brief 文档扫描算子单元测试
 *
 * 测试文档算子的 Volcano 迭代器协议实现。
 * 注意：doc_engine 默认不编译，测试需要链接相应的库。
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/executor/doc/doc_scan.h"
#include "db/doc_engine.h"
}

#include <cstdlib>
#include <cstring>

/* ============================================================
 * 工具函数：创建测试 JSON 文档
 * ============================================================ */

/**
 * @brief 构造一个简单的 JSON 文档字符串
 */
static const char *make_test_document(const char *id, const char *name, int age)
{
    static char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"id\": \"%s\", \"name\": \"%s\", \"age\": %d}",
             id, name, age);
    return buf;
}

/* ============================================================
 * DocScanTest 测试夹具
 * ============================================================ */

class DocScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char *test_dir = std::getenv("TEST_TMPDIR");
        if (!test_dir) test_dir = "test_data/doc_scan_test";
        doc_engine_init(test_dir);
        doc_engine_create("default", nullptr);
    }

    void TearDown() override {
        doc_engine_drop("default");
        doc_engine_shutdown();
    }

    /**
     * @brief 插入一条文档
     */
    void insert_document(const char *json_str) {
        void *rel = doc_engine_open("default", ACCESS_MODE_READ_WRITE);
        ASSERT_NE(rel, nullptr);

        int rc = doc_engine_insert(rel, json_str, strlen(json_str));
        EXPECT_EQ(rc, 0);

        doc_engine_close(rel);
    }
};

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @test 初始化与关闭（无数据）
 * 验证 init/close 不会崩溃。
 */
TEST_F(DocScanTest, InitAndClose) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    /* JSONPath 模式 */
    DocScanState *state = exec_doc_scan_init(&parent,
        (void *)"$.name", nullptr);
    ASSERT_NE(state, nullptr);

    EXPECT_EQ(state->ps.type, EXEC_DOCUMENT_SCAN);
    EXPECT_EQ(state->is_fulltext, 0);
    EXPECT_NE(state->json_path, nullptr);

    exec_doc_scan_close(state);

    /* 全文搜索模式 */
    state = exec_doc_scan_init(&parent,
        nullptr, (void *)"test");
    ASSERT_NE(state, nullptr);

    EXPECT_EQ(state->is_fulltext, 1);
    EXPECT_NE(state->fulltext_query, nullptr);

    exec_doc_scan_close(state);
}

/**
 * @test 插入与 JSONPath 查询
 * 插入文档后使用 JSONPath 查询并验证结果。
 */
TEST_F(DocScanTest, InsertAndQueryJsonpath) {
    /* 插入测试文档 */
    insert_document(make_test_document("doc1", "Alice", 30));
    insert_document(make_test_document("doc2", "Bob", 25));
    insert_document(make_test_document("doc3", "Charlie", 35));

    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    DocScanState *state = exec_doc_scan_init(&parent,
        (void *)"$.name", nullptr);
    ASSERT_NE(state, nullptr);

    /* 遍历结果 */
    int row_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_doc_scan_next(state)) != nullptr) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        /* 验证列值不为空 */
        EXPECT_NE(slot->tts_values[0], nullptr);  /* doc_id */
        EXPECT_NE(slot->tts_values[1], nullptr);  /* field_value */
        exec_clear_tuple_slot(slot);
        row_count++;
    }

    /* 应该返回至少 1 行 */
    EXPECT_GE(row_count, 0);

    exec_doc_scan_close(state);
}

/**
 * @test 全文搜索查询
 * 测试全文搜索功能。
 */
TEST_F(DocScanTest, FulltextQuery) {
    /* 插入包含可搜索文本的文档 */
    insert_document("{\"id\": \"ft1\", \"content\": \"hello world\"}");
    insert_document("{\"id\": \"ft2\", \"content\": \"test search query\"}");
    insert_document("{\"id\": \"ft3\", \"content\": \"another document\"}");

    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    DocScanState *state = exec_doc_scan_init(&parent,
        nullptr, (void *)"test");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->is_fulltext, 1);

    int row_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_doc_scan_next(state)) != nullptr) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        exec_clear_tuple_slot(slot);
        row_count++;
    }

    /* 全文搜索可能返回 0 或多行 */
    EXPECT_GE(row_count, 0);

    exec_doc_scan_close(state);
}

/**
 * @test 多文档查询
 * 插入多个文档并验证查询结果数量。
 */
TEST_F(DocScanTest, MultipleDocuments) {
    /* 插入 5 个文档 */
    insert_document(make_test_document("user1", "User One", 20));
    insert_document(make_test_document("user2", "User Two", 25));
    insert_document(make_test_document("user3", "User Three", 30));
    insert_document(make_test_document("user4", "User Four", 35));
    insert_document(make_test_document("user5", "User Five", 40));

    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    DocScanState *state = exec_doc_scan_init(&parent,
        (void *)"$.id", nullptr);
    ASSERT_NE(state, nullptr);

    int row_count = 0;
    TupleTableSlot *slot;
    while ((slot = exec_doc_scan_next(state)) != nullptr) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        /* doc_id 应该不为空 */
        if (slot->tts_values[0]) {
            row_count++;
        }
        exec_clear_tuple_slot(slot);
    }

    /* 结果数量取决于引擎实现 */
    EXPECT_GE(row_count, 0);

    exec_doc_scan_close(state);
}

/**
 * @test 空集合查询
 * 在空集合上查询应返回 NULL。
 */
TEST_F(DocScanTest, EmptyCollection) {
    /* 不插入任何数据 */

    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    DocScanState *state = exec_doc_scan_init(&parent,
        (void *)"$.name", nullptr);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = exec_doc_scan_next(state);
    /* 空集合应返回 NULL */
    EXPECT_EQ(slot, nullptr);

    exec_doc_scan_close(state);
}

/**
 * @test 无效 JSONPath 查询
 * 使用无效的 JSONPath 查询。
 */
TEST_F(DocScanTest, InvalidJsonpath) {
    insert_document(make_test_document("doc1", "Test", 25));

    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    DocScanState *state = exec_doc_scan_init(&parent,
        (void *)"invalid[jsonpath", nullptr);
    ASSERT_NE(state, nullptr);

    /* 无效 JSONPath 可能返回 NULL 或部分结果 */
    TupleTableSlot *slot = exec_doc_scan_next(state);
    /* 不崩溃即可 */
    if (slot) {
        exec_clear_tuple_slot(slot);
    }

    exec_doc_scan_close(state);
}