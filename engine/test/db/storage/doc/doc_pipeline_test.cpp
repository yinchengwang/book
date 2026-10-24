/**
 * @file doc_pipeline_test.cpp
 * @brief 文档聚合管道测试
 */

#include <gtest/gtest.h>
#include "db/storage/doc/doc_pipeline.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

/* ========================================================================
 * 表达式测试
 * ======================================================================== */

TEST(DocPipelineExprTest, CreateFieldExpr) {
    DocExpr *expr = doc_expr_create_field("name");
    ASSERT_NE(nullptr, expr);
    EXPECT_EQ(DOC_EXPR_FIELD, expr->op);
    EXPECT_STREQ("name", expr->field_name);
    doc_expr_free(expr);
}

TEST(DocPipelineExprTest, CreateConstIntExpr) {
    DocExpr *expr = doc_expr_create_const_int(42);
    ASSERT_NE(nullptr, expr);
    EXPECT_EQ(DOC_EXPR_CONST, expr->op);
    EXPECT_EQ(DOC_EXPR_TYPE_INT, expr->type);
    EXPECT_EQ(42, expr->int_value);
    doc_expr_free(expr);
}

TEST(DocPipelineExprTest, CreateConstDoubleExpr) {
    DocExpr *expr = doc_expr_create_const_double(3.14);
    ASSERT_NE(nullptr, expr);
    EXPECT_EQ(DOC_EXPR_CONST, expr->op);
    EXPECT_EQ(DOC_EXPR_TYPE_DOUBLE, expr->type);
    EXPECT_NEAR(3.14, expr->num_value, 0.001);
    doc_expr_free(expr);
}

TEST(DocPipelineExprTest, CreateConstStringExpr) {
    DocExpr *expr = doc_expr_create_const_string("hello");
    ASSERT_NE(nullptr, expr);
    EXPECT_EQ(DOC_EXPR_CONST, expr->op);
    EXPECT_EQ(DOC_EXPR_TYPE_STRING, expr->type);
    EXPECT_STREQ("hello", expr->str_value);
    doc_expr_free(expr);
}

TEST(DocPipelineExprTest, CreateConstBoolExpr) {
    DocExpr *expr = doc_expr_create_const_bool(true);
    ASSERT_NE(nullptr, expr);
    EXPECT_EQ(DOC_EXPR_CONST, expr->op);
    EXPECT_EQ(DOC_EXPR_TYPE_BOOL, expr->type);
    EXPECT_TRUE(expr->bool_value);
    doc_expr_free(expr);
}

TEST(DocPipelineExprTest, CreateBinaryExpr) {
    DocExpr *left = doc_expr_create_field("age");
    DocExpr *right = doc_expr_create_const_int(18);
    DocExpr *expr = doc_expr_create_binary(DOC_EXPR_GE, left, right);

    ASSERT_NE(nullptr, expr);
    EXPECT_EQ(DOC_EXPR_GE, expr->op);
    EXPECT_EQ(2, expr->num_args);

    doc_expr_free(expr);
}

TEST(DocPipelineExprTest, CloneExpr) {
    DocExpr *expr = doc_expr_create_field("test");
    DocExpr *clone = doc_expr_clone(expr);

    ASSERT_NE(nullptr, clone);
    EXPECT_EQ(expr->op, clone->op);
    EXPECT_STREQ(expr->field_name, clone->field_name);

    doc_expr_free(expr);
    doc_expr_free(clone);
}

/* ========================================================================
 * $match 阶段测试
 * ======================================================================== */

TEST(DocPipelineMatchTest, CreateMatchStage) {
    DocExpr *filter = doc_expr_create_field("status");
    DocMatchStage *stage = doc_match_stage_create(filter);

    ASSERT_NE(nullptr, stage);
    EXPECT_NE(nullptr, stage->filter);
    EXPECT_FALSE(stage->use_index);

    doc_match_stage_free(stage);
}

TEST(DocPipelineMatchTest, CreateMatchStageWithNull) {
    DocMatchStage *stage = doc_match_stage_create(nullptr);

    ASSERT_NE(nullptr, stage);
    EXPECT_EQ(nullptr, stage->filter);

    doc_match_stage_free(stage);
}

/* ========================================================================
 * $group 阶段测试
 * ======================================================================== */

TEST(DocPipelineGroupTest, CreateGroupStage) {
    DocExpr *group_id = doc_expr_create_field("category");
    DocGroupStage *stage = doc_group_stage_create(group_id, "_id");

    ASSERT_NE(nullptr, stage);
    EXPECT_NE(nullptr, stage->group_id);
    EXPECT_STREQ("_id", stage->group_id_field);
    EXPECT_EQ(0, stage->num_accumulators);

    doc_group_stage_free(stage);
}

TEST(DocPipelineGroupTest, AddAccumulator) {
    DocGroupStage *stage = doc_group_stage_create(NULL, "_id");

    ASSERT_NE(nullptr, stage);
    EXPECT_EQ(0, stage->num_accumulators);

    DocExpr *count_expr = doc_expr_create_const_int(1);
    int ret = doc_group_stage_add_accumulator(stage, "count", DOC_ACC_COUNT, count_expr);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, stage->num_accumulators);
    EXPECT_STREQ("count", stage->accumulators[0].name);
    EXPECT_EQ(DOC_ACC_COUNT, stage->accumulators[0].type);

    doc_expr_free(count_expr);
    doc_group_stage_free(stage);
}

/* ========================================================================
 * $sort 阶段测试
 * ======================================================================== */

TEST(DocPipelineSortTest, CreateSortStage) {
    DocSortField fields[] = {
        {"age", 1},
        {"name", -1}
    };

    DocSortStage *stage = doc_sort_stage_create(fields, 2);

    ASSERT_NE(nullptr, stage);
    EXPECT_EQ(2, stage->num_fields);
    EXPECT_STREQ("age", stage->fields[0].field);
    EXPECT_EQ(1, stage->fields[0].direction);
    EXPECT_STREQ("name", stage->fields[1].field);
    EXPECT_EQ(-1, stage->fields[1].direction);

    doc_sort_stage_free(stage);
}

TEST(DocPipelineSortTest, AddSortField) {
    DocSortStage *stage = doc_sort_stage_create(nullptr, 0);

    ASSERT_NE(nullptr, stage);
    EXPECT_EQ(0, stage->num_fields);

    int ret = doc_sort_stage_add_field(stage, "score", -1);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, stage->num_fields);
    EXPECT_EQ(-1, stage->fields[0].direction);

    doc_sort_stage_free(stage);
}

/* ========================================================================
 * $limit 和 $skip 阶段测试
 * ======================================================================== */

TEST(DocPipelineLimitTest, CreateLimitStage) {
    DocLimitStage *stage = doc_limit_stage_create(100);

    ASSERT_NE(nullptr, stage);
    EXPECT_EQ(100, stage->limit);

    doc_limit_stage_free(stage);
}

TEST(DocPipelineSkipTest, CreateSkipStage) {
    DocSkipStage *stage = doc_skip_stage_create(50);

    ASSERT_NE(nullptr, stage);
    EXPECT_EQ(50, stage->skip);

    doc_skip_stage_free(stage);
}

/* ========================================================================
 * $project 阶段测试
 * ======================================================================== */

TEST(DocPipelineProjectTest, CreateProjectStage) {
    DocProjectField fields[] = {
        {"name", nullptr, true},
        {"age", nullptr, true}
    };

    DocProjectStage *stage = doc_project_stage_create(fields, 2);

    ASSERT_NE(nullptr, stage);
    EXPECT_EQ(2, stage->num_fields);
    EXPECT_FALSE(stage->exclude_id);

    doc_project_stage_free(stage);
}

/* ========================================================================
 * 管道测试
 * ======================================================================== */

TEST(DocPipelineTest, CreatePipeline) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);

    ASSERT_NE(nullptr, pipeline);
    EXPECT_EQ(nullptr, pipeline->head);
    EXPECT_EQ(nullptr, pipeline->tail);
    EXPECT_EQ(0, pipeline->num_stages);

    doc_pipeline_free(pipeline);
}

TEST(DocPipelineTest, AddMatchStage) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    DocExpr *filter = doc_expr_create_field("status");

    int ret = doc_pipeline_add_match(pipeline, filter);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, pipeline->num_stages);
    EXPECT_NE(nullptr, pipeline->head);
    EXPECT_EQ(pipeline->head, pipeline->tail);

    doc_expr_free(filter);
    doc_pipeline_free(pipeline);
}

TEST(DocPipelineTest, AddGroupStage) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    DocExpr *group_id = doc_expr_create_field("category");

    int ret = doc_pipeline_add_group(pipeline, group_id, "_id");

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, pipeline->num_stages);

    doc_expr_free(group_id);
    doc_pipeline_free(pipeline);
}

TEST(DocPipelineTest, AddSortStage) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    DocSortField fields[] = {{"age", 1}};

    int ret = doc_pipeline_add_sort(pipeline, fields, 1);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, pipeline->num_stages);

    doc_pipeline_free(pipeline);
}

TEST(DocPipelineTest, AddLimitStage) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);

    int ret = doc_pipeline_add_limit(pipeline, 10);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, pipeline->num_stages);

    doc_pipeline_free(pipeline);
}

TEST(DocPipelineTest, AddSkipStage) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);

    int ret = doc_pipeline_add_skip(pipeline, 5);

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, pipeline->num_stages);

    doc_pipeline_free(pipeline);
}

TEST(DocPipelineTest, MultipleStages) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);

    /* 添加 $match */
    DocExpr *filter = doc_expr_create_field("active");
    doc_pipeline_add_match(pipeline, filter);
    doc_expr_free(filter);

    /* 添加 $group */
    DocExpr *group_id = doc_expr_create_field("category");
    doc_pipeline_add_group(pipeline, group_id, "_id");
    doc_expr_free(group_id);

    /* 添加 $sort */
    DocSortField sort_field = {"count", -1};
    doc_pipeline_add_sort(pipeline, &sort_field, 1);

    /* 添加 $limit */
    doc_pipeline_add_limit(pipeline, 10);

    EXPECT_EQ(4, pipeline->num_stages);

    doc_pipeline_free(pipeline);
}

/* ========================================================================
 * 管道执行器测试
 * ======================================================================== */

TEST(DocPipelineExecutorTest, CreateExecutor) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    doc_pipeline_add_limit(pipeline, 10);

    DocPipelineExecutor *exec = doc_pipeline_executor_create(pipeline, nullptr);

    ASSERT_NE(nullptr, exec);
    EXPECT_EQ(pipeline, exec->pipeline);
    EXPECT_EQ(0, exec->num_output_docs);

    doc_pipeline_executor_free(exec);
}

TEST(DocPipelineExecutorTest, ExecuteWithMatch) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    doc_pipeline_add_match(pipeline, nullptr);
    doc_pipeline_add_limit(pipeline, 10);

    DocPipelineExecutor *exec = doc_pipeline_executor_create(pipeline, nullptr);

    /* 测试数据 */
    const char *docs[] = {
        "{\"name\":\"Alice\",\"age\":25}",
        "{\"name\":\"Bob\",\"age\":30}",
        "{\"name\":\"Charlie\",\"age\":35}"
    };

    char **results = nullptr;
    int num_results = doc_pipeline_execute(exec, docs, 3, &results);

    EXPECT_GE(num_results, 0);
    EXPECT_NE(nullptr, results);

    /* 清理结果 */
    for (int i = 0; i < num_results; i++) {
        free(results[i]);
    }
    free(results);

    doc_pipeline_executor_free(exec);
}

TEST(DocPipelineExecutorTest, ExecuteWithGroup) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    doc_pipeline_add_group(pipeline, nullptr, "_id");

    DocPipelineExecutor *exec = doc_pipeline_executor_create(pipeline, nullptr);

    /* 测试数据 */
    const char *docs[] = {
        "{\"name\":\"Alice\",\"category\":\"A\"}",
        "{\"name\":\"Bob\",\"category\":\"B\"}",
        "{\"name\":\"Charlie\",\"category\":\"A\"}"
    };

    char **results = nullptr;
    int num_results = doc_pipeline_execute(exec, docs, 3, &results);

    EXPECT_GE(num_results, 0);

    /* 清理结果 */
    for (int i = 0; i < num_results; i++) {
        free(results[i]);
    }
    free(results);

    doc_pipeline_executor_free(exec);
}

TEST(DocPipelineExecutorTest, ExecuteWithSort) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);

    DocSortField field = {"age", -1};  /* 降序 */
    doc_pipeline_add_sort(pipeline, &field, 1);

    DocPipelineExecutor *exec = doc_pipeline_executor_create(pipeline, nullptr);

    /* 测试数据 */
    const char *docs[] = {
        "{\"name\":\"Alice\",\"age\":25}",
        "{\"name\":\"Bob\",\"age\":30}",
        "{\"name\":\"Charlie\",\"age\":35}"
    };

    char **results = nullptr;
    int num_results = doc_pipeline_execute(exec, docs, 3, &results);

    EXPECT_GE(num_results, 0);
    EXPECT_NE(nullptr, results);

    /* 清理结果 */
    for (int i = 0; i < num_results; i++) {
        free(results[i]);
    }
    free(results);

    doc_pipeline_executor_free(exec);
}

TEST(DocPipelineExecutorTest, ExecuteWithLimit) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    doc_pipeline_add_limit(pipeline, 2);

    DocPipelineExecutor *exec = doc_pipeline_executor_create(pipeline, nullptr);

    /* 测试数据 */
    const char *docs[] = {
        "{\"name\":\"Alice\"}",
        "{\"name\":\"Bob\"}",
        "{\"name\":\"Charlie\"}",
        "{\"name\":\"David\"}"
    };

    char **results = nullptr;
    int num_results = doc_pipeline_execute(exec, docs, 4, &results);

    EXPECT_GE(num_results, 0);

    /* 清理结果 */
    for (int i = 0; i < num_results; i++) {
        free(results[i]);
    }
    free(results);

    doc_pipeline_executor_free(exec);
}

TEST(DocPipelineExecutorTest, ExecuteWithSkip) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    doc_pipeline_add_skip(pipeline, 2);

    DocPipelineExecutor *exec = doc_pipeline_executor_create(pipeline, nullptr);

    /* 测试数据 */
    const char *docs[] = {
        "{\"name\":\"Alice\"}",
        "{\"name\":\"Bob\"}",
        "{\"name\":\"Charlie\"}",
        "{\"name\":\"David\"}"
    };

    char **results = nullptr;
    int num_results = doc_pipeline_execute(exec, docs, 4, &results);

    EXPECT_GE(num_results, 0);

    /* 清理结果 */
    for (int i = 0; i < num_results; i++) {
        free(results[i]);
    }
    free(results);

    doc_pipeline_executor_free(exec);
}

TEST(DocPipelineExecutorTest, ExecuteEmptyDocs) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    doc_pipeline_add_limit(pipeline, 10);

    DocPipelineExecutor *exec = doc_pipeline_executor_create(pipeline, nullptr);

    char **results = nullptr;
    int num_results = doc_pipeline_execute(exec, nullptr, 0, &results);

    EXPECT_EQ(0, num_results);

    doc_pipeline_executor_free(exec);
}

/* ========================================================================
 * 便捷函数测试
 * ======================================================================== */

TEST(DocPipelineUtilTest, PipelineToJson) {
    DocPipeline *pipeline = doc_pipeline_create(nullptr);
    doc_pipeline_add_match(pipeline, nullptr);
    doc_pipeline_add_limit(pipeline, 10);

    char *json = doc_pipeline_to_json(pipeline);

    ASSERT_NE(nullptr, json);
    EXPECT_TRUE(strstr(json, "$match") != nullptr);
    EXPECT_TRUE(strstr(json, "$limit") != nullptr);

    free(json);
    doc_pipeline_free(pipeline);
}

TEST(DocPipelineUtilTest, PipelineParse) {
    DocPipeline *pipeline = doc_pipeline_parse("[{\"$match\":{}},{\"$limit\":10}]");

    ASSERT_NE(nullptr, pipeline);
    EXPECT_GE(pipeline->num_stages, 0);

    doc_pipeline_free(pipeline);
}
