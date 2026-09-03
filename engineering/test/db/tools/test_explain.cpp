/**
 * @file test_explain.cpp
 * @brief EXPLAIN 执行计划分析工具测试
 */

#include <gtest/gtest.h>
#include <db/tools/explain.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

/* ─────────────────────────────────────────────────────────────────
 * 默认选项测试
 * ───────────────────────────────────────────────────────────────── */

class ExplainTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ExplainTest, DefaultOptions)
{
    ExplainOptions opts = explain_default_options();
    EXPECT_EQ(EXPLAIN_FORMAT_TEXT, opts.format);
    EXPECT_FALSE(opts.analyze);
    EXPECT_TRUE(opts.costs);
    EXPECT_FALSE(opts.buffers);
    EXPECT_FALSE(opts.timing);
    EXPECT_FALSE(opts.verbose);
}

/* ─────────────────────────────────────────────────────────────────
 * 上下文创建/销毁测试
 * ───────────────────────────────────────────────────────────────── */

TEST_F(ExplainTest, CreateDestroy)
{
    ExplainOptions opts = explain_default_options();
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);
    explain_destroy(ctx);
}

TEST_F(ExplainTest, CreateWithNullOptions)
{
    ExplainContext *ctx = explain_create(NULL);
    ASSERT_NE(nullptr, ctx);
    explain_destroy(ctx);
}

/* ─────────────────────────────────────────────────────────────────
 * 节点创建/销毁测试
 * ───────────────────────────────────────────────────────────────── */

TEST_F(ExplainTest, CreateNode)
{
    ExplainPlanNode *node = explain_create_node(PLAN_NODE_SEQ_SCAN);
    ASSERT_NE(nullptr, node);
    EXPECT_EQ(PLAN_NODE_SEQ_SCAN, node->type);
    EXPECT_EQ(0, node->n_children);
    EXPECT_EQ(nullptr, node->children);
    explain_free_plan(node);
}

TEST_F(ExplainTest, CreateNodeAllTypes)
{
    PlanNodeType types[] = {
        PLAN_NODE_SEQ_SCAN,
        PLAN_NODE_INDEX_SCAN,
        PLAN_NODE_BITMAP_SCAN,
        PLAN_NODE_NESTED_LOOP,
        PLAN_NODE_HASH_JOIN,
        PLAN_NODE_MERGE_JOIN,
        PLAN_NODE_AGGREGATE,
        PLAN_NODE_SORT,
        PLAN_NODE_LIMIT,
        PLAN_NODE_GATHER
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        ExplainPlanNode *node = explain_create_node(types[i]);
        ASSERT_NE(nullptr, node);
        EXPECT_EQ(types[i], node->type);
        explain_free_plan(node);
    }
}

TEST_F(ExplainTest, FreePlanNull)
{
    /* 不应崩溃 */
    explain_free_plan(NULL);
}

/* ─────────────────────────────────────────────────────────────────
 * 子节点管理测试
 * ───────────────────────────────────────────────────────────────── */

TEST_F(ExplainTest, AddChild)
{
    ExplainPlanNode *parent = explain_create_node(PLAN_NODE_NESTED_LOOP);
    ExplainPlanNode *child = explain_create_node(PLAN_NODE_SEQ_SCAN);

    ASSERT_NE(nullptr, parent);
    ASSERT_NE(nullptr, child);

    int ret = explain_add_child(parent, child);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, parent->n_children);
    EXPECT_EQ(child, parent->children[0]);

    explain_free_plan(parent);  /* 会递归释放 child */
}

TEST_F(ExplainTest, AddMultipleChildren)
{
    ExplainPlanNode *parent = explain_create_node(PLAN_NODE_HASH_JOIN);
    ExplainPlanNode *child1 = explain_create_node(PLAN_NODE_SEQ_SCAN);
    ExplainPlanNode *child2 = explain_create_node(PLAN_NODE_INDEX_SCAN);

    ASSERT_NE(nullptr, parent);

    EXPECT_EQ(0, explain_add_child(parent, child1));
    EXPECT_EQ(0, explain_add_child(parent, child2));
    EXPECT_EQ(2, parent->n_children);
    EXPECT_EQ(child1, parent->children[0]);
    EXPECT_EQ(child2, parent->children[1]);

    explain_free_plan(parent);
}

TEST_F(ExplainTest, AddChildNullParent)
{
    ExplainPlanNode *child = explain_create_node(PLAN_NODE_SEQ_SCAN);
    int ret = explain_add_child(NULL, child);
    EXPECT_NE(0, ret);
    explain_free_plan(child);
}

TEST_F(ExplainTest, AddChildNullChild)
{
    ExplainPlanNode *parent = explain_create_node(PLAN_NODE_NESTED_LOOP);
    int ret = explain_add_child(parent, NULL);
    EXPECT_NE(0, ret);
    explain_free_plan(parent);
}

/* ─────────────────────────────────────────────────────────────────
 * TEXT 格式输出测试
 * ───────────────────────────────────────────────────────────────── */

TEST_F(ExplainTest, OutputTextSimple)
{
    ExplainOptions opts = explain_default_options();
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    scan->relation_name = "users";
    scan->startup_cost = 0.0;
    scan->total_cost = 100.0;
    scan->plan_rows = 1000;
    scan->plan_width = 64;

    /* 使用临时文件进行测试 */
    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, scan, fp);
    EXPECT_EQ(0, ret);

    /* 回读验证 */
    fflush(fp);
    rewind(fp);
    char buf[2048] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "Seq Scan") != nullptr);
    EXPECT_TRUE(strstr(buf, "users") != nullptr);
    EXPECT_TRUE(strstr(buf, "cost=") != nullptr);
    EXPECT_TRUE(strstr(buf, "rows=1000") != nullptr);
    EXPECT_TRUE(strstr(buf, "width=64") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(scan);
    explain_destroy(ctx);
}

TEST_F(ExplainTest, OutputTextWithFilter)
{
    ExplainOptions opts = explain_default_options();
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    scan->relation_name = "users";
    scan->filter = "(age > 18)";
    scan->startup_cost = 0.0;
    scan->total_cost = 100.0;
    scan->plan_rows = 500;
    scan->plan_width = 64;

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, scan, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[2048] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "Filter:") != nullptr);
    EXPECT_TRUE(strstr(buf, "age > 18") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(scan);
    explain_destroy(ctx);
}

TEST_F(ExplainTest, OutputTextWithChildren)
{
    ExplainOptions opts = explain_default_options();
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *nl = explain_create_node(PLAN_NODE_NESTED_LOOP);
    nl->startup_cost = 0.0;
    nl->total_cost = 200.0;
    nl->plan_rows = 100;
    nl->plan_width = 128;

    ExplainPlanNode *seq = explain_create_node(PLAN_NODE_SEQ_SCAN);
    seq->relation_name = "users";
    seq->startup_cost = 0.0;
    seq->total_cost = 50.0;
    seq->plan_rows = 1000;
    seq->plan_width = 64;

    ExplainPlanNode *idx = explain_create_node(PLAN_NODE_INDEX_SCAN);
    idx->relation_name = "orders";
    idx->index_name = "idx_orders_id";
    idx->startup_cost = 0.0;
    idx->total_cost = 30.0;
    idx->plan_rows = 10;
    idx->plan_width = 48;

    explain_add_child(nl, seq);
    explain_add_child(nl, idx);

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, nl, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[4096] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "Nested Loop") != nullptr);
    EXPECT_TRUE(strstr(buf, "Seq Scan") != nullptr);
    EXPECT_TRUE(strstr(buf, "Index Scan") != nullptr);
    EXPECT_TRUE(strstr(buf, "users") != nullptr);
    EXPECT_TRUE(strstr(buf, "orders") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(nl);
    explain_destroy(ctx);
}

TEST_F(ExplainTest, OutputTextNullArgs)
{
    ExplainOptions opts = explain_default_options();
    ExplainContext *ctx = explain_create(&opts);
    ExplainPlanNode *node = explain_create_node(PLAN_NODE_SEQ_SCAN);

    EXPECT_NE(0, explain_output(NULL, node, stdout));
    EXPECT_NE(0, explain_output(ctx, NULL, stdout));
    EXPECT_NE(0, explain_output(ctx, node, NULL));

    explain_free_plan(node);
    explain_destroy(ctx);
}

/* ─────────────────────────────────────────────────────────────────
 * JSON 格式输出测试
 * ───────────────────────────────────────────────────────────────── */

TEST_F(ExplainTest, OutputJsonSimple)
{
    ExplainOptions opts = explain_default_options();
    opts.format = EXPLAIN_FORMAT_JSON;
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    scan->relation_name = "users";
    scan->startup_cost = 0.0;
    scan->total_cost = 100.0;
    scan->plan_rows = 1000;
    scan->plan_width = 64;
    scan->filter = "(age > 18)";

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, scan, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[4096] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "\"Node Type\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Seq Scan\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Relation Name\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"users\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Filter\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Startup Cost\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Total Cost\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Plan Rows\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Plan Width\"") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(scan);
    explain_destroy(ctx);
}

TEST_F(ExplainTest, OutputJsonWithChildren)
{
    ExplainOptions opts = explain_default_options();
    opts.format = EXPLAIN_FORMAT_JSON;
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *hj = explain_create_node(PLAN_NODE_HASH_JOIN);
    hj->startup_cost = 10.0;
    hj->total_cost = 500.0;
    hj->plan_rows = 200;
    hj->plan_width = 128;

    ExplainPlanNode *seq1 = explain_create_node(PLAN_NODE_SEQ_SCAN);
    seq1->relation_name = "users";
    seq1->startup_cost = 0.0;
    seq1->total_cost = 100.0;
    seq1->plan_rows = 1000;
    seq1->plan_width = 64;

    ExplainPlanNode *seq2 = explain_create_node(PLAN_NODE_SEQ_SCAN);
    seq2->relation_name = "orders";
    seq2->startup_cost = 0.0;
    seq2->total_cost = 200.0;
    seq2->plan_rows = 5000;
    seq2->plan_width = 48;

    explain_add_child(hj, seq1);
    explain_add_child(hj, seq2);

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, hj, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[8192] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "\"Hash Join\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Plans\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"users\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"orders\"") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(hj);
    explain_destroy(ctx);
}

TEST_F(ExplainTest, OutputJsonWithIndex)
{
    ExplainOptions opts = explain_default_options();
    opts.format = EXPLAIN_FORMAT_JSON;
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *idx = explain_create_node(PLAN_NODE_INDEX_SCAN);
    idx->relation_name = "orders";
    idx->index_name = "idx_orders_id";
    idx->startup_cost = 0.0;
    idx->total_cost = 8.5;
    idx->plan_rows = 1;
    idx->plan_width = 32;

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, idx, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[2048] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "\"Index Scan\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Index Name\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "idx_orders_id") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Index Cond\"") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(idx);
    explain_destroy(ctx);
}

/* ─────────────────────────────────────────────────────────────────
 * ANALYZE 模式测试
 * ───────────────────────────────────────────────────────────────── */

TEST_F(ExplainTest, OutputTextWithAnalyze)
{
    ExplainOptions opts = explain_default_options();
    opts.analyze = true;
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    scan->relation_name = "users";
    scan->startup_cost = 0.0;
    scan->total_cost = 100.0;
    scan->plan_rows = 1000;
    scan->plan_width = 64;
    scan->actual_time = 3.456;
    scan->actual_rows = 998;
    scan->actual_loops = 1;

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, scan, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[2048] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "actual time=") != nullptr);
    EXPECT_TRUE(strstr(buf, "rows=998") != nullptr);
    EXPECT_TRUE(strstr(buf, "loops=1") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(scan);
    explain_destroy(ctx);
}

TEST_F(ExplainTest, OutputJsonWithAnalyze)
{
    ExplainOptions opts = explain_default_options();
    opts.format = EXPLAIN_FORMAT_JSON;
    opts.analyze = true;
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    scan->relation_name = "users";
    scan->startup_cost = 0.0;
    scan->total_cost = 100.0;
    scan->plan_rows = 1000;
    scan->plan_width = 64;
    scan->actual_time = 2.345;
    scan->actual_rows = 998;
    scan->actual_loops = 1;

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, scan, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[4096] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "\"Actual Total Time\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Actual Rows\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Actual Loops\"") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(scan);
    explain_destroy(ctx);
}

/* ─────────────────────────────────────────────────────────────────
 * BUFFERS 模式测试
 * ───────────────────────────────────────────────────────────────── */

TEST_F(ExplainTest, OutputTextWithBuffers)
{
    ExplainOptions opts = explain_default_options();
    opts.buffers = true;
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    scan->relation_name = "users";
    scan->startup_cost = 0.0;
    scan->total_cost = 100.0;
    scan->plan_rows = 1000;
    scan->plan_width = 64;
    scan->shared_hit = 42;
    scan->shared_read = 8;
    scan->shared_dirtied = 2;
    scan->shared_written = 1;

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, scan, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[2048] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "Buffers:") != nullptr);
    EXPECT_TRUE(strstr(buf, "shared hit=42") != nullptr);
    EXPECT_TRUE(strstr(buf, "read=8") != nullptr);
    EXPECT_TRUE(strstr(buf, "dirtied=2") != nullptr);
    EXPECT_TRUE(strstr(buf, "written=1") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(scan);
    explain_destroy(ctx);
}

TEST_F(ExplainTest, OutputJsonWithBuffers)
{
    ExplainOptions opts = explain_default_options();
    opts.format = EXPLAIN_FORMAT_JSON;
    opts.buffers = true;
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    scan->relation_name = "users";
    scan->startup_cost = 0.0;
    scan->total_cost = 100.0;
    scan->plan_rows = 1000;
    scan->plan_width = 64;
    scan->shared_hit = 42;
    scan->shared_read = 8;

    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, scan, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[4096] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "\"Shared Hit Blocks\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Shared Read Blocks\"") != nullptr);

    fclose(fp);
    remove(tmpfile);
    explain_free_plan(scan);
    explain_destroy(ctx);
}

/* ─────────────────────────────────────────────────────────────────
 * 深层嵌套计划测试
 * ───────────────────────────────────────────────────────────────── */

TEST_F(ExplainTest, DeepNestedPlan)
{
    ExplainOptions opts = explain_default_options();
    ExplainContext *ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    /* Sort -> Limit -> Nested Loop -> (Seq Scan, Index Scan) */
    ExplainPlanNode *sort = explain_create_node(PLAN_NODE_SORT);
    sort->startup_cost = 100.0;
    sort->total_cost = 110.0;
    sort->plan_rows = 100;
    sort->plan_width = 64;

    ExplainPlanNode *limit = explain_create_node(PLAN_NODE_LIMIT);
    limit->startup_cost = 0.0;
    limit->total_cost = 10.0;
    limit->plan_rows = 10;
    limit->plan_width = 64;

    ExplainPlanNode *nl = explain_create_node(PLAN_NODE_NESTED_LOOP);
    nl->startup_cost = 0.0;
    nl->total_cost = 50.0;
    nl->plan_rows = 100;
    nl->plan_width = 64;

    ExplainPlanNode *seq = explain_create_node(PLAN_NODE_SEQ_SCAN);
    seq->relation_name = "users";
    seq->startup_cost = 0.0;
    seq->total_cost = 20.0;
    seq->plan_rows = 1000;
    seq->plan_width = 32;

    ExplainPlanNode *idx = explain_create_node(PLAN_NODE_INDEX_SCAN);
    idx->relation_name = "orders";
    idx->startup_cost = 0.0;
    idx->total_cost = 5.0;
    idx->plan_rows = 10;
    idx->plan_width = 32;

    explain_add_child(nl, seq);
    explain_add_child(nl, idx);
    explain_add_child(limit, nl);
    explain_add_child(sort, limit);

    /* TEXT 格式测试 */
    char tmpfile[] = "test_explain_XXXXXX";
    int fd = mkstemp(tmpfile);
    ASSERT_NE(-1, fd);
    FILE *fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    int ret = explain_output(ctx, sort, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    char buf[8192] = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "Sort") != nullptr);
    EXPECT_TRUE(strstr(buf, "Limit") != nullptr);
    EXPECT_TRUE(strstr(buf, "Nested Loop") != nullptr);
    EXPECT_TRUE(strstr(buf, "Seq Scan") != nullptr);
    EXPECT_TRUE(strstr(buf, "Index Scan") != nullptr);

    fclose(fp);
    remove(tmpfile);

    /* JSON 格式测试 */
    opts.format = EXPLAIN_FORMAT_JSON;
    explain_destroy(ctx);
    ctx = explain_create(&opts);
    ASSERT_NE(nullptr, ctx);

    char tmpfile2[] = "test_explain_XXXXXX";
    fd = mkstemp(tmpfile2);
    ASSERT_NE(-1, fd);
    fp = fdopen(fd, "w+");
    ASSERT_NE(nullptr, fp);

    ret = explain_output(ctx, sort, fp);
    EXPECT_EQ(0, ret);

    fflush(fp);
    rewind(fp);
    fread(buf, 1, sizeof(buf) - 1, fp);
    EXPECT_TRUE(strstr(buf, "\"Sort\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Limit\"") != nullptr);
    EXPECT_TRUE(strstr(buf, "\"Nested Loop\"") != nullptr);

    fclose(fp);
    remove(tmpfile2);
    explain_free_plan(sort);
    explain_destroy(ctx);
}