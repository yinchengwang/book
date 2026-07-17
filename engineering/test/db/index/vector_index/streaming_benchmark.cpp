/**
 * @file streaming_benchmark.cpp
 * @brief 流式索引性能基准测试
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <chrono>

extern "C" {
#include "db/index/vector_index/streaming/streaming_index.h"
#include "db/index/vector_index/streaming/vector_write_buffer.h"
}

/* ========================================================================
 * 测试配置
 * ======================================================================== */

#define BENCHMARK_DIMS 128
#define BENCHMARK_BATCH_SIZE 1000
#define BENCHMARK_TOTAL_VECTORS 10000

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

static void generate_random_vectors(float *vectors, int32_t n, int32_t dims) {
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

static double get_time_ms() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(duration).count();
}

/* ========================================================================
 * 基准测试
 * ======================================================================== */

static void benchmark_buffer_operations(int32_t total_vectors, int32_t batch_size, int32_t dims) {
    printf("\n========================================\n");
    printf("流式缓冲区性能测试\n");
    printf("========================================\n");
    printf("总向量数: %d\n", total_vectors);
    printf("批次大小: %d\n", batch_size);
    printf("维度: %d\n", dims);
    printf("----------------------------------------\n");

    /* 创建缓冲区 */
    vector_buffer_config_t config = vector_buffer_config_default(dims);
    config.capacity = 100000;
    config.flush_threshold = 5000;
    config.batch_size = batch_size;

    vector_write_buffer_t *buffer = vector_buffer_create(&config);
    if (buffer == nullptr) {
        printf("错误: 无法创建缓冲区\n");
        return;
    }

    float *vectors = (float *)malloc((size_t)batch_size * dims * sizeof(float));

    /* 测试批量写入 */
    double start_time = get_time_ms();
    int32_t batches = total_vectors / batch_size;
    int32_t total_pushed = 0;

    for (int32_t b = 0; b < batches; b++) {
        generate_random_vectors(vectors, batch_size, dims);
        int32_t pushed = vector_buffer_push_batch(buffer, vectors, batch_size);
        total_pushed += pushed;
    }

    double write_time = get_time_ms() - start_time;
    double write_qps = (double)total_pushed / (write_time / 1000.0);

    printf("批量写入:\n");
    printf("  - 总写入向量: %d\n", total_pushed);
    printf("  - 耗时: %.2f ms\n", write_time);
    printf("  - QPS: %.0f\n", write_qps);
    printf("----------------------------------------\n");

    /* 测试刷新 */
    float *flush_buffer = (float *)malloc((size_t)config.flush_threshold * dims * sizeof(float));
    double flush_start = get_time_ms();
    int32_t flushed_total = 0;

    while (vector_buffer_size(buffer) > 0) {
        int32_t flushed = vector_buffer_flush(buffer, flush_buffer, config.flush_threshold);
        flushed_total += flushed;
    }

    double flush_time = get_time_ms() - flush_start;
    double flush_qps = (double)flushed_total / (flush_time / 1000.0);

    printf("缓冲区刷新:\n");
    printf("  - 总刷新向量: %d\n", flushed_total);
    printf("  - 耗时: %.2f ms\n", flush_time);
    printf("  - QPS: %.0f\n", flush_qps);
    printf("========================================\n");

    free(vectors);
    free(flush_buffer);
    vector_buffer_destroy(buffer);
}

static void benchmark_streaming_index(int32_t total_vectors, int32_t dims) {
    printf("\n========================================\n");
    printf("流式索引 vs 同步索引 性能对比\n");
    printf("========================================\n");
    printf("总向量数: %d\n", total_vectors);
    printf("维度: %d\n", dims);
    printf("----------------------------------------\n");

    /* 创建流式索引 */
    streaming_index_config_t config = streaming_index_config_default(STREAMING_INDEX_HNSW, dims);
    config.buffer_capacity = 100000;
    config.buffer_flush_threshold = 5000;
    config.merge_interval_ms = 100;

    streaming_index_t *index = streaming_index_create(&config);
    if (index == nullptr) {
        printf("错误: 无法创建流式索引\n");
        return;
    }

    float *vectors = (float *)malloc((size_t)BENCHMARK_BATCH_SIZE * dims * sizeof(float));

    /* 流式写入测试（只添加到缓冲区，不同步合并） */
    double start_time = get_time_ms();
    int32_t batches = total_vectors / BENCHMARK_BATCH_SIZE;
    int32_t total_added = 0;

    for (int32_t b = 0; b < batches; b++) {
        generate_random_vectors(vectors, BENCHMARK_BATCH_SIZE, dims);
        int32_t added = streaming_index_streaming_add(index, vectors, BENCHMARK_BATCH_SIZE);
        if (added > 0) {
            total_added += added;
        }
    }

    double stream_time = get_time_ms() - start_time;
    double stream_qps = (double)total_added / (stream_time / 1000.0);

    printf("流式写入 (仅缓冲):\n");
    printf("  - 总写入向量: %d\n", total_added);
    printf("  - 耗时: %.2f ms\n", stream_time);
    printf("  - QPS: %.0f\n", stream_qps);
    printf("----------------------------------------\n");

    /* 统计信息 */
    streaming_index_stats_t stats;
    streaming_index_get_stats(index, &stats);

    printf("索引统计:\n");
    printf("  - 总向量: %d\n", stats.total_vectors);
    printf("  - 主索引向量: %d\n", stats.main_index_vectors);
    printf("  - 缓冲区向量: %d\n", stats.buffer_vectors);
    printf("  - 已完成合并次数: %d\n", stats.completed_merges);
    printf("========================================\n");

    free(vectors);
    streaming_index_destroy(index);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    printf("========================================\n");
    printf("流式向量索引 性能基准测试\n");
    printf("========================================\n");

    srand((unsigned)time(nullptr));

    /* 缓冲区性能测试 */
    benchmark_buffer_operations(50000, BENCHMARK_BATCH_SIZE, BENCHMARK_DIMS);

    /* 索引性能测试（小规模） */
    benchmark_streaming_index(BENCHMARK_TOTAL_VECTORS, BENCHMARK_DIMS);

    printf("\n基准测试完成!\n");
    return 0;
}
