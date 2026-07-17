/**
 * @file vdb_stress_test.cpp
 * @brief 向量数据库压力测试与并发测试
 *
 * 测试范围：
 * 1. 多线程并发插入
 * 2. 多线程并发查询
 * 3. 读写混合压力测试
 */
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <db/core/vector_query.h>
#include <thread>
#include <atomic>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

// ============================================================
// 测试配置
// ============================================================

constexpr int DEFAULT_DIM = 128;
constexpr int DEFAULT_TOP_K = 10;

// ============================================================
// 辅助函数
// ============================================================

static VectorCreateParams make_create_params(const char* name, int32_t dim,
                                              VectorIndexType idx_type,
                                              VectorMetricType metric) {
    VectorCreateParams p;
    p.name = name;
    p.dimension = dim;
    p.index_type = idx_type;
    p.metric_type = metric;
    p.index_ef_search = 100;
    p.index_m = 16;
    p.index_ef_construction = 200;
    return p;
}

static VectorInsertParams make_insert_params(const char* coll, int32_t n,
                                              int64_t* ids, float** vecs,
                                              void** meta, int32_t* meta_sizes) {
    VectorInsertParams p;
    p.collection = coll;
    p.n = n;
    p.ids = ids;
    p.vectors = vecs;
    p.metadata = meta;
    p.metadata_sizes = meta_sizes;
    return p;
}

static VectorSearchParams make_search_params(const char* coll, const float* query,
                                              int32_t top_k, float radius,
                                              bool with_dist, bool with_meta,
                                              const char* filter) {
    VectorSearchParams p;
    p.collection = coll;
    p.query = query;
    p.top_k = top_k;
    p.radius = radius;
    p.with_distance = with_dist;
    p.with_metadata = with_meta;
    p.filter = filter;
    return p;
}

// 性能统计
struct PerformanceStats {
    std::atomic<uint64_t> total_operations{0};
    std::atomic<uint64_t> successful_operations{0};
    std::atomic<uint64_t> failed_operations{0};
    std::atomic<uint64_t> total_latency_us{0};
    std::atomic<uint64_t> min_latency_us{UINT64_MAX};
    std::atomic<uint64_t> max_latency_us{0};

    void record_operation(bool success, uint64_t latency_us) {
        total_operations.fetch_add(1);
        if (success) {
            successful_operations.fetch_add(1);
        } else {
            failed_operations.fetch_add(1);
        }
        total_latency_us.fetch_add(latency_us);

        uint64_t prev_min = min_latency_us.load();
        while (latency_us < prev_min && !min_latency_us.compare_exchange_weak(prev_min, latency_us)) {}

        uint64_t prev_max = max_latency_us.load();
        while (latency_us > prev_max && !max_latency_us.compare_exchange_weak(prev_max, latency_us)) {}
    }

    void print_stats(const char* operation) {
        uint64_t total = total_operations.load();
        uint64_t success = successful_operations.load();
        uint64_t latency_sum = total_latency_us.load();
        uint64_t min_lat = min_latency_us.load();
        uint64_t max_lat = max_latency_us.load();

        if (total == 0) {
            printf("[%s] No operations recorded\n", operation);
            return;
        }

        double avg_lat = (double)latency_sum / total / 1000.0;
        double qps = success * 1000000.0 / latency_sum;

        printf("[%s] Total: %llu, Success: %llu\n", operation, total, success);
        printf("[%s] Latency - Avg: %.2f ms, Min: %.2f ms, Max: %.2f ms\n",
               operation, avg_lat, min_lat / 1000.0, max_lat / 1000.0);
        printf("[%s] QPS: %.2f\n", operation, qps);
    }
};

// ============================================================
// 测试夹具
// ============================================================

class VDBStressTest : public ::testing::Test {
protected:
    static const char* test_data_dir;
    VectorAPI* api = nullptr;

    static void SetUpTestSuite() {
#ifdef _WIN32
        _mkdir(test_data_dir);
#else
        mkdir(test_data_dir, 0755);
#endif
    }

    void SetUp() override {
        api = vector_api_create(test_data_dir);
        ASSERT_NE(nullptr, api);
    }

    void TearDown() override {
        if (api) {
            vector_api_destroy(api);
            api = nullptr;
        }
    }

    // 生成随机向量
    static float* generate_vector(int32_t dim) {
        static thread_local std::mt19937 rng(
            std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
            std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float* v = (float*)malloc(dim * sizeof(float));
        for (int i = 0; i < dim; i++) {
            v[i] = dist(rng);
        }
        return v;
    }

    // 生成多个随机向量
    static float** generate_vectors(int32_t n, int32_t dim) {
        float** vectors = (float**)malloc(n * sizeof(float*));
        for (int i = 0; i < n; i++) {
            vectors[i] = generate_vector(dim);
        }
        return vectors;
    }

    // 释放向量数组
    static void free_vectors(float** vectors, int32_t n) {
        if (!vectors) return;
        for (int i = 0; i < n; i++) {
            free(vectors[i]);
        }
        free(vectors);
    }
};

const char* VDBStressTest::test_data_dir = "./test-results/vdb_stress";

// ============================================================
// 5. 并发测试
// ============================================================

/**
 * 测试 5.2：多线程并发插入
 */
TEST_F(VDBStressTest, ConcurrentInsert) {
    VectorCreateParams params = make_create_params(
        "test_concurrent_insert", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int num_threads = 10;
    const int vectors_per_thread = 100;
    const int total_vectors = num_threads * vectors_per_thread;

    std::atomic<int> success_count(0);
    std::atomic<int> fail_count(0);
    PerformanceStats stats;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &stats, &success_count, &fail_count, vectors_per_thread]() {
            float** vectors = generate_vectors(vectors_per_thread, DEFAULT_DIM);
            float** vec_ptrs = (float**)malloc(vectors_per_thread * sizeof(float*));
            for (int i = 0; i < vectors_per_thread; i++) {
                vec_ptrs[i] = vectors[i];
            }

            auto op_start = std::chrono::high_resolution_clock::now();
            VectorInsertParams insert_params = make_insert_params(
                "test_concurrent_insert", vectors_per_thread, NULL, vec_ptrs, NULL, NULL);

            int inserted = vector_api_insert(api, &insert_params, NULL);
            auto op_end = std::chrono::high_resolution_clock::now();
            uint64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();

            stats.record_operation(inserted == vectors_per_thread, latency_us);

            if (inserted == vectors_per_thread) {
                success_count.fetch_add(inserted);
            } else {
                fail_count.fetch_add(vectors_per_thread - inserted);
            }

            free(vec_ptrs);
            free_vectors(vectors, vectors_per_thread);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    printf("\n=== Concurrent Insert Results ===\n");
    printf("Total time: %.2f ms\n", total_time_ms);
    printf("Inserted: %d, Failed: %d\n", success_count.load(), fail_count.load());
    stats.print_stats("Insert");

    EXPECT_GE(success_count.load(), total_vectors * 0.8);
    int32_t size = vector_api_size(api, "test_concurrent_insert");
    printf("Collection size: %d\n", size);
}

/**
 * 测试 5.3：多线程并发查询
 */
TEST_F(VDBStressTest, ConcurrentSearch) {
    VectorCreateParams params = make_create_params(
        "test_concurrent_search", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int pre_insert = 1000;
    float** vectors = generate_vectors(pre_insert, DEFAULT_DIM);
    float** vec_ptrs = (float**)malloc(pre_insert * sizeof(float*));
    for (int i = 0; i < pre_insert; i++) {
        vec_ptrs[i] = vectors[i];
    }

    VectorInsertParams insert_params = make_insert_params(
        "test_concurrent_search", pre_insert, NULL, vec_ptrs, NULL, NULL);
    ASSERT_EQ(pre_insert, vector_api_insert(api, &insert_params, NULL));
    free(vec_ptrs);
    free_vectors(vectors, pre_insert);

    const int num_threads = 10;
    const int queries_per_thread = 50;
    const int total_queries = num_threads * queries_per_thread;

    std::atomic<int> success_count(0);
    std::atomic<int> fail_count(0);
    PerformanceStats stats;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &stats, &success_count, &fail_count, queries_per_thread]() {
            for (int q = 0; q < queries_per_thread; q++) {
                float* query = generate_vector(DEFAULT_DIM);

                auto op_start = std::chrono::high_resolution_clock::now();
                VectorSearchParams search_params = make_search_params(
                    "test_concurrent_search", query, DEFAULT_TOP_K, 0, true, false, NULL);

                VectorSearchResults* results = vector_api_search(api, &search_params);
                auto op_end = std::chrono::high_resolution_clock::now();
                uint64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    op_end - op_start).count();

                bool success = (results != nullptr);
                stats.record_operation(success, latency_us);

                if (success) {
                    success_count.fetch_add(1);
                    vector_api_results_free(results);
                } else {
                    fail_count.fetch_add(1);
                }

                free(query);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    printf("\n=== Concurrent Search Results ===\n");
    printf("Total time: %.2f ms\n", total_time_ms);
    printf("Success: %d, Failed: %d\n", success_count.load(), fail_count.load());
    stats.print_stats("Search");

    EXPECT_GE(success_count.load(), total_queries * 0.9);
}

/**
 * 测试 5.4：读写混合
 */
TEST_F(VDBStressTest, ReadWriteMix) {
    VectorCreateParams params = make_create_params(
        "test_mixed", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int pre_insert = 500;
    float** vectors = generate_vectors(pre_insert, DEFAULT_DIM);
    float** vec_ptrs = (float**)malloc(pre_insert * sizeof(float*));
    for (int i = 0; i < pre_insert; i++) {
        vec_ptrs[i] = vectors[i];
    }

    VectorInsertParams insert_params = make_insert_params(
        "test_mixed", pre_insert, NULL, vec_ptrs, NULL, NULL);
    ASSERT_EQ(pre_insert, vector_api_insert(api, &insert_params, NULL));
    free(vec_ptrs);
    free_vectors(vectors, pre_insert);

    const int num_threads = 8;
    const int ops_per_thread = 20;

    std::atomic<int> insert_success(0);
    std::atomic<int> search_success(0);
    PerformanceStats insert_stats;
    PerformanceStats search_stats;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &insert_stats, &search_stats, &insert_success,
                             &search_success, ops_per_thread, t]() {
            for (int op = 0; op < ops_per_thread; op++) {
                bool is_insert = ((t * ops_per_thread + op) % 2 == 0);

                if (is_insert) {
                    float** insert_vecs = generate_vectors(10, DEFAULT_DIM);
                    float** insert_ptrs = (float**)malloc(10 * sizeof(float*));
                    for (int i = 0; i < 10; i++) {
                        insert_ptrs[i] = insert_vecs[i];
                    }

                    auto op_start = std::chrono::high_resolution_clock::now();
                    VectorInsertParams ip = make_insert_params(
                        "test_mixed", 10, NULL, insert_ptrs, NULL, NULL);

                    int inserted = vector_api_insert(api, &ip, NULL);
                    auto op_end = std::chrono::high_resolution_clock::now();
                    uint64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        op_end - op_start).count();

                    bool success = (inserted == 10);
                    insert_stats.record_operation(success, latency_us);
                    if (success) {
                        insert_success.fetch_add(1);
                    }

                    free(insert_ptrs);
                    free_vectors(insert_vecs, 10);
                } else {
                    float* query = generate_vector(DEFAULT_DIM);

                    auto op_start = std::chrono::high_resolution_clock::now();
                    VectorSearchParams sp = make_search_params(
                        "test_mixed", query, 10, 0, true, false, NULL);

                    VectorSearchResults* results = vector_api_search(api, &sp);
                    auto op_end = std::chrono::high_resolution_clock::now();
                    uint64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        op_end - op_start).count();

                    bool success = (results != nullptr);
                    search_stats.record_operation(success, latency_us);
                    if (success) {
                        search_success.fetch_add(1);
                        vector_api_results_free(results);
                    }

                    free(query);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    printf("\n=== Read-Write Mix Results ===\n");
    printf("Total time: %.2f ms\n", total_time_ms);
    printf("Insert success: %d, Search success: %d\n", insert_success.load(), search_success.load());
    insert_stats.print_stats("Insert");
    search_stats.print_stats("Search");

    int32_t final_size = vector_api_size(api, "test_mixed");
    printf("Final collection size: %d\n", final_size);
    EXPECT_GE(final_size, pre_insert);
}

// ============================================================
// 7. 性能基准测试
// ============================================================

/**
 * 测试 7.1：插入性能测试
 */
TEST_F(VDBStressTest, InsertPerformance) {
    VectorCreateParams params = make_create_params(
        "test_insert_perf", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int total_vectors = 10000;
    const int batch_sizes[] = {1, 10, 100, 1000};

    printf("\n=== Insert Performance (Total: %d vectors) ===\n", total_vectors);

    for (int batch_size : batch_sizes) {
        vector_api_drop_collection(api, "test_insert_perf");
        ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

        int num_batches = total_vectors / batch_size;
        auto start_time = std::chrono::high_resolution_clock::now();

        for (int b = 0; b < num_batches; b++) {
            float** vectors = generate_vectors(batch_size, DEFAULT_DIM);
            float** vec_ptrs = (float**)malloc(batch_size * sizeof(float*));
            for (int i = 0; i < batch_size; i++) {
                vec_ptrs[i] = vectors[i];
            }

            VectorInsertParams ip = make_insert_params(
                "test_insert_perf", batch_size, NULL, vec_ptrs, NULL, NULL);

            int inserted = vector_api_insert(api, &ip, NULL);
            EXPECT_EQ(batch_size, inserted);

            free(vec_ptrs);
            free_vectors(vectors, batch_size);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        double qps = total_vectors / (elapsed_ms / 1000.0);

        printf("Batch size %4d: %.2f ms total, %.2f vectors/sec\n",
               batch_size, elapsed_ms, qps);
    }
}

/**
 * 测试 7.2：查询性能测试（延迟 P50/P95/P99）
 */
TEST_F(VDBStressTest, SearchLatency) {
    VectorCreateParams params = make_create_params(
        "test_search_latency", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    const int num_vectors = 50000;
    const int batch_size = 1000;
    int num_batches = num_vectors / batch_size;

    for (int b = 0; b < num_batches; b++) {
        float** vectors = generate_vectors(batch_size, DEFAULT_DIM);
        float** vec_ptrs = (float**)malloc(batch_size * sizeof(float*));
        for (int i = 0; i < batch_size; i++) {
            vec_ptrs[i] = vectors[i];
        }

        VectorInsertParams ip = make_insert_params(
            "test_search_latency", batch_size, NULL, vec_ptrs, NULL, NULL);

        ASSERT_EQ(batch_size, vector_api_insert(api, &ip, NULL));
        free(vec_ptrs);
        free_vectors(vectors, batch_size);
    }

    const int num_queries = 1000;
    std::vector<uint64_t> latencies;
    latencies.reserve(num_queries);

    printf("\n=== Search Latency Test (%d queries, %d vectors) ===\n",
           num_queries, num_vectors);

    for (int q = 0; q < num_queries; q++) {
        float* query = generate_vector(DEFAULT_DIM);

        auto op_start = std::chrono::high_resolution_clock::now();
        VectorSearchParams sp = make_search_params(
            "test_search_latency", query, DEFAULT_TOP_K, 0, true, false, NULL);

        VectorSearchResults* results = vector_api_search(api, &sp);
        auto op_end = std::chrono::high_resolution_clock::now();

        if (results) {
            uint64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
                op_end - op_start).count();
            latencies.push_back(latency_us);
            vector_api_results_free(results);
        }

        free(query);
    }

    // 计算百分位数
    std::sort(latencies.begin(), latencies.end());
    size_t p50_idx = latencies.size() * 0.50;
    size_t p95_idx = latencies.size() * 0.95;
    size_t p99_idx = latencies.size() * 0.99;

    printf("Latency P50: %.2f ms\n", latencies[p50_idx] / 1000.0);
    printf("Latency P95: %.2f ms\n", latencies[p95_idx] / 1000.0);
    printf("Latency P99: %.2f ms\n", latencies[p99_idx] / 1000.0);
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
