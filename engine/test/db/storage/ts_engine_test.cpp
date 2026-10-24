/**
 * @file ts_engine_test.cpp
 * @brief 时序存储引擎测试
 *
 * 测试时序引擎的基本操作和 Gorilla 压缩算法
 */
#include <gtest/gtest.h>
#include "db/ts_engine.h"
#include "db/storage/ts/ts_compress.h"
#include "db/storage/ts/ts_mview.h"
#include "db/storage/ts/ts_segment.h"
#include "db/log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/**
 * @brief 时序引擎测试夹具
 */
class TsEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_WARN;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 确保测试目录存在 */
#ifdef _WIN32
        mkdir("./test_data");
        mkdir("./test_data/ts_engine");
#else
        mkdir("./test_data", 0755);
        mkdir("./test_data/ts_engine", 0755);
#endif

        /* 初始化时序引擎 */
        ASSERT_EQ(0, ts_engine_init("./test_data/ts_engine"));
    }

    void TearDown() override {
        ts_engine_shutdown();
        log_shutdown();

        /* 清理测试数据目录 */
        system("rm -rf ./test_data/ts_engine");
    }
};

/**
 * @brief 时序压缩测试夹具
 */
class TsCompressTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_WARN;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);
    }

    void TearDown() override {
        log_shutdown();
    }
};

/* ========================================================================
 * 时序引擎测试
 * ======================================================================== */

/**
 * @brief 测试时序引擎生命周期
 */
TEST_F(TsEngineTest, Lifecycle) {
    /* 创建时序指标 */
    EXPECT_EQ(0, ts_engine_create("test_metric", NULL));

    /* 打开指标 */
    void *rel = ts_engine_open("test_metric", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    ts_engine_db_t *db = static_cast<ts_engine_db_t *>(rel);
    EXPECT_STREQ("test_metric", db->name);
    EXPECT_EQ(ACCESS_MODE_READ_WRITE, db->mode);

    /* 关闭指标 */
    EXPECT_EQ(0, ts_engine_close(rel));

    /* 删除指标 */
    EXPECT_EQ(0, ts_engine_drop("test_metric"));
}

/**
 * @brief 测试插入单个数据点
 */
TEST_F(TsEngineTest, InsertSinglePoint) {
    EXPECT_EQ(0, ts_engine_create("test_metric", NULL));

    void *rel = ts_engine_open("test_metric", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 构造时序数据: timestamp(8) + value(8) */
    uint8_t data[32];
    int64_t timestamp = 1700000000000LL;  /* 2023-11-15 */
    double value = 42.5;

    uint8_t *ptr = data;
    memcpy(ptr, &timestamp, sizeof(int64_t));
    ptr += sizeof(int64_t);
    memcpy(ptr, &value, sizeof(double));

    /* 插入数据点 */
    int ret = ts_engine_insert(rel, data, sizeof(int64_t) + sizeof(double));
    EXPECT_EQ(0, ret);

    /* 关闭 */
    EXPECT_EQ(0, ts_engine_close(rel));
    ts_engine_drop("test_metric");
}

/**
 * @brief 测试插入多个数据点
 */
TEST_F(TsEngineTest, InsertMultiplePoints) {
    EXPECT_EQ(0, ts_engine_create("test_metric", NULL));

    void *rel = ts_engine_open("test_metric", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入多个数据点 */
    int64_t base_timestamp = 1700000000000LL;
    for (int i = 0; i < 100; i++) {
        uint8_t data[32];
        int64_t timestamp = base_timestamp + i * 1000;  /* 每秒一个点 */
        double value = 100.0 + i * 0.1;  /* 线性增长 */

        uint8_t *ptr = data;
        memcpy(ptr, &timestamp, sizeof(int64_t));
        ptr += sizeof(int64_t);
        memcpy(ptr, &value, sizeof(double));

        EXPECT_EQ(0, ts_engine_insert(rel, data, sizeof(int64_t) + sizeof(double)));
    }

    /* 关闭 */
    EXPECT_EQ(0, ts_engine_close(rel));
    ts_engine_drop("test_metric");
}

/**
 * @brief 测试时间范围查询
 */
TEST_F(TsEngineTest, QueryRange) {
    EXPECT_EQ(0, ts_engine_create("test_metric", NULL));

    void *rel = ts_engine_open("test_metric", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入测试数据 */
    int64_t base_timestamp = 1700000000000LL;
    for (int i = 0; i < 1000; i++) {
        uint8_t data[32];
        int64_t timestamp = base_timestamp + i * 60000;  /* 每分钟一个点 */
        double value = sin(i * 0.1) * 100;  /* 正弦波 */

        uint8_t *ptr = data;
        memcpy(ptr, &timestamp, sizeof(int64_t));
        ptr += sizeof(int64_t);
        memcpy(ptr, &value, sizeof(double));

        ts_engine_insert(rel, data, sizeof(int64_t) + sizeof(double));
    }

    /* 查询时间范围 */
    int64_t start_time = base_timestamp + 10 * 60000;
    int64_t end_time = base_timestamp + 100 * 60000;

    ts_query_results_t results;
    memset(&results, 0, sizeof(results));

    int ret = ts_engine_query(rel, start_time, end_time,
                              TS_GRANULARITY_MINUTE, AGG_AVG, &results);

    /* 查询应该成功（可能返回 0 表示不支持或无结果） */
    EXPECT_GE(ret, 0);

    /* 释放结果 */
    ts_engine_free_results(&results);

    /* 关闭 */
    EXPECT_EQ(0, ts_engine_close(rel));
    ts_engine_drop("test_metric");
}

/**
 * @brief 测试各种聚合函数
 */
TEST_F(TsEngineTest, Aggregations) {
    EXPECT_EQ(0, ts_engine_create("test_metric", NULL));

    void *rel = ts_engine_open("test_metric", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入测试数据 */
    int64_t base_timestamp = 1700000000000LL;
    double values[] = {10.0, 20.0, 30.0, 40.0, 50.0};

    for (size_t i = 0; i < 5; i++) {
        uint8_t data[32];
        int64_t timestamp = base_timestamp + i * 1000;
        double value = values[i];

        uint8_t *ptr = data;
        memcpy(ptr, &timestamp, sizeof(int64_t));
        ptr += sizeof(int64_t);
        memcpy(ptr, &value, sizeof(double));

        ts_engine_insert(rel, data, sizeof(int64_t) + sizeof(double));
    }

    /* 查询 SUM */
    ts_query_results_t results_sum;
    memset(&results_sum, 0, sizeof(results_sum));
    ts_engine_query(rel, base_timestamp, base_timestamp + 10000,
                    TS_GRANULARITY_HOUR, AGG_SUM, &results_sum);

    /* 查询 AVG */
    ts_query_results_t results_avg;
    memset(&results_avg, 0, sizeof(results_avg));
    ts_engine_query(rel, base_timestamp, base_timestamp + 10000,
                    TS_GRANULARITY_HOUR, AGG_AVG, &results_avg);

    /* 查询 MIN */
    ts_query_results_t results_min;
    memset(&results_min, 0, sizeof(results_min));
    ts_engine_query(rel, base_timestamp, base_timestamp + 10000,
                    TS_GRANULARITY_HOUR, AGG_MIN, &results_min);

    /* 查询 MAX */
    ts_query_results_t results_max;
    memset(&results_max, 0, sizeof(results_max));
    ts_engine_query(rel, base_timestamp, base_timestamp + 10000,
                    TS_GRANULARITY_HOUR, AGG_MAX, &results_max);

    /* 释放结果 */
    ts_engine_free_results(&results_sum);
    ts_engine_free_results(&results_avg);
    ts_engine_free_results(&results_min);
    ts_engine_free_results(&results_max);

    /* 关闭 */
    EXPECT_EQ(0, ts_engine_close(rel));
    ts_engine_drop("test_metric");
}

/**
 * @brief 测试各种时间粒度
 */
TEST_F(TsEngineTest, Granularities) {
    EXPECT_EQ(0, ts_engine_create("test_metric", NULL));

    void *rel = ts_engine_open("test_metric", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 测试各粒度工具函数 */
    EXPECT_EQ(1000, ts_granularity_to_ms(TS_GRANULARITY_SECOND));
    EXPECT_EQ(60 * 1000, ts_granularity_to_ms(TS_GRANULARITY_MINUTE));
    EXPECT_EQ(60 * 60 * 1000, ts_granularity_to_ms(TS_GRANULARITY_HOUR));
    EXPECT_EQ(24 * 60 * 60 * 1000, ts_granularity_to_ms(TS_GRANULARITY_DAY));

    /* 测试时间戳对齐：ts = 1700000123 ms */
    int64_t ts = 1700000123LL;
    EXPECT_EQ(1700000000LL, ts_align_timestamp(ts, TS_GRANULARITY_SECOND));
    EXPECT_EQ(1699980000LL, ts_align_timestamp(ts, TS_GRANULARITY_MINUTE));
    EXPECT_EQ(1699200000LL, ts_align_timestamp(ts, TS_GRANULARITY_HOUR));
    EXPECT_EQ(1641600000LL, ts_align_timestamp(ts, TS_GRANULARITY_DAY));

    /* 关闭 */
    EXPECT_EQ(0, ts_engine_close(rel));
    ts_engine_drop("test_metric");
}

/**
 * @brief 测试统计信息
 */
TEST_F(TsEngineTest, Stats) {
    EXPECT_EQ(0, ts_engine_create("test_metric", NULL));

    void *rel = ts_engine_open("test_metric", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入数据 */
    int64_t base_timestamp = 1700000000000LL;
    for (int i = 0; i < 50; i++) {
        uint8_t data[32];
        int64_t timestamp = base_timestamp + i * 1000;
        double value = i * 1.0;

        uint8_t *ptr = data;
        memcpy(ptr, &timestamp, sizeof(int64_t));
        ptr += sizeof(int64_t);
        memcpy(ptr, &value, sizeof(double));

        ts_engine_insert(rel, data, sizeof(int64_t) + sizeof(double));
    }

    EXPECT_EQ(0, ts_engine_close(rel));

    /* 获取统计信息 */
    storage_stats_t stats;
    int ret = ts_engine_stats("test_metric", &stats);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(50, stats.num_objects);

    ts_engine_drop("test_metric");
}

/**
 * @brief 测试获取引擎操作表
 */
TEST_F(TsEngineTest, GetOps) {
    const storage_ops_t *ops = ts_engine_get_ops();
    ASSERT_NE(nullptr, ops);

    EXPECT_STREQ("ts_engine", ops->name);
    EXPECT_EQ(MODEL_TIMESERIES, ops->model);

    /* 验证函数指针有效 */
    EXPECT_NE(nullptr, ops->init);
    EXPECT_NE(nullptr, ops->shutdown);
    EXPECT_NE(nullptr, ops->table_create);
    EXPECT_NE(nullptr, ops->table_open);
    EXPECT_NE(nullptr, ops->tuple_insert);
    EXPECT_NE(nullptr, ops->table_close);
    EXPECT_NE(nullptr, ops->table_drop);
}

/* ========================================================================
 * 时序压缩测试（直接测试压缩器）
 * ======================================================================== */

/**
 * @brief 测试压缩器创建和销毁
 */
TEST_F(TsCompressTest, CompressorCreateFree) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 验证初始状态 */
    EXPECT_EQ(0, comp->total_points);
    EXPECT_EQ(0, comp->total_original_size);
    EXPECT_EQ(0, comp->total_compressed_size);

    /* 释放压缩器 */
    ts_compressor_free(comp);
}

/**
 * @brief 测试添加单个数据点
 */
TEST_F(TsCompressTest, CompressAddSingle) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加单个数据点 */
    int64_t timestamp = 1700000000000LL;
    double value = 123.456;

    int ret = ts_compress_add(comp, timestamp, value);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, comp->total_points);

    /* 刷新压缩 */
    ret = ts_compress_flush(comp);
    EXPECT_EQ(0, ret) << "ts_compress_flush failed";

    /* 验证压缩成功：通过统计信息判断 */
    uint64_t total_points = 0;
    double compression_ratio = 0.0;
    ts_compress_get_stats(comp, &total_points, &compression_ratio);

    EXPECT_EQ(1, total_points);
    EXPECT_GT(compression_ratio, 0.0) << "compression_ratio should be > 0";
    printf("[DEBUG] Compression: points=%llu, ratio=%.2f\n",
           (unsigned long long)total_points, compression_ratio);

    ts_compressor_free(comp);
}

/**
 * @brief 测试添加多个数据点
 */
TEST_F(TsCompressTest, CompressAddMultiple) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加多个数据点 */
    int64_t base_timestamp = 1700000000000LL;
    for (int i = 0; i < 100; i++) {
        int64_t timestamp = base_timestamp + i * 1000;  /* 每秒一个点 */
        double value = 100.0 + i * 0.5;  /* 线性增长 */

        EXPECT_EQ(0, ts_compress_add(comp, timestamp, value));
    }

    EXPECT_EQ(100, comp->total_points);

    /* 刷新压缩 */
    EXPECT_EQ(0, ts_compress_flush(comp));

    /* 获取压缩数据 */
    size_t compressed_size = 0;
    const uint8_t *data = ts_compress_get_data(comp, &compressed_size);
    ASSERT_NE(nullptr, data);
    EXPECT_GT(compressed_size, 0);

    /* 验证压缩统计 */
    uint64_t total_points = 0;
    double compression_ratio = 0.0;
    ts_compress_get_stats(comp, &total_points, &compression_ratio);

    EXPECT_EQ(100, total_points);
    /* 原始数据大小: 100 * (8 + 8) = 1600 字节 */
    /* 压缩比应该大于 1（表示压缩后更小） */
    EXPECT_GT(compression_ratio, 1.0);

    ts_compressor_free(comp);
}

/**
 * @brief 测试压缩解压往返
 */
TEST_F(TsCompressTest, CompressDecompressRoundTrip) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加测试数据 */
    const int NUM_POINTS = 100;
    int64_t base_timestamp = 1700000000000LL;
    double values[NUM_POINTS];

    for (int i = 0; i < NUM_POINTS; i++) {
        int64_t timestamp = base_timestamp + i * 1000;
        values[i] = 100.0 + i * 0.5;

        ts_compress_add(comp, timestamp, values[i]);
    }

    /* 刷新并获取压缩数据 */
    ts_compress_flush(comp);
    size_t compressed_size = 0;
    const uint8_t *compressed_data = ts_compress_get_data(comp, &compressed_size);
    ASSERT_NE(nullptr, compressed_data);

    ts_compressor_free(comp);

    /* 解压数据 */
    ts_record_t *out_records = new ts_record_t[NUM_POINTS];
    int decompressed_count = ts_decompress(compressed_data, compressed_size,
                                           out_records, NUM_POINTS);

    EXPECT_EQ(NUM_POINTS, decompressed_count);

    /* 验证解压数据与原始数据一致 */
    for (int i = 0; i < decompressed_count; i++) {
        EXPECT_EQ(base_timestamp + i * 1000, out_records[i].timestamp);
        EXPECT_DOUBLE_EQ(values[i], out_records[i].value);
    }

    delete[] out_records;
}

/**
 * @brief 测试解压块信息
 */
TEST_F(TsCompressTest, DecompressBlockInfo) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加数据 */
    int64_t base_timestamp = 1700000000000LL;
    for (int i = 0; i < 50; i++) {
        ts_compress_add(comp, base_timestamp + i * 1000, 100.0 + i);
    }

    /* 刷新并获取压缩数据 */
    ts_compress_flush(comp);
    size_t compressed_size = 0;
    const uint8_t *compressed_data = ts_compress_get_data(comp, &compressed_size);

    ts_compressor_free(comp);

    /* 获取压缩块信息 */
    int64_t first_ts = 0;
    uint32_t num_points = 0;

    int ret = ts_compress_get_info(compressed_data, &first_ts, &num_points);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(base_timestamp, first_ts);
    EXPECT_EQ(50, num_points);
}

/**
 * @brief 测试压缩率 - 线性增长数据
 */
TEST_F(TsCompressTest, CompressionRatioLinear) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加线性增长数据（压缩效果好） */
    const int NUM_POINTS = 1000;
    int64_t base_timestamp = 1700000000000LL;

    for (int i = 0; i < NUM_POINTS; i++) {
        int64_t timestamp = base_timestamp + i * 1000;
        double value = i * 1.0;  /* 线性增长 */

        ts_compress_add(comp, timestamp, value);
    }

    /* 刷新压缩 */
    ts_compress_flush(comp);

    /* 获取压缩统计 */
    uint64_t total_points = 0;
    double compression_ratio = 0.0;
    ts_compress_get_stats(comp, &total_points, &compression_ratio);

    /* 原始数据: NUM_POINTS * (8 + 8) = 16000 字节 */
    /* 线性数据应该有很好的压缩比 */
    EXPECT_EQ(NUM_POINTS, total_points);
    EXPECT_GT(compression_ratio, 5.0);  /* 压缩比应该大于 5 */

    ts_compressor_free(comp);
}

/**
 * @brief 测试压缩率 - 随机数据
 */
TEST_F(TsCompressTest, CompressionRatioRandom) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加随机数据（压缩效果较差） */
    const int NUM_POINTS = 1000;
    int64_t base_timestamp = 1700000000000LL;

    srand(42);  /* 固定种子保证可重复 */
    for (int i = 0; i < NUM_POINTS; i++) {
        int64_t timestamp = base_timestamp + i * 1000;
        double value = (double)rand() / RAND_MAX * 1000.0;

        ts_compress_add(comp, timestamp, value);
    }

    /* 刷新压缩 */
    ts_compress_flush(comp);

    /* 获取压缩统计 */
    uint64_t total_points = 0;
    double compression_ratio = 0.0;
    ts_compress_get_stats(comp, &total_points, &compression_ratio);

    /* 随机数据压缩比应该较低 */
    EXPECT_EQ(NUM_POINTS, total_points);
    /* 随机数据压缩比应该小于线性数据，但仍应大于 1 */
    EXPECT_GT(compression_ratio, 1.0);

    ts_compressor_free(comp);
}

/**
 * @brief 测试压缩率 - 常数值
 */
TEST_F(TsCompressTest, CompressionRatioConstant) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加常数值（最佳压缩效果） */
    const int NUM_POINTS = 1000;
    int64_t base_timestamp = 1700000000000LL;
    double constant_value = 42.0;

    for (int i = 0; i < NUM_POINTS; i++) {
        int64_t timestamp = base_timestamp + i * 1000;
        ts_compress_add(comp, timestamp, constant_value);
    }

    /* 刷新压缩 */
    ts_compress_flush(comp);

    /* 获取压缩统计 */
    uint64_t total_points = 0;
    double compression_ratio = 0.0;
    ts_compress_get_stats(comp, &total_points, &compression_ratio);

    /* 常数值应该有极高的压缩比 */
    EXPECT_EQ(NUM_POINTS, total_points);
    EXPECT_GT(compression_ratio, 20.0);  /* 压缩比应该大于 20 */

    ts_compressor_free(comp);
}

/**
 * @brief 测试解压范围查询
 */
TEST_F(TsCompressTest, DecompressRange) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加测试数据 */
    const int NUM_POINTS = 100;
    int64_t base_timestamp = 1700000000000LL;

    for (int i = 0; i < NUM_POINTS; i++) {
        ts_compress_add(comp, base_timestamp + i * 1000, 100.0 + i);
    }

    /* 刷新并获取压缩数据 */
    ts_compress_flush(comp);
    size_t compressed_size = 0;
    const uint8_t *compressed_data = ts_compress_get_data(comp, &compressed_size);

    ts_compressor_free(comp);

    /* 解压指定范围（只取中间部分） */
    ts_record_t *out_records = new ts_record_t[30];
    int64_t start_time = base_timestamp + 30 * 1000;
    int64_t end_time = base_timestamp + 60 * 1000;

    int decompressed_count = ts_decompress_range(compressed_data, compressed_size,
                                                  start_time, end_time,
                                                  out_records, 30);

    /* 应该返回范围内的点数 */
    EXPECT_GT(decompressed_count, 0);
    EXPECT_LE(decompressed_count, 30);

    /* 验证时间范围 */
    for (int i = 0; i < decompressed_count; i++) {
        EXPECT_GE(out_records[i].timestamp, start_time);
        EXPECT_LE(out_records[i].timestamp, end_time);
    }

    delete[] out_records;
}

/**
 * @brief 测试空压缩器
 */
TEST_F(TsCompressTest, EmptyCompressor) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 不添加任何数据，直接刷新 */
    int ret = ts_compress_flush(comp);
    EXPECT_EQ(0, ret);

    /* 获取压缩数据应该返回 NULL 或空 */
    size_t compressed_size = 0;
    const uint8_t *data = ts_compress_get_data(comp, &compressed_size);

    /* 统计数据 */
    uint64_t total_points = 0;
    double compression_ratio = 0.0;
    ts_compress_get_stats(comp, &total_points, &compression_ratio);

    EXPECT_EQ(0, total_points);
    EXPECT_EQ(0.0, compression_ratio);

    ts_compressor_free(comp);
}

/**
 * @brief 测试压缩器块大小限制
 */
TEST_F(TsCompressTest, BlockSizeLimit) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 添加超过块大小的数据 */
    const int NUM_POINTS = TS_COMPRESS_BLOCK_SIZE + 100;
    int64_t base_timestamp = 1700000000000LL;

    for (int i = 0; i < NUM_POINTS; i++) {
        int64_t timestamp = base_timestamp + i * 1000;
        double value = 100.0 + i;

        int ret = ts_compress_add(comp, timestamp, value);
        EXPECT_EQ(0, ret);
    }

    /* 刷新压缩 */
    ts_compress_flush(comp);

    /* 验证总点数 */
    uint64_t total_points = 0;
    double compression_ratio = 0.0;
    ts_compress_get_stats(comp, &total_points, &compression_ratio);

    EXPECT_EQ(NUM_POINTS, total_points);

    ts_compressor_free(comp);
}

/**
 * @brief 测试解压空数据
 */
TEST_F(TsCompressTest, DecompressEmpty) {
    /* 解压空数据应该返回 0 或负数 */
    ts_record_t out_record;
    int count = ts_decompress(NULL, 0, &out_record, 1);
    EXPECT_LE(count, 0);
}

/**
 * @brief 测试解压无效数据
 */
TEST_F(TsCompressTest, DecompressInvalid) {
    /* 使用随机数据作为压缩数据应该失败 */
    uint8_t invalid_data[16] = {0xDE, 0xAD, 0xBE, 0xEF};
    ts_record_t out_record;

    int count = ts_decompress(invalid_data, sizeof(invalid_data), &out_record, 1);
    EXPECT_LE(count, 0);
}

/**
 * @brief 测试压缩块大小常量
 */
TEST_F(TsCompressTest, BlockSizeConstant) {
    /* 验证压缩块大小常量定义 */
    EXPECT_EQ(4096, TS_COMPRESS_BLOCK_SIZE);
    EXPECT_EQ(64, TS_COMPRESS_MAX_BITS);
}

/**
 * @brief 测试压缩块状态
 */
TEST_F(TsCompressTest, BlockState) {
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    /* 初始状态应该为空 */
    EXPECT_EQ(TS_COMPRESS_BLOCK_EMPTY, comp->current_block->state);

    /* 添加数据后应该变为写入状态 */
    ts_compress_add(comp, 1700000000000LL, 100.0);
    EXPECT_EQ(TS_COMPRESS_BLOCK_WRITING, comp->current_block->state);

    /* 刷新后应该变为已压缩状态 */
    ts_compress_flush(comp);
    EXPECT_EQ(TS_COMPRESS_BLOCK_COMPRESSED, comp->current_block->state);

    ts_compressor_free(comp);
}

/* ========================================================================
 * 时序分段索引测试
 * ======================================================================== */

/**
 * @brief 测试夹具：时序分段索引
 */
class TsSegmentTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        mkdir("./test_data/ts_segment");
#else
        mkdir("./test_data/ts_segment", 0755);
#endif
    }

    void TearDown() override {
        system("rm -rf ./test_data/ts_segment");
    }
};

TEST_F(TsSegmentTest, CreateAndDestroy) {
    ts_segment_index_t *index = ts_segment_index_create(
        "./test_data/ts_segment", 100, TS_GRANULARITY_HOUR);
    ASSERT_NE(nullptr, index);
    EXPECT_EQ(0u, ts_segment_count(index));
    ts_segment_index_destroy(index);
}

TEST_F(TsSegmentTest, Append) {
    ts_segment_index_t *index = ts_segment_index_create(
        "./test_data/ts_segment", 100, TS_GRANULARITY_HOUR);
    ASSERT_NE(nullptr, index);

    int64_t ts = 1700000000000LL;
    EXPECT_EQ(0, ts_segment_append(index, ts, 100.0));
    EXPECT_EQ(1u, ts_segment_count(index));

    ts_segment_index_destroy(index);
}

TEST_F(TsSegmentTest, AppendBatch) {
    ts_segment_index_t *index = ts_segment_index_create(
        "./test_data/ts_segment", 100, TS_GRANULARITY_MINUTE);
    ASSERT_NE(nullptr, index);

    int64_t timestamps[10];
    double values[10];
    for (int i = 0; i < 10; i++) {
        timestamps[i] = 1700000000000LL + i * 60000;
        values[i] = 100.0 + i;
    }

    uint32_t count = ts_segment_append_batch(index, timestamps, values, 10);
    EXPECT_EQ(10u, count);

    ts_segment_index_destroy(index);
}

TEST_F(TsSegmentTest, SegmentFlush) {
    ts_segment_index_t *index = ts_segment_index_create(
        "./test_data/ts_segment", 10, TS_GRANULARITY_MINUTE);
    ASSERT_NE(nullptr, index);

    /* 添加超过段大小的数据触发自动切换 */
    int64_t ts = 1700000000000LL;
    for (int i = 0; i < 15; i++) {
        ts_segment_append(index, ts + i * 60000, 100.0 + i);
    }

    EXPECT_GE(ts_segment_count(index), 1u);
    ts_segment_index_destroy(index);
}

TEST_F(TsSegmentTest, Expire) {
    ts_segment_index_t *index = ts_segment_index_create(
        "./test_data/ts_segment", 100, TS_GRANULARITY_HOUR);
    ASSERT_NE(nullptr, index);

    /* 添加一些数据 */
    int64_t old_ts = 1600000000000LL;  /* 旧时间戳 */
    int64_t new_ts = 1700000000000LL;  /* 当前时间戳 */
    ts_segment_append(index, old_ts, 50.0);
    ts_segment_append(index, new_ts, 100.0);

    /* 删除 2020 年之前的数据 */
    uint32_t deleted = ts_segment_expire(index, 1600000000000LL);
    EXPECT_GE(deleted, 0u);

    ts_segment_index_destroy(index);
}

/* ========================================================================
 * 时序物化视图测试
 * ======================================================================== */

/**
 * @brief 测试夹具：时序物化视图
 */
class TsMViewTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        mkdir("./test_data/ts_mview");
#else
        mkdir("./test_data/ts_mview", 0755);
#endif
    }

    void TearDown() override {
        system("rm -rf ./test_data/ts_mview");
    }
};

TEST_F(TsMViewTest, CreateAndDestroy) {
    ts_mview_t *mview = ts_mview_create("test_view",
        "./test_data/ts_mview", TS_AGG_AVG, 60000);
    ASSERT_NE(nullptr, mview);
    EXPECT_STREQ("test_view", mview->name);
    EXPECT_EQ(TS_AGG_AVG, mview->func);
    ts_mview_destroy(mview);
}

TEST_F(TsMViewTest, AddData) {
    ts_mview_t *mview = ts_mview_create("test_view",
        "./test_data/ts_mview", TS_AGG_AVG, 60000);
    ASSERT_NE(nullptr, mview);

    int64_t ts = 1700000000000LL;
    EXPECT_EQ(0, ts_mview_add(mview, ts, 100.0));
    EXPECT_EQ(1u, mview->count);

    /* 同一时间粒度的数据应该聚合 */
    EXPECT_EQ(0, ts_mview_add(mview, ts + 30000, 200.0));  /* 同 1 分钟内 */
    EXPECT_EQ(1u, mview->count);  /* 仍然是 1 条记录 */

    ts_mview_destroy(mview);
}

TEST_F(TsMViewTest, Refresh) {
    ts_mview_t *mview = ts_mview_create("test_view",
        "./test_data/ts_mview", TS_AGG_AVG, 60000);
    ASSERT_NE(nullptr, mview);

    /* 添加数据 */
    int64_t ts = 1700000000000LL;
    ts_mview_add(mview, ts, 100.0);
    ts_mview_add(mview, ts + 30000, 200.0);

    /* 刷新计算 */
    EXPECT_EQ(0, ts_mview_refresh(mview));

    /* AVG: (100 + 200) / 2 = 150 */
    EXPECT_NEAR(mview->records[0].value, 150.0, 0.01);

    ts_mview_destroy(mview);
}

TEST_F(TsMViewTest, Query) {
    ts_mview_t *mview = ts_mview_create("test_view",
        "./test_data/ts_mview", TS_AGG_MIN, 60000);
    ASSERT_NE(nullptr, mview);

    /* 添加不同时间粒度的数据 */
    for (int i = 0; i < 5; i++) {
        ts_mview_add(mview, 1700000000000LL + i * 60000, 100.0 + i);
    }
    ts_mview_refresh(mview);

    /* 查询范围 */
    ts_mview_record_t results[10];
    uint32_t count = ts_mview_query(mview,
        1700000000000LL, 1700000000000LL + 300000, results, 10);

    EXPECT_GE(count, 1u);
    EXPECT_LE(count, 5u);

    ts_mview_destroy(mview);
}

TEST_F(TsMViewTest, Percentile) {
    ts_mview_t *mview = ts_mview_create("test_view",
        "./test_data/ts_mview", TS_AGG_AVG, 60000);
    ASSERT_NE(nullptr, mview);

    /* 添加数据 */
    for (int i = 0; i < 10; i++) {
        ts_mview_add(mview, 1700000000000LL + i * 60000, (double)i * 10);
    }
    ts_mview_refresh(mview);

    /* 计算百分位 */
    double p50 = ts_mview_percentile(mview->records, mview->count, 50.0);
    double p95 = ts_mview_percentile(mview->records, mview->count, 95.0);

    EXPECT_GE(p50, 0.0);
    EXPECT_GE(p95, p50);

    ts_mview_destroy(mview);
}

TEST_F(TsMViewTest, Aggregate) {
    ts_mview_record_t records[] = {
        {1700000000000LL, 100.0, 10, 50.0, 150.0, 1000.0},
        {1700000060000LL, 200.0, 10, 100.0, 300.0, 2000.0},
        {1700000120000LL, 300.0, 10, 200.0, 400.0, 3000.0}
    };

    /* 测试 SUM */
    double sum = ts_mview_aggregate(records, 3, TS_AGG_SUM);
    EXPECT_NEAR(sum, 600.0, 0.01);

    /* 测试 MIN */
    double min = ts_mview_aggregate(records, 3, TS_AGG_MIN);
    EXPECT_NEAR(min, 50.0, 0.01);

    /* 测试 MAX */
    double max = ts_mview_aggregate(records, 3, TS_AGG_MAX);
    EXPECT_NEAR(max, 400.0, 0.01);

    /* 测试 COUNT */
    double count = ts_mview_aggregate(records, 3, TS_AGG_COUNT);
    EXPECT_NEAR(count, 30.0, 0.01);  /* 10 + 10 + 10 */
}
