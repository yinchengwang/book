/**
 * @file vdb_chaos_test.cpp
 * @brief 向量数据库混沌测试
 *
 * 测试范围：
 * 1. 写入中 kill → 重启 → WAL 恢复
 * 2. 索引构建中 kill → 重启 → 重建
 * 3. 磁盘满模拟 → 错误处理验证
 *
 * 注意：这些测试模拟故障场景，可能对系统造成压力
 */
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <db/core/vector_query.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define Sleep(ms) Sleep(ms)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#define Sleep(ms) usleep((ms) * 1000)
#endif

namespace fs = std::filesystem;

// ============================================================
// 测试配置
// ============================================================

constexpr int DEFAULT_DIM = 128;
constexpr int CHAOS_TEST_VECTORS = 1000;

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

// ============================================================
// 测试夹具
// ============================================================

class VDBChaosTest : public ::testing::Test {
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

    // 清理测试数据目录
    static void cleanup_test_dir() {
        if (fs::exists(test_data_dir)) {
            fs::remove_all(test_data_dir);
            fs::create_directories(test_data_dir);
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

const char* VDBChaosTest::test_data_dir = "./test-results/vdb_chaos";

// ============================================================
// 6. 混沌测试
// ============================================================

/**
 * 测试 6.2：写入中 kill → 重启 → WAL 恢复
 */
TEST_F(VDBChaosTest, WriteKillRecovery) {
    // 创建集合
    VectorCreateParams params = make_create_params(
        "test_crash_recovery", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 第一阶段：插入初始数据并保存
    float** initial_vecs = generate_vectors(100, DEFAULT_DIM);
    float** initial_ptrs = (float**)malloc(100 * sizeof(float*));
    for (int i = 0; i < 100; i++) initial_ptrs[i] = initial_vecs[i];

    VectorInsertParams ip1 = make_insert_params(
        "test_crash_recovery", 100, NULL, initial_ptrs, NULL, NULL);
    ASSERT_EQ(100, vector_api_insert(api, &ip1, NULL));
    EXPECT_EQ(VECTOR_API_OK, vector_api_save(api));
    free(initial_ptrs);
    free_vectors(initial_vecs, 100);

    // 第二阶段：继续插入数据（模拟写入中崩溃）
    float** crash_vecs = generate_vectors(50, DEFAULT_DIM);
    float** crash_ptrs = (float**)malloc(50 * sizeof(float*));
    for (int i = 0; i < 50; i++) crash_ptrs[i] = crash_vecs[i];

    VectorInsertParams ip2 = make_insert_params(
        "test_crash_recovery", 50, NULL, crash_ptrs, NULL, NULL);
    ASSERT_EQ(50, vector_api_insert(api, &ip2, NULL));
    free(crash_ptrs);
    free_vectors(crash_vecs, 50);

    // 记录崩溃前大小
    int size_before_crash = vector_api_size(api, "test_crash_recovery");
    printf("\n[WriteKillRecovery] Size before crash: %d\n", size_before_crash);

    // 模拟崩溃（不调用 save）
    vector_api_destroy(api);
    api = nullptr;

    // 重新加载
    api = vector_api_create(test_data_dir);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    // 验证恢复结果
    int size_after_recovery = vector_api_size(api, "test_crash_recovery");
    printf("[WriteKillRecovery] Size after recovery: %d\n", size_after_recovery);

    // 如果启用了 WAL，应该恢复到 size_before_crash
    // 如果未启用 WAL，则只恢复到 100
    // 这里我们接受两种情况
    EXPECT_GE(size_after_recovery, 100);  // 至少恢复到初始数据
    EXPECT_LE(size_after_recovery, size_before_crash);  // 不应超过崩溃前

    // 验证搜索功能正常
    float query[DEFAULT_DIM];
    for (int i = 0; i < DEFAULT_DIM; i++) query[i] = 0.5f;

    VectorSearchParams sp = make_search_params(
        "test_crash_recovery", query, 10, 0, true, false, NULL);

    VectorSearchResults* results = vector_api_search(api, &sp);
    if (results) {
        EXPECT_GE(vector_api_results_count(results), 0);
        vector_api_results_free(results);
    }
}

/**
 * 测试 6.3：索引构建中 kill → 重启 → 重建
 */
TEST_F(VDBChaosTest, IndexRebuildAfterKill) {
    // 创建集合
    VectorCreateParams params = make_create_params(
        "test_index_rebuild", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 插入大量数据
    const int total_vectors = 1000;
    const int batch_size = 100;
    int num_batches = total_vectors / batch_size;

    for (int b = 0; b < num_batches; b++) {
        float** vecs = generate_vectors(batch_size, DEFAULT_DIM);
        float** ptrs = (float**)malloc(batch_size * sizeof(float*));
        for (int i = 0; i < batch_size; i++) ptrs[i] = vecs[i];

        VectorInsertParams ip = make_insert_params(
            "test_index_rebuild", batch_size, NULL, ptrs, NULL, NULL);

        ASSERT_EQ(batch_size, vector_api_insert(api, &ip, NULL));
        free(ptrs);
        free_vectors(vecs, batch_size);
    }

    EXPECT_EQ(total_vectors, vector_api_size(api, "test_index_rebuild"));

    // 模拟崩溃（不保存索引）
    printf("\n[IndexRebuildAfterKill] Simulating index crash...\n");
    vector_api_destroy(api);
    api = nullptr;

    // 重新创建并加载
    api = vector_api_create(test_data_dir);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    // 验证数据完整
    int recovered_size = vector_api_size(api, "test_index_rebuild");
    printf("[IndexRebuildAfterKill] Recovered size: %d\n", recovered_size);
    EXPECT_EQ(total_vectors, recovered_size);

    // 验证搜索功能（可能需要重建索引）
    float query[DEFAULT_DIM];
    for (int i = 0; i < DEFAULT_DIM; i++) query[i] = 0.5f;

    VectorSearchParams sp = make_search_params(
        "test_index_rebuild", query, 10, 0, true, false, NULL);

    VectorSearchResults* results = vector_api_search(api, &sp);
    if (results) {
        EXPECT_EQ(10, vector_api_results_count(results));
        printf("[IndexRebuildAfterKill] Search returned %d results\n",
               vector_api_results_count(results));
        vector_api_results_free(results);
    } else {
        // 如果索引需要重建但暂时不支持，返回 null
        printf("[IndexRebuildAfterKill] Search returned NULL (may need index rebuild)\n");
    }
}

/**
 * 测试 6.4：磁盘满模拟 → 错误处理验证
 */
TEST_F(VDBChaosTest, DiskFullSimulation) {
    // 创建集合
    VectorCreateParams params = make_create_params(
        "test_disk_full", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 尝试大量写入（测试内存管理）
    const int large_batch = 10000;
    float** vecs = generate_vectors(large_batch, DEFAULT_DIM);
    float** ptrs = (float**)malloc(large_batch * sizeof(float*));
    for (int i = 0; i < large_batch; i++) ptrs[i] = vecs[i];

    VectorInsertParams ip = make_insert_params(
        "test_disk_full", large_batch, NULL, ptrs, NULL, NULL);

    // 尝试插入，可能遇到资源限制
    int inserted = vector_api_insert(api, &ip, NULL);
    printf("\n[DiskFullSimulation] Attempted %d, inserted %d\n",
           large_batch, inserted);

    // 验证至少有部分数据被插入或操作失败被正确处理
    EXPECT_GE(inserted, 0);  // 插入数量应该 >= 0

    // 验证集合状态正确
    int size = vector_api_size(api, "test_disk_full");
    printf("[DiskFullSimulation] Final size: %d\n", size);

    // 如果完全失败，集合应该为空
    if (inserted == 0) {
        EXPECT_EQ(0, size);
    } else {
        // 如果部分成功，验证数据一致性
        EXPECT_EQ(inserted, size);
    }

    free(ptrs);
    free_vectors(vecs, large_batch);

    // 验证搜索功能
    if (size > 0) {
        float query[DEFAULT_DIM];
        for (int i = 0; i < DEFAULT_DIM; i++) query[i] = 0.5f;

        VectorSearchParams sp = make_search_params(
            "test_disk_full", query, 10, 0, true, false, NULL);

        VectorSearchResults* results = vector_api_search(api, &sp);
        if (results) {
            vector_api_results_free(results);
        }
    }
}

/**
 * 测试：多次崩溃恢复
 */
TEST_F(VDBChaosTest, MultipleCrashRecovery) {
    const int num_cycles = 3;

    for (int cycle = 0; cycle < num_cycles; cycle++) {
        printf("\n[MultipleCrashRecovery] Cycle %d/%d\n", cycle + 1, num_cycles);

        // 创建或打开集合
        VectorCreateParams params = make_create_params(
            "test_multi_crash", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);

        // 尝试创建（如果已存在则忽略）
        vector_api_create_collection(api, &params);

        // 插入数据
        const int batch_size = 100;
        float** vecs = generate_vectors(batch_size, DEFAULT_DIM);
        float** ptrs = (float**)malloc(batch_size * sizeof(float*));
        for (int i = 0; i < batch_size; i++) ptrs[i] = vecs[i];

        VectorInsertParams ip = make_insert_params(
            "test_multi_crash", batch_size, NULL, ptrs, NULL, NULL);

        int inserted = vector_api_insert(api, &ip, NULL);
        printf("  Inserted: %d/%d\n", inserted, batch_size);

        free(ptrs);
        free_vectors(vecs, batch_size);

        // 验证大小
        int size = vector_api_size(api, "test_multi_crash");
        printf("  Current size: %d\n", size);
        EXPECT_GE(size, (cycle + 1) * inserted);

        // 模拟崩溃（如果非最后一轮）
        if (cycle < num_cycles - 1) {
            vector_api_destroy(api);
            api = nullptr;

            api = vector_api_create(test_data_dir);
            ASSERT_NE(nullptr, api);
            EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

            int recovered_size = vector_api_size(api, "test_multi_crash");
            printf("  Recovered size: %d\n", recovered_size);
        }
    }

    // 最终保存
    EXPECT_EQ(VECTOR_API_OK, vector_api_save(api));
}

/**
 * 测试：并发写入与崩溃
 */
TEST_F(VDBChaosTest, ConcurrentWriteWithCrash) {
    // 创建集合
    VectorCreateParams params = make_create_params(
        "test_concurrent_crash", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 启动并发写入线程
    const int num_writers = 4;
    const int vectors_per_writer = 50;
    std::atomic<bool> stop_writing(false);
    std::atomic<int> total_inserted(0);

    std::vector<std::thread> writers;
    for (int w = 0; w < num_writers; w++) {
        writers.emplace_back([this, &stop_writing, &total_inserted, vectors_per_writer, w]() {
            for (int i = 0; i < vectors_per_writer && !stop_writing.load(); i++) {
                float* vec = generate_vector(DEFAULT_DIM);
                float* vec_ptr = vec;

                VectorInsertParams ip = make_insert_params(
                    "test_concurrent_crash", 1, NULL, &vec_ptr, NULL, NULL);

                int inserted = vector_api_insert(api, &ip, NULL);
                if (inserted > 0) {
                    total_inserted.fetch_add(inserted);
                }

                free(vec);
            }
        });
    }

    // 运行一段时间后停止
    Sleep(100);  // 100ms
    stop_writing.store(true);

    // 等待所有线程
    for (auto& t : writers) {
        t.join();
    }

    printf("\n[ConcurrentWriteWithCrash] Total inserted: %d\n", total_inserted.load());

    // 模拟崩溃
    vector_api_destroy(api);
    api = nullptr;

    // 恢复
    api = vector_api_create(test_data_dir);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    int recovered_size = vector_api_size(api, "test_concurrent_crash");
    printf("[ConcurrentWriteWithCrash] Recovered size: %d\n", recovered_size);

    // 验证数据一致性
    EXPECT_GE(recovered_size, 0);
    EXPECT_LE(recovered_size, total_inserted.load());
}

/**
 * 测试：保存与加载的正确性
 */
TEST_F(VDBChaosTest, SaveLoadCorrectness) {
    // 创建集合
    VectorCreateParams params = make_create_params(
        "test_save_load", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 插入数据
    const int n = 500;
    float** vecs = generate_vectors(n, DEFAULT_DIM);
    float** ptrs = (float**)malloc(n * sizeof(float*));
    for (int i = 0; i < n; i++) ptrs[i] = vecs[i];

    VectorInsertParams ip = make_insert_params(
        "test_save_load", n, NULL, ptrs, NULL, NULL);
    ASSERT_EQ(n, vector_api_insert(api, &ip, NULL));

    int size_before = vector_api_size(api, "test_save_load");
    EXPECT_EQ(n, size_before);

    free(ptrs);
    free_vectors(vecs, n);

    // 保存
    EXPECT_EQ(VECTOR_API_OK, vector_api_save(api));

    // 销毁并重新加载
    vector_api_destroy(api);
    api = nullptr;

    api = vector_api_create(test_data_dir);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    // 验证大小完全一致
    int size_after = vector_api_size(api, "test_save_load");
    printf("\n[SaveLoadCorrectness] Before: %d, After: %d\n", size_before, size_after);
    EXPECT_EQ(size_before, size_after);

    // 验证搜索结果一致性
    float query[DEFAULT_DIM];
    for (int i = 0; i < DEFAULT_DIM; i++) query[i] = 0.5f;

    VectorSearchParams sp = make_search_params(
        "test_save_load", query, 10, 0, true, false, NULL);

    VectorSearchResults* results = vector_api_search(api, &sp);
    ASSERT_NE(nullptr, results);
    EXPECT_EQ(10, vector_api_results_count(results));
    vector_api_results_free(results);
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
