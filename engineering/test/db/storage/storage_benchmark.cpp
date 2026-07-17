/**
 * @file storage_benchmark.cpp
 * @brief 存储引擎性能基准测试
 *
 * 测试各存储引擎的读写性能和压缩率
 */
#include <gtest/gtest.h>
#include "db/kv_engine.h"
#include "db/ts_engine.h"
#include "db/doc_engine.h"
#include "db/graph_engine.h"
#include "db/yang_engine.h"
#include "db/storage/ts/ts_compress.h"
#include "db/log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <chrono>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/**
 * @brief 性能测试夹具
 */
class StorageBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_ERROR;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 确保测试目录存在 */
#ifdef _WIN32
        mkdir("./test_data/benchmark");
#else
        mkdir("./test_data", 0755);
        mkdir("./test_data/benchmark", 0755);
#endif
    }

    void TearDown() override {
        log_shutdown();
        system("rm -rf ./test_data/benchmark");
    }
};

/* ========================================================================
 * KV 引擎性能测试
 * ======================================================================== */

TEST_F(StorageBenchmarkTest, KVPutPerformance) {
    ASSERT_EQ(0, kv_engine_init("./test_data/benchmark/kv"));
    ASSERT_EQ(0, kv_engine_create("benchmark", NULL));

    void *rel = kv_engine_open("benchmark", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    const int NUM_OPS = 10000;
    char key[32], value[256], data[512];

    /* 测试写入性能 */
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_OPS; i++) {
        snprintf(key, sizeof(key), "key_%08d", i);
        snprintf(value, sizeof(value), "value_%08d_data_here", i);
        /* KV 引擎的 insert 格式: [key_len(4) + key + value_len(4) + value] */
        size_t key_len = strlen(key);
        size_t value_len = strlen(value);
        size_t offset = 0;
        memcpy(data + offset, &key_len, sizeof(size_t)); offset += sizeof(size_t);
        memcpy(data + offset, key, key_len); offset += key_len;
        memcpy(data + offset, &value_len, sizeof(size_t)); offset += sizeof(size_t);
        memcpy(data + offset, value, value_len); offset += value_len;
        kv_engine_insert(rel, data, offset);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    printf("\n[KV Write] %d ops in %lld ms (%.2f ops/ms)\n",
           NUM_OPS, (long long)duration, (double)NUM_OPS / duration);

    kv_engine_close(rel);
    kv_engine_drop("benchmark");
    kv_engine_shutdown();
}

TEST_F(StorageBenchmarkTest, KVGetPerformance) {
    ASSERT_EQ(0, kv_engine_init("./test_data/benchmark/kv"));
    ASSERT_EQ(0, kv_engine_create("benchmark", NULL));

    void *rel = kv_engine_open("benchmark", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    /* 写入测试数据 */
    const int NUM_OPS = 1000;
    char key[32], value[256], data[512];
    for (int i = 0; i < NUM_OPS; i++) {
        snprintf(key, sizeof(key), "key_%08d", i);
        snprintf(value, sizeof(value), "value_%08d_data_here", i);
        size_t key_len = strlen(key);
        size_t value_len = strlen(value);
        size_t offset = 0;
        memcpy(data + offset, &key_len, sizeof(size_t)); offset += sizeof(size_t);
        memcpy(data + offset, key, key_len); offset += key_len;
        memcpy(data + offset, &value_len, sizeof(size_t)); offset += sizeof(size_t);
        memcpy(data + offset, value, value_len); offset += value_len;
        kv_engine_insert(rel, data, offset);
    }

    /* 测试读取性能 */
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_OPS; i++) {
        snprintf(key, sizeof(key), "key_%08d", i);
        void *out_value = NULL;
        size_t out_len = 0;
        kv_engine_get(rel, key, strlen(key), &out_value, &out_len);
        if (out_value) free(out_value);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    printf("\n[KV Read] %d ops in %lld ms (%.2f ops/ms)\n",
           NUM_OPS, (long long)duration, (double)NUM_OPS / duration);

    kv_engine_close(rel);
    kv_engine_drop("benchmark");
    kv_engine_shutdown();
}

/* ========================================================================
 * 时序引擎性能测试
 * ======================================================================== */

TEST_F(StorageBenchmarkTest, TsInsertPerformance) {
    ASSERT_EQ(0, ts_engine_init("./test_data/benchmark/ts"));
    ASSERT_EQ(0, ts_engine_create("benchmark", NULL));

    void *rel = ts_engine_open("benchmark", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    const int NUM_OPS = 10000;
    int64_t base_ts = 1700000000000LL;

    /* 测试写入性能 */
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_OPS; i++) {
        int64_t ts = base_ts + i * 1000;  /* 每秒一个点 */
        double value = 100.0 + i * 0.1;
        char data[16];
        memcpy(data, &ts, sizeof(int64_t));
        memcpy(data + sizeof(int64_t), &value, sizeof(double));
        ts_engine_insert(rel, data, sizeof(data));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    printf("\n[TS Write] %d points in %lld ms (%.2f pts/ms)\n",
           NUM_OPS, (long long)duration, (double)NUM_OPS / duration);

    ts_engine_close(rel);
    ts_engine_drop("benchmark");
    ts_engine_shutdown();
}

TEST_F(StorageBenchmarkTest, TsCompressRatio) {
    /* 测试压缩率 */
    ts_compressor_t *comp = ts_compressor_create();
    ASSERT_NE(nullptr, comp);

    const int NUM_POINTS = 4096;  /* 一个压缩块 */
    int64_t base_ts = 1700000000000LL;

    /* 添加数据 */
    for (int i = 0; i < NUM_POINTS; i++) {
        int64_t ts = base_ts + i * 1000;
        double value = 100.0 + (i % 100) * 0.5;  /* 变化较小，便于压缩 */
        ts_compress_add(comp, ts, value);
    }

    /* 获取统计 */
    uint64_t total_points = 0;
    double compression_ratio = 0.0;
    ts_compress_get_stats(comp, &total_points, &compression_ratio);

    size_t original_size = NUM_POINTS * sizeof(ts_record_t);
    size_t compressed_size = (size_t)(original_size / compression_ratio);

    printf("\n[TS Compress] %d points:\n", NUM_POINTS);
    printf("  Original: %zu bytes\n", original_size);
    printf("  Compressed: %zu bytes\n", compressed_size);
    printf("  Ratio: %.2fx\n", compression_ratio);

    ts_compressor_free(comp);
}

/* ========================================================================
 * 文档引擎性能测试
 * ======================================================================== */

TEST_F(StorageBenchmarkTest, DocInsertPerformance) {
    ASSERT_EQ(0, doc_engine_init("./test_data/benchmark/doc"));
    ASSERT_EQ(0, doc_engine_create("benchmark", NULL));

    void *rel = doc_engine_open("benchmark", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    const int NUM_OPS = 1000;
    char doc_content[512];

    /* doc_engine_insert 格式: [id_len(4) + id + json_len(4) + json] */
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_OPS; i++) {
        snprintf(doc_content, sizeof(doc_content),
                 R"({"id":%d,"name":"doc_%d","value":%f,"tags":["tag1","tag2"]})",
                 i, i, 100.0 + i);
        size_t json_len = strlen(doc_content);
        char data[1024];
        size_t offset = 0;
        memcpy(data + offset, &json_len, sizeof(size_t)); offset += sizeof(size_t);
        memcpy(data + offset, doc_content, json_len); offset += json_len;
        doc_engine_insert(rel, data, offset);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    printf("\n[Doc Write] %d docs in %lld ms (%.2f docs/ms)\n",
           NUM_OPS, (long long)duration, (double)NUM_OPS / duration);

    doc_engine_close(rel);
    doc_engine_drop("benchmark");
    doc_engine_shutdown();
}

/* ========================================================================
 * 图引擎性能测试
 * ======================================================================== */

TEST_F(StorageBenchmarkTest, GraphAddVerticesPerformance) {
    ASSERT_EQ(0, graph_engine_init("./test_data/benchmark/graph"));
    ASSERT_EQ(0, graph_engine_create("benchmark", NULL));

    void *rel = graph_engine_open("benchmark", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(nullptr, rel);

    const int NUM_VERTICES = 1000;
    uint8_t vdata[64];

    /* 测试添加顶点性能 */
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_VERTICES; i++) {
        snprintf((char*)vdata, sizeof(vdata), "vertex_%d", i);
        graph_engine_add_vertex(rel, vdata, strlen((char*)vdata));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    printf("\n[Graph Add Vertices] %d vertices in %lld ms (%.2f vtx/ms)\n",
           NUM_VERTICES, (long long)duration, (double)NUM_VERTICES / duration);

    graph_engine_close(rel);
    graph_engine_drop("benchmark");
    graph_engine_shutdown();
}
