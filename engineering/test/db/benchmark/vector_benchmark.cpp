/**
 * @file vector_benchmark.cpp
 * @brief 向量索引基准测试
 */
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <db/optimizer/optimizer.h>
#include <db/sharding/sharding.h>
#include <sys/stat.h>
#include <time.h>
#include <cmath>

// 测试夹具
class VectorBenchmarkTest : public ::testing::Test {
protected:
    VectorAPI *api;
    const char *test_data_dir;

    void SetUp() override {
        test_data_dir = "./benchmark_data";
#ifdef _WIN32
        mkdir(test_data_dir);
#else
        mkdir(test_data_dir, 0755);
#endif
        api = vector_api_create(test_data_dir);
        ASSERT_NE(nullptr, api);
    }

    void TearDown() override {
        if (api) vector_api_destroy(api);
    }
};

// ============================================================
// 向量插入性能基准测试
// ============================================================

TEST_F(VectorBenchmarkTest, BulkInsertPerf1000) {
    // 创建集合
    VectorCreateParams params = {
        .name = "bench_1k",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int n = 1000;
    float **vectors = (float **)malloc(n * sizeof(float *));
    for (int i = 0; i < n; i++) {
        vectors[i] = (float *)malloc(128 * sizeof(float));
        for (int d = 0; d < 128; d++) {
            vectors[i][d] = (float)(i + d) / 128.0f;
        }
    }

    VectorInsertParams insert_params = {
        .collection = "bench_1k",
        .n = n,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int64_t start = clock();
    int inserted = vector_api_insert(api, &insert_params, NULL);
    int64_t elapsed = clock() - start;

    ASSERT_EQ(n, inserted);
    printf("\n  [BulkInsert 1K] vectors=%d, time=%.2f ms, throughput=%.0f vectors/sec\n",
           n, elapsed * 1000.0 / CLOCKS_PER_SEC,
           n * CLOCKS_PER_SEC / (double)elapsed);

    for (int i = 0; i < n; i++) free(vectors[i]);
    free(vectors);
}

TEST_F(VectorBenchmarkTest, BulkInsertPerf10K) {
    VectorCreateParams params = {
        .name = "bench_10k",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int n = 10000;
    float **vectors = (float **)malloc(n * sizeof(float *));
    for (int i = 0; i < n; i++) {
        vectors[i] = (float *)malloc(128 * sizeof(float));
        for (int d = 0; d < 128; d++) {
            vectors[i][d] = (float)(rand() % 1000) / 100.0f;
        }
    }

    VectorInsertParams insert_params = {
        .collection = "bench_10k",
        .n = n,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };

    int64_t start = clock();
    int inserted = vector_api_insert(api, &insert_params, NULL);
    int64_t elapsed = clock() - start;

    ASSERT_EQ(n, inserted);
    printf("\n  [BulkInsert 10K] vectors=%d, time=%.2f ms, throughput=%.0f vectors/sec\n",
           n, elapsed * 1000.0 / CLOCKS_PER_SEC,
           n * CLOCKS_PER_SEC / (double)elapsed);

    for (int i = 0; i < n; i++) free(vectors[i]);
    free(vectors);
}

// ============================================================
// 向量搜索性能基准测试
// ============================================================

TEST_F(VectorBenchmarkTest, SearchPerf1K) {
    // 准备数据
    VectorCreateParams params = {
        .name = "search_bench",
        .dimension = 128,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int n = 1000;
    float **vectors = (float **)malloc(n * sizeof(float *));
    for (int i = 0; i < n; i++) {
        vectors[i] = (float *)malloc(128 * sizeof(float));
        for (int d = 0; d < 128; d++) {
            vectors[i][d] = (float)i / (float)n;
        }
    }

    VectorInsertParams insert_params = {
        .collection = "search_bench",
        .n = n,
        .ids = NULL,
        .vectors = vectors,
        .metadata = NULL,
        .metadata_sizes = NULL
    };
    vector_api_insert(api, &insert_params, NULL);

    for (int i = 0; i < n; i++) free(vectors[i]);
    free(vectors);

    // 搜索基准测试
    float query[128];
    for (int d = 0; d < 128; d++) query[d] = 0.5f;

    VectorSearchParams search_params = {
        .collection = "search_bench",
        .query = query,
        .top_k = 10,
        .radius = 0,
        .with_distance = true,
        .with_metadata = false,
        .filter = NULL
    };

    const int iterations = 1000;
    int64_t start = clock();
    for (int i = 0; i < iterations; i++) {
        VectorSearchResults *results = vector_api_search(api, &search_params);
        if (results) vector_api_results_free(results);
    }
    int64_t elapsed = clock() - start;

    printf("\n  [Search 1K vectors] iterations=%d, total_time=%.2f ms, avg_latency=%.2f us\n",
           iterations, elapsed * 1000.0 / CLOCKS_PER_SEC,
           elapsed * 1000000.0 / (iterations * CLOCKS_PER_SEC));
}

// ============================================================
// 代价模型基准测试
// ============================================================

TEST(OptimizerBenchmark, CostModelHNSW) {
    int64_t num_vectors = 1000000;
    int32_t top_k = 10;
    int32_t ef_search = 100;

    double hnsw_cost = cost_vector_hnsw(ef_search, num_vectors, top_k);
    double ivf_cost = cost_vector_ivf(ef_search / 10, num_vectors, top_k);
    double diskann_cost = cost_vector_diskann(ef_search, num_vectors, top_k);

    printf("\n  [CostModel] num_vectors=%lld, top_k=%d\n", (long long)num_vectors, top_k);
    printf("    HNSW cost:   %.2f\n", hnsw_cost);
    printf("    IVF cost:     %.2f\n", ivf_cost);
    printf("    DiskANN cost: %.2f\n", diskann_cost);

    const char *best = select_best_vector_index(num_vectors, top_k, ef_search, ef_search / 10);
    printf("    Best index:   %s\n", best);

    EXPECT_GT(hnsw_cost, 0);
    EXPECT_GT(ivf_cost, 0);
    EXPECT_GT(diskann_cost, 0);
}

// ============================================================
// 分片基准测试
// ============================================================

TEST(ShardingBenchmark, ConsistentHashPerf) {
    const int n = 10000;
    uint64_t *hashes = (uint64_t *)malloc(n * sizeof(uint64_t));

    float vector[128];
    for (int d = 0; d < 128; d++) vector[d] = (float)d / 128.0f;

    int64_t start = clock();
    for (int i = 0; i < n; i++) {
        /* 修改向量值以模拟不同向量 */
        vector[0] = (float)i / n;
        hashes[i] = vector_consistent_hash(vector, 128);
    }
    int64_t elapsed = clock() - start;

    printf("\n  [ConsistentHash] vectors=%d, time=%.2f ms, throughput=%.0f ops/sec\n",
           n, elapsed * 1000.0 / CLOCKS_PER_SEC,
           n * CLOCKS_PER_SEC / (double)elapsed);

    /* 验证哈希分布 */
    double mean = 0;
    for (int i = 0; i < n; i++) mean += hashes[i];
    mean /= n;
    printf("    Hash mean: %.2e\n", mean);

    free(hashes);
    EXPECT_GT(hashes[0], 0);
}

// ============================================================
// 吞吐量测试
// ============================================================

TEST_F(VectorBenchmarkTest, ThroughputTest) {
    VectorCreateParams params = {
        .name = "throughput_test",
        .dimension = 64,
        .index_type = VECTOR_INDEX_HNSW,
        .metric_type = VECTOR_METRIC_L2
    };
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int batch_size = 100;
    const int batches = 100;
    const int total = batch_size * batches;

    int64_t total_start = clock();

    for (int b = 0; b < batches; b++) {
        float **vectors = (float **)malloc(batch_size * sizeof(float *));
        for (int i = 0; i < batch_size; i++) {
            vectors[i] = (float *)malloc(64 * sizeof(float));
            for (int d = 0; d < 64; d++) {
                vectors[i][d] = (float)rand() / RAND_MAX;
            }
        }

        VectorInsertParams insert_params = {
            .collection = "throughput_test",
            .n = batch_size,
            .ids = NULL,
            .vectors = vectors,
            .metadata = NULL,
            .metadata_sizes = NULL
        };
        vector_api_insert(api, &insert_params, NULL);

        for (int i = 0; i < batch_size; i++) free(vectors[i]);
        free(vectors);
    }

    int64_t total_elapsed = clock() - total_start;

    printf("\n  [Throughput] total=%d, time=%.2f s, throughput=%.0f vectors/sec\n",
           total, total_elapsed * 1.0 / CLOCKS_PER_SEC,
           total * CLOCKS_PER_SEC / (double)total_elapsed);
}

// 主函数
int main(int argc, char **argv) {
    printf("\n========================================\n");
    printf("  MiniVecDB 基准测试套件\n");
    printf("========================================\n\n");

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
