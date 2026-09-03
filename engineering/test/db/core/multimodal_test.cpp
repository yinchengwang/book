/**
 * @file multimodal_test.cpp
 * @brief 多模态查询单元测试
 *
 * 测试内容：
 * - 过滤表达式解析（各种运算符组合）
 * - 过滤执行（单条件、多条件、嵌套条件）
 * - 向量 + BM25 混合检索
 * - RRF 融合验证
 * - 加权融合验证
 * - 性能测试
 */

#include <gtest/gtest.h>
#include <db/core/query_filter.h>
#include <db/core/query_fusion.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// 测试夹具
// ============================================================================

class QueryFilterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// 辅助函数：创建简单的字段回调
// ============================================================================

typedef struct {
    const char *field;
    VDBFilterValueType type;
    VDBFilterValueData data;
} SimpleField;

/** 简单的字段查找回调 */
static int simple_field_callback(void *ctx, const char *field_name, VDBFilterValue *out_value) {
    SimpleField *fields = (SimpleField *)ctx;

    // 简单实现：只支持 price 和 category 字段
    if (strcmp(field_name, "price") == 0) {
        out_value->type = VDB_FILTER_VALUE_INT;
        out_value->data.int_val = fields[0].data.int_val;
        return 0;
    }
    if (strcmp(field_name, "category") == 0) {
        out_value->type = VDB_FILTER_VALUE_STRING;
        // 注意：这里返回的字符串需要被释放
        out_value->data.str_val = strdup(fields[1].data.str_val);
        return 0;
    }
    if (strcmp(field_name, "count") == 0) {
        out_value->type = VDB_FILTER_VALUE_INT;
        out_value->data.int_val = fields[2].data.int_val;
        return 0;
    }
    if (strcmp(field_name, "rating") == 0) {
        out_value->type = VDB_FILTER_VALUE_FLOAT;
        out_value->data.float_val = fields[3].data.float_val;
        return 0;
    }
    if (strcmp(field_name, "active") == 0) {
        out_value->type = VDB_FILTER_VALUE_BOOL;
        out_value->data.bool_val = fields[4].data.bool_val;
        return 0;
    }

    return -1;  // 字段不存在
}

// ============================================================================
// 过滤表达式解析测试
// ============================================================================

TEST_F(QueryFilterTest, ParseSimpleComparison) {
    FilterParseResult *result = filter_parse("price < 100");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    EXPECT_NE(nullptr, result->ast);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseStringComparison) {
    FilterParseResult *result = filter_parse("category = \"electronics\"");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    EXPECT_NE(nullptr, result->ast);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseAndExpression) {
    FilterParseResult *result = filter_parse("price < 100 AND category = \"electronics\"");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    EXPECT_NE(nullptr, result->ast);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseOrExpression) {
    FilterParseResult *result = filter_parse("price < 100 OR price > 500");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    EXPECT_NE(nullptr, result->ast);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseNotExpression) {
    FilterParseResult *result = filter_parse("NOT price < 100");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    EXPECT_NE(nullptr, result->ast);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseComplexExpression) {
    FilterParseResult *result = filter_parse(
        "(status = \"active\" OR status = \"pending\") AND count > 0");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    EXPECT_NE(nullptr, result->ast);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseNestedExpression) {
    FilterParseResult *result = filter_parse(
        "price < 100 AND (category = \"electronics\" OR category = \"books\") AND count > 5");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    EXPECT_NE(nullptr, result->ast);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseAllOperators) {
    // 测试所有比较操作符
    EXPECT_NE(nullptr, filter_parse("price = 100"));
    EXPECT_NE(nullptr, filter_parse("price != 100"));
    EXPECT_NE(nullptr, filter_parse("price < 100"));
    EXPECT_NE(nullptr, filter_parse("price <= 100"));
    EXPECT_NE(nullptr, filter_parse("price > 100"));
    EXPECT_NE(nullptr, filter_parse("price >= 100"));
}

TEST_F(QueryFilterTest, ParseFloatValues) {
    FilterParseResult *result = filter_parse("rating >= 4.5");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseBooleanValues) {
    FilterParseResult *result = filter_parse("active = true");
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(FILTER_PARSE_OK, result->error);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseEmptyExpression) {
    FilterParseResult *result = filter_parse("");
    ASSERT_NE(nullptr, result);
    EXPECT_NE(FILTER_PARSE_OK, result->error);
    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, ParseInvalidExpression) {
    FilterParseResult *result = filter_parse("price < ");
    ASSERT_NE(nullptr, result);
    EXPECT_NE(FILTER_PARSE_OK, result->error);
    filter_parse_result_free(result);
}

// ============================================================================
// 过滤执行测试
// ============================================================================

TEST_F(QueryFilterTest, EvaluateSimpleComparison) {
    FilterParseResult *result = filter_parse("price < 100");
    ASSERT_NE(nullptr, result);
    ASSERT_EQ(FILTER_PARSE_OK, result->error);

    // 设置字段值：price = 50
    SimpleField fields[] = {
        {"price", VDB_FILTER_VALUE_INT, {.int_val = 50}},
        {"", VDB_FILTER_VALUE_STRING, {.str_val = ""}},
        {"", VDB_FILTER_VALUE_INT, {.int_val = 0}},
        {"", VDB_FILTER_VALUE_FLOAT, {.float_val = 0}},
        {"", VDB_FILTER_VALUE_BOOL, {.bool_val = false}}
    };

    FilterContext ctx = {fields, simple_field_callback};

    EXPECT_TRUE(filter_evaluate(result->ast, &ctx));

    // price = 150 应该不匹配
    fields[0].data.int_val = 150;
    EXPECT_FALSE(filter_evaluate(result->ast, &ctx));

    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, EvaluateStringComparison) {
    FilterParseResult *result = filter_parse("category = \"electronics\"");
    ASSERT_NE(nullptr, result);
    ASSERT_EQ(FILTER_PARSE_OK, result->error);

    SimpleField fields[] = {
        {"", VDB_FILTER_VALUE_INT, {.int_val = 0}},
        {"category", VDB_FILTER_VALUE_STRING, {.str_val = "electronics"}},
        {"", VDB_FILTER_VALUE_INT, {.int_val = 0}},
        {"", VDB_FILTER_VALUE_FLOAT, {.float_val = 0}},
        {"", VDB_FILTER_VALUE_BOOL, {.bool_val = false}}
    };

    FilterContext ctx = {fields, simple_field_callback};

    EXPECT_TRUE(filter_evaluate(result->ast, &ctx));

    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, EvaluateAndExpression) {
    FilterParseResult *result = filter_parse("price < 100 AND count > 5");
    ASSERT_NE(nullptr, result);
    ASSERT_EQ(FILTER_PARSE_OK, result->error);

    SimpleField fields[] = {
        {"price", VDB_FILTER_VALUE_INT, {.int_val = 50}},
        {"", VDB_FILTER_VALUE_STRING, {.str_val = ""}},
        {"count", VDB_FILTER_VALUE_INT, {.int_val = 10}},
        {"", VDB_FILTER_VALUE_FLOAT, {.float_val = 0}},
        {"", VDB_FILTER_VALUE_BOOL, {.bool_val = false}}
    };

    FilterContext ctx = {fields, simple_field_callback};

    // 两个条件都满足
    EXPECT_TRUE(filter_evaluate(result->ast, &ctx));

    // price 不满足
    fields[0].data.int_val = 150;
    EXPECT_FALSE(filter_evaluate(result->ast, &ctx));

    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, EvaluateOrExpression) {
    FilterParseResult *result = filter_parse("price < 100 OR count > 5");
    ASSERT_NE(nullptr, result);
    ASSERT_EQ(FILTER_PARSE_OK, result->error);

    SimpleField fields[] = {
        {"price", VDB_FILTER_VALUE_INT, {.int_val = 150}},
        {"", VDB_FILTER_VALUE_STRING, {.str_val = ""}},
        {"count", VDB_FILTER_VALUE_INT, {.int_val = 10}},
        {"", VDB_FILTER_VALUE_FLOAT, {.float_val = 0}},
        {"", VDB_FILTER_VALUE_BOOL, {.bool_val = false}}
    };

    FilterContext ctx = {fields, simple_field_callback};

    // price 不满足，但 count 满足
    EXPECT_TRUE(filter_evaluate(result->ast, &ctx));

    // 两个都不满足
    fields[0].data.int_val = 150;
    fields[2].data.int_val = 3;
    EXPECT_FALSE(filter_evaluate(result->ast, &ctx));

    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, EvaluateNotExpression) {
    FilterParseResult *result = filter_parse("NOT price < 100");
    ASSERT_NE(nullptr, result);
    ASSERT_EQ(FILTER_PARSE_OK, result->error);

    SimpleField fields[] = {
        {"price", VDB_FILTER_VALUE_INT, {.int_val = 150}},
        {"", VDB_FILTER_VALUE_STRING, {.str_val = ""}},
        {"", VDB_FILTER_VALUE_INT, {.int_val = 0}},
        {"", VDB_FILTER_VALUE_FLOAT, {.float_val = 0}},
        {"", VDB_FILTER_VALUE_BOOL, {.bool_val = false}}
    };

    FilterContext ctx = {fields, simple_field_callback};

    EXPECT_TRUE(filter_evaluate(result->ast, &ctx));

    // price < 100 时应该不匹配
    fields[0].data.int_val = 50;
    EXPECT_FALSE(filter_evaluate(result->ast, &ctx));

    filter_parse_result_free(result);
}

TEST_F(QueryFilterTest, EvaluateFloatComparison) {
    FilterParseResult *result = filter_parse("rating >= 4.5");
    ASSERT_NE(nullptr, result);
    ASSERT_EQ(FILTER_PARSE_OK, result->error);

    SimpleField fields[] = {
        {"", VDB_FILTER_VALUE_INT, {.int_val = 0}},
        {"", VDB_FILTER_VALUE_STRING, {.str_val = ""}},
        {"", VDB_FILTER_VALUE_INT, {.int_val = 0}},
        {"rating", VDB_FILTER_VALUE_FLOAT, {.float_val = 4.8}},
        {"", VDB_FILTER_VALUE_BOOL, {.bool_val = false}}
    };

    FilterContext ctx = {fields, simple_field_callback};

    EXPECT_TRUE(filter_evaluate(result->ast, &ctx));

    filter_parse_result_free(result);
}

// ============================================================================
// 结果融合测试
// ============================================================================

class QueryFusionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // 创建简单的向量检索结果
    static QueryResultItem make_vector_item(int64_t id, double score, int rank) {
        QueryResultItem item = {0};
        item.id = id;
        item.vector_score = score;
        item.bm25_score = 0;
        item.vector_rank = rank;
        item.bm25_rank = 0;
        item.source = SOURCE_VECTOR;
        return item;
    }

    // 创建简单的 BM25 检索结果
    static QueryResultItem make_bm25_item(int64_t id, double score, int rank) {
        QueryResultItem item = {0};
        item.id = id;
        item.vector_score = 0;
        item.bm25_score = score;
        item.vector_rank = 0;
        item.bm25_rank = rank;
        item.source = SOURCE_BM25;
        return item;
    }
};

TEST_F(QueryFusionTest, RRFWithEmptyResults) {
    QueryResultItem vector_items[3] = {
        make_vector_item(1, 0.95, 1),
        make_vector_item(2, 0.90, 2),
        make_vector_item(3, 0.85, 3)
    };

    FusionConfig *config = fusion_config_create_rrf(60, 10);

    // 空 BM25 结果
    QueryResultList *result = fusion_rrf(vector_items, 3, NULL, 0, config);
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(3, result->count);

    query_result_list_free(result);
    fusion_config_free(config);
}

TEST_F(QueryFusionTest, RRFWithOverlappingResults) {
    QueryResultItem vector_items[3] = {
        make_vector_item(1, 0.95, 1),
        make_vector_item(2, 0.90, 2),
        make_vector_item(3, 0.85, 3)
    };

    QueryResultItem bm25_items[3] = {
        make_bm25_item(2, 0.9, 1),  // 与向量结果重叠
        make_bm25_item(4, 0.8, 2),  // 新结果
        make_bm25_item(5, 0.7, 3)   // 新结果
    };

    FusionConfig *config = fusion_config_create_rrf(60, 10);

    QueryResultList *result = fusion_rrf(vector_items, 3, bm25_items, 3, config);
    ASSERT_NE(nullptr, result);

    // ID 2 应该在结果中（两个结果都有贡献）
    bool found_id_2 = false;
    for (int i = 0; i < result->count; i++) {
        if (result->items[i].id == 2) {
            found_id_2 = true;
            // ID 2 的分数应该更高（两个检索都有贡献）
            printf("ID 2 score: %.4f\n", result->items[i].combined_score);
            break;
        }
    }
    EXPECT_TRUE(found_id_2);

    query_result_list_free(result);
    fusion_config_free(config);
}

TEST_F(QueryFusionTest, RRFWithDifferentRanks) {
    // 测试 RRF 公式：1/(k+rank)
    QueryResultItem vector_items[3] = {
        make_vector_item(1, 0.95, 1),
        make_vector_item(2, 0.90, 2),
        make_vector_item(3, 0.85, 3)
    };

    FusionConfig *config = fusion_config_create_rrf(60, 10);

    QueryResultList *result = fusion_rrf(vector_items, 3, NULL, 0, config);
    ASSERT_NE(nullptr, result);

    // 排名 1 的分数应该最高
    EXPECT_EQ(1, result->items[0].id);
    EXPECT_GT(result->items[0].combined_score, result->items[1].combined_score);

    query_result_list_free(result);
    fusion_config_free(config);
}

TEST_F(QueryFusionTest, WeightedFusion) {
    QueryResultItem vector_items[3] = {
        make_vector_item(1, 0.95, 1),
        make_vector_item(2, 0.90, 2),
        make_vector_item(3, 0.85, 3)
    };

    QueryResultItem bm25_items[3] = {
        make_bm25_item(2, 0.8, 1),
        make_bm25_item(4, 0.7, 2),
        make_bm25_item(5, 0.6, 3)
    };

    FusionConfig *config = fusion_config_create_weighted(0.5, 0.5, 10);

    QueryResultList *result = fusion_weighted(vector_items, 3, bm25_items, 3, config);
    ASSERT_NE(nullptr, result);

    // ID 2 应该在结果中
    bool found_id_2 = false;
    for (int i = 0; i < result->count; i++) {
        if (result->items[i].id == 2) {
            found_id_2 = true;
            break;
        }
    }
    EXPECT_TRUE(found_id_2);

    query_result_list_free(result);
    fusion_config_free(config);
}

TEST_F(QueryFusionTest, FusionWithTopK) {
    QueryResultItem vector_items[10];
    for (int i = 0; i < 10; i++) {
        vector_items[i] = make_vector_item(i + 1, 1.0 - i * 0.05, i + 1);
    }

    FusionConfig *config = fusion_config_create_rrf(60, 5);

    QueryResultList *result = fusion_rrf(vector_items, 10, NULL, 0, config);
    ASSERT_NE(nullptr, result);

    // 应该只返回 top_k = 5 个结果
    EXPECT_EQ(5, result->count);

    query_result_list_free(result);
    fusion_config_free(config);
}

TEST_F(QueryFusionTest, FusionExecute) {
    QueryResultItem vector_items[2] = {
        make_vector_item(1, 0.9, 1),
        make_vector_item(2, 0.8, 2)
    };

    FusionConfig *config = fusion_config_create(FUSION_STRATEGY_RRF);
    config->top_k = 10;
    config->rrf_k = 60;

    QueryResultList *result = fusion_execute(vector_items, 2, NULL, 0, config);
    ASSERT_NE(nullptr, result);
    EXPECT_EQ(2, result->count);

    query_result_list_free(result);
    fusion_config_free(config);
}

// ============================================================================
// 性能测试
// ============================================================================

TEST_F(QueryFilterTest, DISABLED_PerformanceFilterParsing) {
    // 这个测试用于性能基准测试，默认禁用
    const char *expr = "price < 100 AND category = \"electronics\" AND count > 5";

    int64_t start = 0;
    int iterations = 10000;

    start = clock();
    for (int i = 0; i < iterations; i++) {
        FilterParseResult *result = filter_parse(expr);
        if (result && result->ast) {
            filter_ast_free(result->ast);
        }
        filter_parse_result_free(result);
    }

    printf("Filter parsing: %d iterations in %.2f ms\n",
           iterations, (double)(clock() - start) / CLOCKS_PER_SEC * 1000);
}

TEST_F(QueryFusionTest, DISABLED_PerformanceFusion) {
    // 这个测试用于性能基准测试，默认禁用
    const int n = 1000;
    QueryResultItem *items = new QueryResultItem[n];
    for (int i = 0; i < n; i++) {
        items[i] = make_vector_item(i + 1, 1.0 - (double)i / n, i + 1);
    }

    FusionConfig *config = fusion_config_create_rrf(60, 100);

    int64_t start = clock();
    int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        QueryResultList *result = fusion_rrf(items, n, NULL, 0, config);
        query_result_list_free(result);
    }

    printf("RRF fusion: %d iterations with %d items in %.2f ms\n",
           iterations, n, (double)(clock() - start) / CLOCKS_PER_SEC * 1000);

    delete[] items;
    fusion_config_free(config);
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
