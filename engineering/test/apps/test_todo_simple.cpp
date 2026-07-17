#include <gtest/gtest.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include "todo_model.h"
}

/* 简化测试 - 每个测试使用独立内存数据库 */
TEST(TodoSimpleTest, BasicCreate) {
    todo_db_reset();

    todo_t t = {0};
    strcpy(t.title, "Test");
    strcpy(t.status, "open");

    int64_t id = 0;
    int r = todo_create(&t, &id);
    EXPECT_EQ(0, r);
    EXPECT_EQ(1, id);
    EXPECT_EQ(0, todo_get_by_id(id, &t));

    todo_db_shutdown();
}

TEST(TodoSimpleTest, Update) {
    todo_db_reset();

    todo_t t = {0};
    strcpy(t.title, "Original");
    strcpy(t.status, "open");
    int64_t id;
    todo_create(&t, &id);

    t.id = id;
    strcpy(t.title, "Updated");
    EXPECT_EQ(0, todo_update(&t));

    todo_t result;
    todo_get_by_id(id, &result);
    EXPECT_STREQ("Updated", result.title);

    todo_db_shutdown();
}

TEST(TodoSimpleTest, Delete) {
    todo_db_reset();

    todo_t t = {0};
    strcpy(t.title, "ToDelete");
    strcpy(t.status, "open");
    int64_t id;
    todo_create(&t, &id);

    EXPECT_EQ(0, todo_delete(id));
    EXPECT_NE(0, todo_get_by_id(id, &t));

    todo_db_shutdown();
}

TEST(TodoSimpleTest, Stats) {
    todo_db_reset();

    todo_t t = {0};
    strcpy(t.status, "open");
    todo_create(&t, NULL);

    strcpy(t.status, "closed");
    todo_create(&t, NULL);

    todo_stats_t stats;
    EXPECT_EQ(0, todo_get_stats(&stats));
    EXPECT_EQ(2, stats.total);
    EXPECT_EQ(1, stats.open);
    EXPECT_EQ(1, stats.closed);

    todo_db_shutdown();
}

TEST(TodoSimpleTest, GroupCRUD) {
    todo_db_reset();

    group_t g = {0};
    strcpy(g.name, "Work");
    strcpy(g.color, "#ff0000");

    int64_t gid = 0;
    EXPECT_EQ(0, group_create(&g, &gid));
    EXPECT_EQ(1, gid);

    group_t result;
    EXPECT_EQ(0, group_get(gid, &result));
    EXPECT_STREQ("Work", result.name);

    group_t *groups = NULL;
    int count = 0;
    EXPECT_EQ(0, group_list(&groups, &count));
    EXPECT_EQ(1, count);
    group_list_free(groups, count);

    EXPECT_EQ(0, group_delete(gid));
    EXPECT_NE(0, group_get(gid, &result));

    todo_db_shutdown();
}

TEST(TodoSimpleTest, CommentCRUD) {
    todo_db_reset();

    todo_t t = {0};
    strcpy(t.title, "For Comment");
    strcpy(t.status, "open");
    int64_t tid;
    todo_create(&t, &tid);

    comment_t c = {0};
    EXPECT_EQ(0, comment_add(tid, "Test comment", &c));
    EXPECT_EQ(1, c.id);

    comment_t *comments = NULL;
    int count = 0;
    EXPECT_EQ(0, comment_list(tid, &comments, &count));
    EXPECT_EQ(1, count);
    comment_list_free(comments, count);

    EXPECT_EQ(0, comment_delete(c.id));

    todo_db_shutdown();
}
