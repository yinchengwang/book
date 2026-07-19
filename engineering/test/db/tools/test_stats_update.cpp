#include <gtest/gtest.h>
extern "C" {
#include "db/tools/stats.h"
}
#include <string.h>

TEST(StatsUpdateTest, GlobalCollector) {
    StatsCollector *sc = stats_init();
    ASSERT_NE(sc, nullptr);
    
    /* 初始时全局收集器为 NULL */
    EXPECT_EQ(stats_get_collector(), nullptr);
    
    /* 设置全局收集器 */
    stats_set_collector(sc);
    EXPECT_EQ(stats_get_collector(), sc);
    
    stats_set_collector(NULL);
    EXPECT_EQ(stats_get_collector(), nullptr);
    
    stats_shutdown(sc);
}

TEST(StatsUpdateTest, UpdateInsert) {
    StatsCollector *sc = stats_init();
    ASSERT_NE(sc, nullptr);
    
    /* 插入计数 */
    EXPECT_EQ(stats_update_table_insert(sc, "users", 5), 0);
    EXPECT_EQ(stats_update_table_insert(sc, "users", 3), 0);
    
    StatTable stat;
    int count = 0;
    EXPECT_EQ(stats_get_tables(sc, "users", &stat, 1, &count), 0);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(stat.n_tup_ins, 8);
    
    stats_shutdown(sc);
}

TEST(StatsUpdateTest, UpdateUpdate) {
    StatsCollector *sc = stats_init();
    ASSERT_NE(sc, nullptr);
    
    EXPECT_EQ(stats_update_table_update(sc, "items", 2), 0);
    EXPECT_EQ(stats_update_table_update(sc, "items", 1), 0);
    
    StatTable stat;
    int count = 0;
    EXPECT_EQ(stats_get_tables(sc, "items", &stat, 1, &count), 0);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(stat.n_tup_upd, 3);
    
    stats_shutdown(sc);
}

TEST(StatsUpdateTest, UpdateDelete) {
    StatsCollector *sc = stats_init();
    ASSERT_NE(sc, nullptr);
    
    EXPECT_EQ(stats_update_table_delete(sc, "orders", 10), 0);
    
    StatTable stat;
    int count = 0;
    EXPECT_EQ(stats_get_tables(sc, "orders", &stat, 1, &count), 0);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(stat.n_tup_del, 10);
    
    stats_shutdown(sc);
}

TEST(StatsUpdateTest, UpdateTuples) {
    StatsCollector *sc = stats_init();
    ASSERT_NE(sc, nullptr);
    
    EXPECT_EQ(stats_update_table_tuples(sc, "users", 100, 5), 0);
    
    StatTable stat;
    int count = 0;
    EXPECT_EQ(stats_get_tables(sc, "users", &stat, 1, &count), 0);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(stat.n_live_tup, 100);
    EXPECT_EQ(stat.n_dead_tup, 5);
    
    stats_shutdown(sc);
}

TEST(StatsUpdateTest, MultipleTables) {
    StatsCollector *sc = stats_init();
    ASSERT_NE(sc, nullptr);
    
    EXPECT_EQ(stats_update_table_insert(sc, "a", 1), 0);
    EXPECT_EQ(stats_update_table_insert(sc, "b", 2), 0);
    EXPECT_EQ(stats_update_table_insert(sc, "c", 3), 0);
    
    StatTable stats[3];
    int count = 0;
    EXPECT_EQ(stats_get_tables(sc, NULL, stats, 3, &count), 0);
    EXPECT_EQ(count, 3);
    
    stats_shutdown(sc);
}

TEST(StatsUpdateTest, DatabaseLevelStats) {
    StatsCollector *sc = stats_init();
    ASSERT_NE(sc, nullptr);
    
    stats_update_table_insert(sc, "t1", 5);
    stats_update_table_update(sc, "t2", 3);
    stats_update_table_delete(sc, "t3", 2);
    
    StatDatabase db_stat;
    EXPECT_EQ(stats_get_database(sc, &db_stat), 0);
    EXPECT_EQ(db_stat.tup_inserted, 5);
    EXPECT_EQ(db_stat.tup_updated, 3);
    EXPECT_EQ(db_stat.tup_deleted, 2);
    
    stats_shutdown(sc);
}

TEST(StatsUpdateTest, NullCollector) {
    EXPECT_EQ(stats_update_table_insert(NULL, "t", 1), -1);
    EXPECT_EQ(stats_update_table_update(NULL, "t", 1), -1);
    EXPECT_EQ(stats_update_table_delete(NULL, "t", 1), -1);
    EXPECT_EQ(stats_update_table_tuples(NULL, "t", 1, 0), -1);
}
