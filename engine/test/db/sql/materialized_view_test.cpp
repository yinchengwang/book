/**
 * @file materialized_view_test.cpp
 * @brief 物化视图单元测试
 *
 * 测试内容：
 * 1. 物化视图管理器创建与销毁
 * 2. 物化视图创建、删除、查询
 * 3. 完整刷新与增量刷新
 * 4. CONCURRENTLY 刷新
 * 5. 查询重写
 * 6. 时序物化视图
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/materialized_view.h"
}

namespace {

/* ========================================================================
 * 测试夹具
 * ======================================================================== */

class MvTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = mv_manager_create("/tmp/test_mv");
        ASSERT_NE(mgr, nullptr);
    }

    void TearDown() override {
        if (mgr) {
            mv_manager_destroy(mgr);
            mgr = nullptr;
        }
    }

    MvManager *mgr = nullptr;
};

/* ========================================================================
 * 物化视图管理器测试
 * ======================================================================== */

TEST_F(MvTest, CreateAndDestroy) {
    /* 已在 SetUp/TearDown 中测试 */
    EXPECT_TRUE(mgr != nullptr);
}

TEST_F(MvTest, ListEmpty) {
    int count = 0;
    char **names = mv_list(mgr, &count);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(names, nullptr);
    mv_free_names(names, count);
}

/* ========================================================================
 * 物化视图生命周期测试
 * ======================================================================== */

TEST_F(MvTest, CreateBasic) {
    MvDef *def = mv_def_create("mv_test", "SELECT id, name FROM users");
    ASSERT_NE(def, nullptr);

    mv_def_add_column(def, "id", "INTEGER");
    mv_def_add_column(def, "name", "VARCHAR(100)");
    mv_def_add_source_table(def, "users");

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    EXPECT_STREQ(mv_get_name(view), "mv_test");
    EXPECT_EQ(mv_get_state(view), MV_STATE_VALID);  /* with_data=true 会自动刷新 */
    EXPECT_GT(mv_row_count(view), 0);

    mv_def_destroy(def);
}

TEST_F(MvTest, CreateWithoutData) {
    MvDef *def = mv_def_create("mv_empty", "SELECT id FROM orders");
    ASSERT_NE(def, nullptr);

    def->with_data = false;
    mv_def_add_source_table(def, "orders");

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    EXPECT_EQ(mv_get_state(view), MV_STATE_STALE);  /* without_data 初始为过期 */
    EXPECT_EQ(mv_row_count(view), 0);

    mv_def_destroy(def);
}

TEST_F(MvTest, CreateDuplicate) {
    MvDef *def = mv_def_create("mv_dup", "SELECT * FROM t");
    ASSERT_NE(def, nullptr);

    MaterializedView *v1 = mv_create(mgr, def);
    ASSERT_NE(v1, nullptr);

    /* 尝试创建同名视图应该失败 */
    MaterializedView *v2 = mv_create(mgr, def);
    EXPECT_EQ(v2, nullptr);

    mv_def_destroy(def);
}

TEST_F(MvTest, Drop) {
    MvDef *def = mv_def_create("mv_drop", "SELECT 1");
    ASSERT_NE(def, nullptr);

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    EXPECT_TRUE(mv_exists(mgr, "mv_drop"));
    EXPECT_EQ(mv_drop(mgr, "mv_drop"), 0);
    EXPECT_FALSE(mv_exists(mgr, "mv_drop"));

    mv_def_destroy(def);
}

TEST_F(MvTest, DropNotExists) {
    EXPECT_EQ(mv_drop(mgr, "not_exists"), -1);
}

TEST_F(MvTest, Get) {
    MvDef *def = mv_def_create("mv_get", "SELECT 1");
    ASSERT_NE(def, nullptr);

    MaterializedView *original = mv_create(mgr, def);
    ASSERT_NE(original, nullptr);

    MaterializedView *found = mv_get(mgr, "mv_get");
    EXPECT_EQ(found, original);

    mv_def_destroy(def);
}

TEST_F(MvTest, GetNotExists) {
    EXPECT_EQ(mv_get(mgr, "not_exists"), nullptr);
}

TEST_F(MvTest, List) {
    /* 创建多个视图 */
    MvDef *def1 = mv_def_create("mv_list_1", "SELECT 1");
    MvDef *def2 = mv_def_create("mv_list_2", "SELECT 2");
    MvDef *def3 = mv_def_create("mv_list_3", "SELECT 3");

    ASSERT_NE(def1, nullptr);
    ASSERT_NE(def2, nullptr);
    ASSERT_NE(def3, nullptr);

    ASSERT_NE(mv_create(mgr, def1), nullptr);
    ASSERT_NE(mv_create(mgr, def2), nullptr);
    ASSERT_NE(mv_create(mgr, def3), nullptr);

    int count = 0;
    char **names = mv_list(mgr, &count);
    EXPECT_EQ(count, 3);

    /* 验证名称存在 */
    bool found_1 = false, found_2 = false, found_3 = false;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], "mv_list_1") == 0) found_1 = true;
        if (strcmp(names[i], "mv_list_2") == 0) found_2 = true;
        if (strcmp(names[i], "mv_list_3") == 0) found_3 = true;
    }
    EXPECT_TRUE(found_1 && found_2 && found_3);

    mv_free_names(names, count);
    mv_def_destroy(def1);
    mv_def_destroy(def2);
    mv_def_destroy(def3);
}

/* ========================================================================
 * 刷新测试
 * ======================================================================== */

TEST_F(MvTest, RefreshFull) {
    MvDef *def = mv_def_create("mv_refresh", "SELECT id, COUNT(*) FROM orders GROUP BY id");
    ASSERT_NE(def, nullptr);
    def->with_data = false;

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    EXPECT_EQ(mv_get_state(view), MV_STATE_STALE);

    /* 执行完整刷新 */
    EXPECT_EQ(mv_refresh(view, REFRESH_FULL), 0);
    EXPECT_EQ(mv_get_state(view), MV_STATE_VALID);
    EXPECT_GT(mv_last_refresh_time(view), 0);

    mv_def_destroy(def);
}

TEST_F(MvTest, RefreshIncremental) {
    MvDef *def = mv_def_create("mv_incr", "SELECT 1");
    ASSERT_NE(def, nullptr);
    def->with_data = false;

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    /* 先完整刷新 */
    EXPECT_EQ(mv_refresh(view, REFRESH_FULL), 0);
    EXPECT_EQ(mv_get_state(view), MV_STATE_VALID);

    /* 记录变更 */
    EXPECT_EQ(mv_record_change(view, "t", 100, 'I', NULL, "row1"), 0);
    EXPECT_EQ(mv_get_state(view), MV_STATE_STALE);

    /* 增量刷新 */
    EXPECT_EQ(mv_refresh(view, REFRESH_INCREMENTAL), 0);
    EXPECT_EQ(mv_get_state(view), MV_STATE_VALID);

    mv_def_destroy(def);
}

TEST_F(MvTest, RefreshConcurrently) {
    MvDef *def = mv_def_create("mv_concurrent", "SELECT 1");
    ASSERT_NE(def, nullptr);
    def->with_data = false;

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    EXPECT_EQ(mv_refresh(view, REFRESH_CONCURRENTLY), 0);
    EXPECT_EQ(mv_get_state(view), MV_STATE_VALID);

    /* 再次刷新应该成功（CONCURRENTLY允许重复刷新） */
    EXPECT_EQ(mv_refresh(view, REFRESH_CONCURRENTLY), 0);

    mv_def_destroy(def);
}

TEST_F(MvTest, RefreshAll) {
    MvDef *def1 = mv_def_create("mv_all_1", "SELECT 1");
    MvDef *def2 = mv_def_create("mv_all_2", "SELECT 2");
    MvDef *def3 = mv_def_create("mv_all_3", "SELECT 3");

    ASSERT_NE(def1, nullptr);
    ASSERT_NE(def2, nullptr);
    ASSERT_NE(def3, nullptr);

    def1->with_data = false;
    def2->with_data = false;
    def3->with_data = false;

    ASSERT_NE(mv_create(mgr, def1), nullptr);
    ASSERT_NE(mv_create(mgr, def2), nullptr);
    ASSERT_NE(mv_create(mgr, def3), nullptr);

    int count = mv_refresh_all(mgr, REFRESH_FULL);
    EXPECT_EQ(count, 3);

    mv_def_destroy(def1);
    mv_def_destroy(def2);
    mv_def_destroy(def3);
}

/* ========================================================================
 * 查询重写测试
 * ======================================================================== */

TEST_F(MvTest, RewriteCanRewrite) {
    /* 创建物化视图 */
    MvDef *def = mv_def_create("mv_rewrite", "SELECT id, name FROM users WHERE status = 1");
    ASSERT_NE(def, nullptr);
    mv_def_add_source_table(def, "users");

    ASSERT_NE(mv_create(mgr, def), nullptr);

    /* 分析查询 */
    const char *query = "SELECT id, name FROM users";
    MvRewriteResult *result = mv_analyze_rewrite(mgr, query);
    ASSERT_NE(result, nullptr);

    EXPECT_TRUE(result->can_rewrite);
    EXPECT_STREQ(result->matched_mv_name, "mv_rewrite");

    mv_rewrite_result_free(result);
    mv_def_destroy(def);
}

TEST_F(MvTest, RewriteCannotRewrite) {
    /* 无物化视图时无法重写 */
    const char *query = "SELECT * FROM unknown";
    MvRewriteResult *result = mv_analyze_rewrite(mgr, query);
    ASSERT_NE(result, nullptr);

    EXPECT_FALSE(result->can_rewrite);

    mv_rewrite_result_free(result);
}

TEST_F(MvTest, RewriteQuery) {
    MvDef *def = mv_def_create("mv_rq", "SELECT * FROM orders");
    ASSERT_NE(def, nullptr);
    mv_def_add_source_table(def, "orders");

    ASSERT_NE(mv_create(mgr, def), nullptr);

    char buffer[256];
    int ret = mv_rewrite_query(mgr, "SELECT * FROM orders", buffer, sizeof(buffer));
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(strstr(buffer, "mv_rq") != nullptr);

    mv_def_destroy(def);
}

TEST_F(MvTest, RewriteQueryNoMatch) {
    char buffer[256];
    int ret = mv_rewrite_query(mgr, "SELECT * FROM nonexistent", buffer, sizeof(buffer));
    EXPECT_EQ(ret, 0);
    /* 无法重写时返回原查询 */
    EXPECT_TRUE(strstr(buffer, "nonexistent") != nullptr);
}

/* ========================================================================
 * 时序物化视图测试
 * ======================================================================== */

TEST_F(MvTest, SlidingWindow) {
    MvDef *def = mv_def_create("mv_window", "SELECT 1");
    ASSERT_NE(def, nullptr);

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    /* 设置滑动窗口 */
    MvSlidingWindow config;
    config.window_size = 60;
    config.window_unit = 60;  /* 60秒=1分钟 */
    config.slide_interval = 10;
    config.slide_unit = 60;   /* 每次滑动10分钟 */
    config.keep_history = false;

    EXPECT_EQ(mv_set_sliding_window(view, &config), 0);

    const MvSlidingWindow *sw = mv_get_sliding_window(view);
    ASSERT_NE(sw, nullptr);
    EXPECT_EQ(sw->window_size, 60);
    EXPECT_EQ(sw->window_unit, 60);

    /* 推进滑动窗口 */
    EXPECT_EQ(mv_advance_sliding_window(view), 0);

    mv_def_destroy(def);
}

/* ========================================================================
 * 变更追踪测试
 * ======================================================================== */

TEST_F(MvTest, RecordChange) {
    MvDef *def = mv_def_create("mv_change", "SELECT 1");
    ASSERT_NE(def, nullptr);

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    /* 记录 INSERT */
    EXPECT_EQ(mv_record_change(view, "orders", 100, 'I', NULL, "new_row"), 0);

    /* 记录 UPDATE */
    EXPECT_EQ(mv_record_change(view, "orders", 101, 'U', "old_row", "new_row"), 0);

    /* 记录 DELETE */
    EXPECT_EQ(mv_record_change(view, "orders", 102, 'D', "old_row", NULL), 0);

    /* 视图应该变为过期状态 */
    EXPECT_EQ(mv_get_state(view), MV_STATE_STALE);

    mv_def_destroy(def);
}

TEST_F(MvTest, GetChanges) {
    MvDef *def = mv_def_create("mv_chg", "SELECT 1");
    ASSERT_NE(def, nullptr);

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    /* 记录多个变更 */
    mv_record_change(view, "t", 100, 'I', NULL, "row1");
    mv_record_change(view, "t", 101, 'I', NULL, "row2");
    mv_record_change(view, "t", 102, 'I', NULL, "row3");

    /* 获取从 LSN=101 开始的变更 */
    int count = 0;
    MvChangeLog *changes = mv_get_changes(view, 101, &count);
    ASSERT_NE(changes, nullptr);
    EXPECT_EQ(count, 2);  /* 101 和 102 */

    mv_free_change_log(changes);
    mv_def_destroy(def);
}

TEST_F(MvTest, PurgeChanges) {
    MvDef *def = mv_def_create("mv_purge", "SELECT 1");
    ASSERT_NE(def, nullptr);

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    /* 记录变更 */
    mv_record_change(view, "t", 100, 'I', NULL, "row1");
    mv_record_change(view, "t", 101, 'I', NULL, "row2");
    mv_record_change(view, "t", 102, 'I', NULL, "row3");

    /* 清理到 LSN=101 */
    mv_purge_changes(view, 101);

    int count = 0;
    MvChangeLog *changes = mv_get_changes(view, 0, &count);
    ASSERT_NE(changes, nullptr);
    EXPECT_EQ(count, 1);  /* 只剩 102 */

    mv_free_change_log(changes);
    mv_def_destroy(def);
}

/* ========================================================================
 * 辅助函数测试
 * ======================================================================== */

TEST_F(MvTest, StateName) {
    EXPECT_STREQ(mv_state_name(MV_STATE_VALID), "valid");
    EXPECT_STREQ(mv_state_name(MV_STATE_STALE), "stale");
    EXPECT_STREQ(mv_state_name(MV_STATE_BUILDING), "building");
    EXPECT_STREQ(mv_state_name(MV_STATE_ERROR), "error");
    EXPECT_STREQ(mv_state_name(MV_STATE_UNUSABLE), "unusable");
}

TEST_F(MvTest, RefreshTypeName) {
    EXPECT_STREQ(mv_refresh_type_name(REFRESH_FULL), "full");
    EXPECT_STREQ(mv_refresh_type_name(REFRESH_INCREMENTAL), "incremental");
    EXPECT_STREQ(mv_refresh_type_name(REFRESH_CONCURRENTLY), "concurrently");
}

TEST_F(MvTest, MvDefCreate) {
    MvDef *def = mv_def_create("test_mv", "SELECT * FROM t");
    ASSERT_NE(def, nullptr);

    EXPECT_STREQ(def->name, "test_mv");
    EXPECT_STREQ(def->select_sql, "SELECT * FROM t");
    EXPECT_TRUE(def->with_data);
    EXPECT_EQ(def->column_count, 0);
    EXPECT_EQ(def->source_table_count, 0);

    mv_def_destroy(def);
}

TEST_F(MvTest, MvDefAddColumn) {
    MvDef *def = mv_def_create("test", "SELECT 1");
    ASSERT_NE(def, nullptr);

    mv_def_add_column(def, "id", "INTEGER");
    mv_def_add_column(def, "name", "VARCHAR(100)");

    EXPECT_EQ(def->column_count, 2);
    EXPECT_STREQ(def->columns[0].name, "id");
    EXPECT_STREQ(def->columns[1].name, "name");

    mv_def_destroy(def);
}

TEST_F(MvTest, MvDefAddSourceTable) {
    MvDef *def = mv_def_create("test", "SELECT 1");
    ASSERT_NE(def, nullptr);

    mv_def_add_source_table(def, "users");
    mv_def_add_source_table(def, "orders");

    EXPECT_EQ(def->source_table_count, 2);
    EXPECT_STREQ(def->source_tables[0], "users");
    EXPECT_STREQ(def->source_tables[1], "orders");

    mv_def_destroy(def);
}

/* ========================================================================
 * 属性测试
 * ======================================================================== */

TEST_F(MvTest, ViewProperties) {
    MvDef *def = mv_def_create("mv_props", "SELECT id, name FROM users");
    ASSERT_NE(def, nullptr);
    mv_def_add_source_table(def, "users");

    MaterializedView *view = mv_create(mgr, def);
    ASSERT_NE(view, nullptr);

    /* 初始状态 */
    EXPECT_STREQ(mv_get_name(view), "mv_props");
    EXPECT_EQ(mv_get_state(view), MV_STATE_VALID);
    EXPECT_GT(mv_last_refresh_time(view), 0);
    EXPECT_GT(mv_row_count(view), 0);
    EXPECT_GT(mv_data_size(view), 0);

    mv_def_destroy(def);
}

/* ========================================================================
 * 边界条件测试
 * ======================================================================== */

TEST_F(MvTest, NullParameters) {
    /* 空名称 */
    MvDef *def = mv_def_create(NULL, "SELECT 1");
    EXPECT_EQ(def, nullptr);

    /* 空管理器 */
    EXPECT_EQ(mv_get(NULL, "test"), nullptr);
    EXPECT_FALSE(mv_exists(NULL, "test"));
    EXPECT_FALSE(mv_exists(mgr, NULL));

    /* 空视图 */
    EXPECT_EQ(mv_refresh(NULL, REFRESH_FULL), -1);
    EXPECT_EQ(mv_get_state(NULL), MV_STATE_ERROR);
    EXPECT_EQ(mv_get_name(NULL), nullptr);
}

TEST(MvManagerStandalone, CreateNullDataDir) {
    /* 数据目录可以为 NULL */
    MvManager *mgr = mv_manager_create(NULL);
    EXPECT_NE(mgr, nullptr);
    mv_manager_destroy(mgr);
}

}  /* namespace */
