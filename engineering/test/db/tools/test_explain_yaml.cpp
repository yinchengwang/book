/**
 * @file test_explain_yaml.cpp
 * @brief YAML 格式 EXPLAIN 测试
 */

#include <gtest/gtest.h>
#include <db/tools/explain.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

/* 辅助函数：创建临时文件 */
static FILE *create_temp_file(char *path_buf, size_t buf_size)
{
    snprintf(path_buf, buf_size, "/tmp/test_explain_yaml_%d.tmp", getpid());
    FILE *fp = fopen(path_buf, "w+");
    return fp;
}

/* 辅助函数：关闭并删除临时文件 */
static void cleanup_temp_file(FILE *fp, const char *path)
{
    if (fp) {
        fclose(fp);
    }
    if (path) {
        unlink(path);
    }
}

/**
 * @brief YAML 格式 EXPLAIN 测试类
 */
class ExplainYamlTest : public ::testing::Test {
protected:
    char temp_path[256];
    FILE *temp_fp;

    void SetUp() override
    {
        temp_fp = create_temp_file(temp_path, sizeof(temp_path));
    }

    void TearDown() override
    {
        cleanup_temp_file(temp_fp, temp_path);
        temp_fp = nullptr;
    }
};

TEST_F(ExplainYamlTest, OutputYamlSeqScan)
{
    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    ASSERT_NE(scan, nullptr);
    scan->relation_name = "users";
    scan->startup_cost = 0.0;
    scan->total_cost = 8.0;
    scan->plan_rows = 100;
    scan->plan_width = 64;

    ASSERT_NE(temp_fp, nullptr);

    int ret = explain_output_yaml(scan, temp_fp, 0);
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[512];
    fgets(buf, sizeof(buf), temp_fp);
    EXPECT_TRUE(strstr(buf, "Node Type: Seq Scan") != nullptr);

    explain_free_plan(scan);
}

TEST_F(ExplainYamlTest, OutputYamlIndexScan)
{
    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_INDEX_SCAN);
    ASSERT_NE(scan, nullptr);
    scan->relation_name = "orders";
    scan->index_name = "idx_order_id";
    scan->startup_cost = 0.5;
    scan->total_cost = 4.2;
    scan->plan_rows = 10;
    scan->plan_width = 32;

    ASSERT_NE(temp_fp, nullptr);

    int ret = explain_output_yaml(scan, temp_fp, 0);
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[1024];
    size_t total = 0;
    while (fgets(buf + total, sizeof(buf) - total, temp_fp)) {
        total += strlen(buf + total);
    }

    EXPECT_TRUE(strstr(buf, "Node Type: Index Scan") != nullptr);
    EXPECT_TRUE(strstr(buf, "Index Name: idx_order_id") != nullptr);
    EXPECT_TRUE(strstr(buf, "Relation Name: orders") != nullptr);

    explain_free_plan(scan);
}

TEST_F(ExplainYamlTest, OutputYamlNestedPlan)
{
    ExplainPlanNode *join = explain_create_node(PLAN_NODE_HASH_JOIN);
    ExplainPlanNode *scan1 = explain_create_node(PLAN_NODE_SEQ_SCAN);
    ExplainPlanNode *scan2 = explain_create_node(PLAN_NODE_SEQ_SCAN);

    ASSERT_NE(join, nullptr);
    ASSERT_NE(scan1, nullptr);
    ASSERT_NE(scan2, nullptr);

    join->relation_name = "users";
    join->startup_cost = 10.0;
    join->total_cost = 100.0;
    join->plan_rows = 1000;
    join->plan_width = 128;

    scan1->relation_name = "orders";
    scan1->startup_cost = 0.0;
    scan1->total_cost = 20.0;
    scan1->plan_rows = 200;
    scan1->plan_width = 64;

    scan2->relation_name = "products";
    scan2->startup_cost = 0.0;
    scan2->total_cost = 15.0;
    scan2->plan_rows = 150;
    scan2->plan_width = 48;

    explain_add_child(join, scan1);
    explain_add_child(join, scan2);

    ASSERT_NE(temp_fp, nullptr);

    int ret = explain_output_yaml(join, temp_fp, 0);
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[2048];
    size_t total = 0;
    while (fgets(buf + total, sizeof(buf) - total, temp_fp)) {
        total += strlen(buf + total);
    }

    EXPECT_TRUE(strstr(buf, "Node Type: Hash Join") != nullptr);
    EXPECT_TRUE(strstr(buf, "Plans:") != nullptr);
    EXPECT_TRUE(strstr(buf, "Relation Name: orders") != nullptr);
    EXPECT_TRUE(strstr(buf, "Relation Name: products") != nullptr);

    explain_free_plan(join);
}

TEST_F(ExplainYamlTest, OutputYamlWithFilter)
{
    ExplainPlanNode *scan = explain_create_node(PLAN_NODE_SEQ_SCAN);
    ASSERT_NE(scan, nullptr);
    scan->relation_name = "items";
    scan->filter = "(price > 100)";
    scan->startup_cost = 0.0;
    scan->total_cost = 50.0;
    scan->plan_rows = 500;
    scan->plan_width = 32;

    ASSERT_NE(temp_fp, nullptr);

    int ret = explain_output_yaml(scan, temp_fp, 0);
    EXPECT_EQ(0, ret);

    rewind(temp_fp);
    char buf[512];
    size_t total = 0;
    while (fgets(buf + total, sizeof(buf) - total, temp_fp)) {
        total += strlen(buf + total);
    }

    EXPECT_TRUE(strstr(buf, "Filter: '(price > 100)'") != nullptr);

    explain_free_plan(scan);
}

TEST_F(ExplainYamlTest, OutputYamlNullNode)
{
    ASSERT_NE(temp_fp, nullptr);

    int ret = explain_output_yaml(NULL, temp_fp, 0);
    EXPECT_EQ(-1, ret);
}
