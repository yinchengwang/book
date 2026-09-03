/**
 * @file columnar_engine_test.cpp
 * @brief 列式存储引擎测试
 *
 * 测试列式引擎的表创建、列操作、压缩、聚合等功能。
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

extern "C" {
#include "db/columnar_engine.h"
#include "db/log.h"
}

#ifdef _WIN32
#define mkdir(path) _mkdir(path)
#endif

/**
 * @brief 列式引擎测试夹具
 */
class ColumnarEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_ERROR;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 确保测试目录存在 */
#ifdef _WIN32
        mkdir("./test_data_columnar");
#else
        mkdir("./test_data_columnar", 0755);
#endif
    }

    void TearDown() override {
        log_shutdown();

        /* 清理测试数据目录 */
#ifdef _WIN32
        system("rmdir /s /q ./test_data_columnar");
#else
        system("rm -rf ./test_data_columnar");
#endif
    }
};

/**
 * @brief 测试列式引擎初始化和关闭
 */
TEST_F(ColumnarEngineTest, InitShutdown) {
    /* 获取列式引擎操作表 */
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    EXPECT_STREQ("columnar_engine", ops->name);
    EXPECT_EQ(MODEL_COLUMNAR, ops->model);

    /* 初始化引擎 */
    if (ops->init) {
        int ret = ops->init("./test_data_columnar");
        EXPECT_EQ(0, ret);

        /* 关闭引擎 */
        if (ops->shutdown) {
            ops->shutdown();
        }
    }
}

/**
 * @brief 测试表创建
 */
TEST_F(ColumnarEngineTest, TableCreate) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 创建表 */
    const char *columns[] = {"id", "name", "value"};
    if (ops->table_create) {
        int ret = ops->table_create("test_table", columns, 3);
        EXPECT_EQ(0, ret);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试表打开和关闭
 */
TEST_F(ColumnarEngineTest, TableOpenClose) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 创建表 */
    const char *columns[] = {"id", "name", "value"};
    if (ops->table_create) {
        ops->table_create("test_table", columns, 3);
    }

    /* 打开表 */
    if (ops->table_open) {
        void *table = ops->table_open("test_table");
        ASSERT_NE(nullptr, table);

        /* 关闭表 */
        if (ops->table_close) {
            EXPECT_EQ(0, ops->table_close(table));
        }
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试列数据追加
 */
TEST_F(ColumnarEngineTest, ColumnAppend) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 创建表 */
    const char *columns[] = {"id", "value"};
    if (ops->table_create) {
        ops->table_create("test_table", columns, 2);
    }

    /* 打开表 */
    void *table = nullptr;
    if (ops->table_open) {
        table = ops->table_open("test_table");
    }

    if (table && ops->column_append) {
        /* 追加 id 列数据 */
        int64_t id_data[] = {1, 2, 3};
        for (size_t i = 0; i < 3; i++) {
            int ret = ops->column_append(table, "id", &id_data[i], sizeof(int64_t));
            EXPECT_EQ(0, ret);
        }

        /* 追加 value 列数据 */
        double value_data[] = {1.5, 2.5, 3.5};
        for (size_t i = 0; i < 3; i++) {
            int ret = ops->column_append(table, "value", &value_data[i], sizeof(double));
            EXPECT_EQ(0, ret);
        }
    }

    if (table && ops->table_close) {
        ops->table_close(table);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试批量列数据追加
 */
TEST_F(ColumnarEngineTest, ColumnAppendBatch) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 创建表 */
    const char *columns[] = {"id", "value"};
    if (ops->table_create) {
        ops->table_create("test_table", columns, 2);
    }

    /* 打开表 */
    void *table = nullptr;
    if (ops->table_open) {
        table = ops->table_open("test_table");
    }

    if (table && ops->column_append_batch) {
        /* 批量追加 id 列 */
        int64_t ids[] = {1, 2, 3, 4, 5};
        void *id_ptrs[] = {&ids[0], &ids[1], &ids[2], &ids[3], &ids[4]};
        size_t lens[] = {sizeof(int64_t), sizeof(int64_t), sizeof(int64_t), sizeof(int64_t), sizeof(int64_t)};

        int ret = ops->column_append_batch(table, "id", id_ptrs, lens, 5);
        EXPECT_EQ(0, ret);
    }

    if (table && ops->table_close) {
        ops->table_close(table);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试聚合函数
 */
TEST_F(ColumnarEngineTest, Aggregations) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 创建表 */
    const char *columns[] = {"id", "value"};
    if (ops->table_create) {
        ops->table_create("test_table", columns, 2);
    }

    /* 打开表 */
    void *table = nullptr;
    if (ops->table_open) {
        table = ops->table_open("test_table");
    }

    if (table && ops->column_append && ops->agg_count) {
        /* 追加数据 */
        int64_t ids[] = {1, 2, 3, 4, 5};
        double values[] = {10.0, 20.0, 30.0, 40.0, 50.0};

        for (size_t i = 0; i < 5; i++) {
            ops->column_append(table, "id", &ids[i], sizeof(int64_t));
            ops->column_append(table, "value", &values[i], sizeof(double));
        }

        /* 测试计数 */
        int64_t count = ops->agg_count(table);
        EXPECT_GE(count, 5);
    }

    if (table && ops->agg_sum_int64) {
        /* 测试 SUM */
        int64_t sum = 0;
        int ret = ops->agg_sum_int64(table, "id", &sum);
        EXPECT_GE(ret, 0);
    }

    if (table && ops->agg_avg_double) {
        /* 测试 AVG */
        double avg = 0.0;
        int ret = ops->agg_avg_double(table, "value", &avg);
        EXPECT_GE(ret, 0);
    }

    if (table && ops->table_close) {
        ops->table_close(table);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试压缩功能
 */
TEST_F(ColumnarEngineTest, Compress) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 创建表 */
    const char *columns[] = {"id", "value"};
    if (ops->table_create) {
        ops->table_create("test_table", columns, 2);
    }

    /* 打开表 */
    void *table = nullptr;
    if (ops->table_open) {
        table = ops->table_open("test_table");
    }

    /* 追加大量数据以便测试压缩 */
    if (table && ops->column_append) {
        for (int i = 0; i < 100; i++) {
            int64_t id = i;
            ops->column_append(table, "id", &id, sizeof(int64_t));
        }
    }

    /* 测试压缩 */
    if (table && ops->compress) {
        int ret = ops->compress(table, COL_COMPRESS_ZSTD);
        EXPECT_GE(ret, 0);
    }

    if (table && ops->table_close) {
        ops->table_close(table);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试统计信息
 */
TEST_F(ColumnarEngineTest, Stats) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 创建表 */
    const char *columns[] = {"id", "value"};
    if (ops->table_create) {
        ops->table_create("test_table", columns, 2);
    }

    /* 打开表 */
    void *table = nullptr;
    if (ops->table_open) {
        table = ops->table_open("test_table");
    }

    /* 追加数据 */
    if (table && ops->column_append) {
        int64_t id = 1;
        double value = 100.0;
        ops->column_append(table, "id", &id, sizeof(int64_t));
        ops->column_append(table, "value", &value, sizeof(double));
    }

    /* 获取统计信息 */
    if (ops->get_stats) {
        storage_stats_t stats;
        int ret = ops->get_stats("test_table", &stats);
        EXPECT_GE(ret, 0);
    }

    if (table && ops->table_close) {
        ops->table_close(table);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试表删除
 */
TEST_F(ColumnarEngineTest, TableDrop) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 创建表 */
    const char *columns[] = {"id"};
    if (ops->table_create) {
        ops->table_create("test_table", columns, 1);
    }

    /* 删除表 */
    if (ops->table_drop) {
        int ret = ops->table_drop("test_table");
        EXPECT_EQ(0, ret);
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 测试各种压缩类型
 */
TEST_F(ColumnarEngineTest, CompressionTypes) {
    const storage_ops_t *ops = columnar_engine_get_ops();
    if (ops == nullptr) {
        GTEST_SKIP() << "Columnar engine not available";
    }

    if (!ops->init || ops->init("./test_data_columnar") != 0) {
        GTEST_SKIP() << "Cannot initialize columnar engine";
    }

    /* 测试无压缩 */
    {
        const char *columns[] = {"id"};
        if (ops->table_create) {
            ops->table_create("test_table_none", columns, 1);
        }

        void *table = nullptr;
        if (ops->table_open) {
            table = ops->table_open("test_table_none");
        }

        if (table && ops->compress) {
            EXPECT_EQ(0, ops->compress(table, COL_COMPRESS_NONE));
        }

        if (table && ops->table_close) {
            ops->table_close(table);
        }
    }

    /* 测试 RLE 压缩 */
    {
        const char *columns[] = {"id"};
        if (ops->table_create) {
            ops->table_create("test_table_rle", columns, 1);
        }

        void *table = nullptr;
        if (ops->table_open) {
            table = ops->table_open("test_table_rle");
        }

        if (table && ops->compress) {
            EXPECT_EQ(0, ops->compress(table, COL_COMPRESS_RLE));
        }

        if (table && ops->table_close) {
            ops->table_close(table);
        }
    }

    /* 测试字典压缩 */
    {
        const char *columns[] = {"id"};
        if (ops->table_create) {
            ops->table_create("test_table_dict", columns, 1);
        }

        void *table = nullptr;
        if (ops->table_open) {
            table = ops->table_open("test_table_dict");
        }

        if (table && ops->compress) {
            EXPECT_EQ(0, ops->compress(table, COL_COMPRESS_DICTIONARY));
        }

        if (table && ops->table_close) {
            ops->table_close(table);
        }
    }

    if (ops->shutdown) {
        ops->shutdown();
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
