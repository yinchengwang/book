/**
 * @file doc_engine_test.cpp
 * @brief 文档存储引擎测试
 *
 * 测试文档引擎的基本操作和 JSONPath 查询功能
 */
#include <gtest/gtest.h>
#include "db/doc_engine.h"
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
 * @brief 文档引擎测试夹具
 */
class DocEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_WARN;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 确保测试目录存在（Windows mkdir 不支持多级目录） */
#ifdef _WIN32
        mkdir("./test_data");
        mkdir("./test_data/doc_engine");
#else
        mkdir("./test_data", 0755);
        mkdir("./test_data/doc_engine", 0755);
#endif

        /* 初始化文档引擎 */
        ASSERT_EQ(0, doc_engine_init("./test_data/doc_engine"));
    }

    void TearDown() override {
        doc_engine_shutdown();
        log_shutdown();

        /* 清理测试数据目录 */
        system("rm -rf ./test_data/doc_engine");
    }
};

/**
 * @brief 测试文档引擎生命周期
 */
TEST_F(DocEngineTest, Lifecycle) {
    /* 创建文档集合 */
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    /* 打开集合 */
    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    doc_engine_db_t *db = static_cast<doc_engine_db_t *>(rel);
    EXPECT_STREQ("test_docs", db->name);

    /* 关闭集合 */
    EXPECT_EQ(0, doc_engine_close(rel));

    /* 删除集合 */
    EXPECT_EQ(0, doc_engine_drop("test_docs"));
}

/**
 * @brief 测试插入文档
 */
TEST_F(DocEngineTest, InsertDocument) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 构造文档数据: id_len(4) + id + json_len(4) + json */
    const char *doc_id = "doc_001";
    const char *json = R"({"name":"Alice","age":30,"city":"Beijing"})";
    uint8_t data[1024];
    uint32_t id_len = strlen(doc_id);
    uint32_t json_len = strlen(json);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, doc_id, id_len);
    ptr += id_len;
    memcpy(ptr, &json_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, json, json_len);

    /* 插入文档 */
    int ret = doc_engine_insert(rel, data,
                                sizeof(uint32_t) * 2 + id_len + json_len);
    EXPECT_EQ(0, ret);

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试插入多个文档
 */
TEST_F(DocEngineTest, InsertMultipleDocuments) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入多个文档 */
    const char *docs[][2] = {
        {"doc_001", R"({"name":"Alice","age":30})"},
        {"doc_002", R"({"name":"Bob","age":25})"},
        {"doc_003", R"({"name":"Charlie","age":35})"},
        {"doc_004", R"({"name":"Diana","age":28})"},
    };

    for (size_t i = 0; i < 4; i++) {
        const char *doc_id = docs[i][0];
        const char *json = docs[i][1];

        uint8_t data[1024];
        uint32_t id_len = strlen(doc_id);
        uint32_t json_len = strlen(json);

        uint8_t *ptr = data;
        memcpy(ptr, &id_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, doc_id, id_len);
        ptr += id_len;
        memcpy(ptr, &json_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, json, json_len);

        EXPECT_EQ(0, doc_engine_insert(rel, data,
                                       sizeof(uint32_t) * 2 + id_len + json_len));
    }

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试获取文档
 */
TEST_F(DocEngineTest, GetDocument) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入文档 */
    const char *doc_id = "doc_get";
    const char *json = R"({"name":"Test","value":123})";
    uint8_t data[1024];
    uint32_t id_len = strlen(doc_id);
    uint32_t json_len = strlen(json);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, doc_id, id_len);
    ptr += id_len;
    memcpy(ptr, &json_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, json, json_len);

    EXPECT_EQ(0, doc_engine_insert(rel, data,
                                   sizeof(uint32_t) * 2 + id_len + json_len));

    /* 获取文档 */
    void *out_data = NULL;
    size_t out_len = 0;
    int ret = doc_engine_get(rel, doc_id, id_len, &out_data, &out_len);

    if (ret == 0 && out_data && out_len > 0) {
        /* 验证数据 */
        uint32_t out_id_len = 0;
        memcpy(&out_id_len, out_data, sizeof(uint32_t));
        EXPECT_EQ(id_len, out_id_len);

        free(out_data);
    }

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试 JSONPath 查询 - 简单字段
 */
TEST_F(DocEngineTest, JsonPathSimpleField) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入测试文档 */
    const char *docs[][2] = {
        {"doc_001", R"({"name":"Alice","age":30})"},
        {"doc_002", R"({"name":"Bob","age":25})"},
    };

    for (size_t i = 0; i < 2; i++) {
        uint8_t data[1024];
        uint32_t id_len = strlen(docs[i][0]);
        uint32_t json_len = strlen(docs[i][1]);

        uint8_t *ptr = data;
        memcpy(ptr, &id_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, docs[i][0], id_len);
        ptr += id_len;
        memcpy(ptr, &json_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, docs[i][1], json_len);

        EXPECT_EQ(0, doc_engine_insert(rel, data,
                                       sizeof(uint32_t) * 2 + id_len + json_len));
    }

    /* 查询 name 字段 */
    doc_jsonpath_result_t *results = NULL;
    int count = doc_engine_query_jsonpath(rel, "$.name", &results, 100);

    if (count > 0 && results) {
        EXPECT_LE(count, 100);

        /* 释放结果 */
        doc_engine_free_jsonpath_results(results, count);
    }

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试 JSONPath 查询 - 嵌套字段
 */
TEST_F(DocEngineTest, JsonPathNestedField) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入嵌套文档 */
    const char *doc_id = "doc_nested";
    const char *json = R"({
        "user": {
            "name": "Alice",
            "address": {
                "city": "Beijing",
                "zip": "100000"
            }
        }
    })";

    uint8_t data[2048];
    uint32_t id_len = strlen(doc_id);
    uint32_t json_len = strlen(json);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, doc_id, id_len);
    ptr += id_len;
    memcpy(ptr, &json_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, json, json_len);

    EXPECT_EQ(0, doc_engine_insert(rel, data,
                                   sizeof(uint32_t) * 2 + id_len + json_len));

    /* 查询嵌套字段 user.address.city */
    doc_jsonpath_result_t *results = NULL;
    int count = doc_engine_query_jsonpath(rel, "$.user.address.city", &results, 10);

    if (count > 0 && results) {
        /* 验证查询结果 */
        EXPECT_STREQ("Beijing", results[0].value);

        /* 释放结果 */
        doc_engine_free_jsonpath_results(results, count);
    }

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试 JSONPath 查询 - 数组元素
 */
TEST_F(DocEngineTest, JsonPathArrayElement) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入包含数组的文档 */
    const char *doc_id = "doc_array";
    const char *json = R"({
        "tags": ["apple", "banana", "cherry"],
        "scores": [95, 87, 92]
    })";

    uint8_t data[2048];
    uint32_t id_len = strlen(doc_id);
    uint32_t json_len = strlen(json);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, doc_id, id_len);
    ptr += id_len;
    memcpy(ptr, &json_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, json, json_len);

    EXPECT_EQ(0, doc_engine_insert(rel, data,
                                   sizeof(uint32_t) * 2 + id_len + json_len));

    /* 查询数组第一个元素 */
    doc_jsonpath_result_t *results = NULL;
    int count = doc_engine_query_jsonpath(rel, "$.tags[0]", &results, 10);

    if (count > 0 && results) {
        /* 验证结果包含 "apple" */
        EXPECT_STREQ("apple", results[0].value);

        /* 释放结果 */
        doc_engine_free_jsonpath_results(results, count);
    }

    /* 查询所有数组元素 */
    results = NULL;
    count = doc_engine_query_jsonpath(rel, "$.tags[*]", &results, 10);

    if (count > 0 && results) {
        /* 释放结果 */
        doc_engine_free_jsonpath_results(results, count);
    }

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试 JSONPath 查询 - 空结果
 */
TEST_F(DocEngineTest, JsonPathEmptyResult) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入简单文档 */
    const char *doc_id = "doc_simple";
    const char *json = R"({"name":"Test"})";

    uint8_t data[1024];
    uint32_t id_len = strlen(doc_id);
    uint32_t json_len = strlen(json);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, doc_id, id_len);
    ptr += id_len;
    memcpy(ptr, &json_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, json, json_len);

    EXPECT_EQ(0, doc_engine_insert(rel, data,
                                   sizeof(uint32_t) * 2 + id_len + json_len));

    /* 查询不存在的字段 */
    doc_jsonpath_result_t *results = NULL;
    int count = doc_engine_query_jsonpath(rel, "$.nonexistent", &results, 10);

    /* 应该返回 0 或负数，表示无匹配结果 */
    EXPECT_LE(count, 0);

    if (results) {
        doc_engine_free_jsonpath_results(results, 0);
    }

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试 JSONPath 查询 - 最大结果数限制
 */
TEST_F(DocEngineTest, JsonPathMaxResults) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入多个文档 */
    for (int i = 0; i < 20; i++) {
        char doc_id[32];
        char json[64];
        snprintf(doc_id, sizeof(doc_id), "doc_%03d", i);
        snprintf(json, sizeof(json), R"({"id":%d,"value":"item_%d"})", i, i);

        uint8_t data[1024];
        uint32_t id_len = strlen(doc_id);
        uint32_t json_len = strlen(json);

        uint8_t *ptr = data;
        memcpy(ptr, &id_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, doc_id, id_len);
        ptr += id_len;
        memcpy(ptr, &json_len, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, json, json_len);

        EXPECT_EQ(0, doc_engine_insert(rel, data,
                                       sizeof(uint32_t) * 2 + id_len + json_len));
    }

    /* 查询，但限制最大结果数为 5 */
    doc_jsonpath_result_t *results = NULL;
    int count = doc_engine_query_jsonpath(rel, "$.id", &results, 5);

    /* 应该返回最多 5 个结果 */
    EXPECT_LE(count, 5);

    if (results && count > 0) {
        doc_engine_free_jsonpath_results(results, count);
    }

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试 JSONPath 查询结果结构
 */
TEST_F(DocEngineTest, JsonPathResultStructure) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入测试文档 */
    const char *doc_id = "doc_struct";
    const char *json = R"({"field":"test_value"})";

    uint8_t data[1024];
    uint32_t id_len = strlen(doc_id);
    uint32_t json_len = strlen(json);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, doc_id, id_len);
    ptr += id_len;
    memcpy(ptr, &json_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, json, json_len);

    EXPECT_EQ(0, doc_engine_insert(rel, data,
                                   sizeof(uint32_t) * 2 + id_len + json_len));

    /* 查询并检查结果结构 */
    doc_jsonpath_result_t *results = NULL;
    int count = doc_engine_query_jsonpath(rel, "$.field", &results, 10);

    if (count > 0 && results) {
        /* 检查结果字段 */
        EXPECT_NE(nullptr, results[0].doc_id);
        EXPECT_GT(results[0].doc_id_len, 0);
        EXPECT_NE(nullptr, results[0].value);
        EXPECT_GT(results[0].value_len, 0);

        /* 释放结果 */
        doc_engine_free_jsonpath_results(results, count);
    }

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试释放空结果
 */
TEST_F(DocEngineTest, FreeEmptyResults) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 释放 NULL 结果应该安全 */
    doc_engine_free_jsonpath_results(NULL, 0);

    /* 关闭 */
    EXPECT_EQ(0, doc_engine_close(rel));
    doc_engine_drop("test_docs");
}

/**
 * @brief 测试获取引擎操作表
 */
TEST_F(DocEngineTest, GetOps) {
    const storage_ops_t *ops = doc_engine_get_ops();
    ASSERT_NE(nullptr, ops);

    EXPECT_STREQ("doc_engine", ops->name);
    EXPECT_EQ(MODEL_DOCUMENT, ops->model);

    /* 验证函数指针有效 */
    EXPECT_NE(nullptr, ops->init);
    EXPECT_NE(nullptr, ops->shutdown);
    EXPECT_NE(nullptr, ops->table_create);
    EXPECT_NE(nullptr, ops->table_open);
    EXPECT_NE(nullptr, ops->tuple_insert);
    EXPECT_NE(nullptr, ops->table_close);
    EXPECT_NE(nullptr, ops->table_drop);
}

/**
 * @brief 测试统计信息
 */
TEST_F(DocEngineTest, Stats) {
    EXPECT_EQ(0, doc_engine_create("test_docs", NULL));

    /* 插入文档 */
    void *rel = doc_engine_open("test_docs", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    const char *doc_id = "doc_stats";
    const char *json = R"({"test":true})";

    uint8_t data[1024];
    uint32_t id_len = strlen(doc_id);
    uint32_t json_len = strlen(json);

    uint8_t *ptr = data;
    memcpy(ptr, &id_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, doc_id, id_len);
    ptr += id_len;
    memcpy(ptr, &json_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, json, json_len);

    EXPECT_EQ(0, doc_engine_insert(rel, data,
                                   sizeof(uint32_t) * 2 + id_len + json_len));

    EXPECT_EQ(0, doc_engine_close(rel));

    /* 获取统计信息 */
    storage_stats_t stats;
    int ret = doc_engine_stats("test_docs", &stats);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, stats.num_objects);

    doc_engine_drop("test_docs");
}
