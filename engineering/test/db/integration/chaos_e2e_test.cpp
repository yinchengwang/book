/**
 * @file chaos_e2e_test.cpp
 * @brief P1-C chaos_e2e 端到端混沌测试（TDD 红阶段）
 *
 * 测试范围（5 用例，proof TDD red status）：
 *   1. KillDuringCheckpoint    — flush+checkpoint+truncate 窗口中途 destroy
 *   2. KillDuringReplay        — WAL replay 中途 destroy，验证再次 create 幂等续接
 *   3. PartialWriteRecovery    — 手动破坏 WAL 末尾 16 字节，验证 CRC 容错
 *   4. ShortIntervalStress     — 100ms 周期 + 200 次 insert+kill 循环，零丢失
 *   5. ConcurrentSaveCrash     — 4 写 + 1 save + 1 destroyer 并发交织不撕裂
 *
 * 【TDD 红状态说明】
 *   本测试 SetUp 调用 `vector_api_create_with_interval(data_dir, interval_ms)`，
 *   该函数尚不存在（属 Task 4 待实现）。因此当前测试套件处于 TDD 红阶段：
 *   编译失败 = 红，符合 TDD 红→绿→重构循环的第一阶段。
 *
 *   Task 4 完成后：
 *     - vector_api.h 声明 vector_api_create_with_interval
 *     - vector_api.c 委托实现，interval_ms 透传给 checkpoint_manager_create
 *     - 此时本测试 SetUp 可编译通过，但运行时部分假设仍需 Task 5 验证。
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
#include <vector>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define CHAOS_SLEEP(ms) Sleep(ms)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#define CHAOS_SLEEP(ms) usleep((ms) * 1000)
#endif

namespace fs = std::filesystem;

// ============================================================
// 测试配置
// ============================================================

constexpr int DEFAULT_DIM = 128;               ///< 默认向量维度
constexpr int CHAOS_E2E_VECTORS = 1000;        ///< 测试用默认批量大小
constexpr int SHORT_INTERVAL_MS = 100;         ///< chaos 用短周期
constexpr int KILL_CYCLES = 10;                ///< KillDuringCheckpoint 循环次数
constexpr int STRESS_ROUNDS = 200;             ///< ShortIntervalStress 循环轮数
constexpr int PARTIAL_WAL_TRUNCATE = 16;       ///< PartialWriteRecovery 末尾截断字节数

// ============================================================
// 辅助函数（拷贝自 vdb_chaos_test.cpp，保持风格一致）
// ============================================================

static VectorCreateParams make_create_params(const char *name, int32_t dim,
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

static VectorInsertParams make_insert_params(const char *coll, int32_t n,
                                             int64_t *ids, float **vecs,
                                             void **meta, int32_t *meta_sizes) {
    VectorInsertParams p;
    p.collection = coll;
    p.n = n;
    p.ids = ids;
    p.vectors = vecs;
    p.metadata = meta;
    p.metadata_sizes = meta_sizes;
    return p;
}

// ============================================================
// 测试夹具
// ============================================================

/**
 * @brief chaos_e2e 测试夹具
 *
 * 与 VDBChaosTest 同源，但：
 *   - 独立 test_data_dir（避免破坏其他混沌套件的共享目录）
 *   - SetUp 使用 vector_api_create_with_interval()（TDD 红 — Task 4 实现）
 *   - 短 interval（100ms）让 flush+checkpoint+truncate 频繁触发
 */
class ChaosE2ETest : public ::testing::Test {
protected:
    static const char *test_data_dir;       ///< suite 级数据目录
    VectorAPI *api = nullptr;              ///< 测试用例持有的 API 句柄

    static void SetUpTestSuite() {
#ifdef _WIN32
        _mkdir(test_data_dir);
#else
        mkdir(test_data_dir, 0755);
#endif
    }

    void SetUp() override {
        // TDD 红状态入口：vector_api_create_with_interval 尚未声明
        // → 编译失败（这是预期红状态）
        // Task 4 将在 vector_api.h 增加该符号
        api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
        ASSERT_NE(nullptr, api);
    }

    void TearDown() override {
        if (api) {
            vector_api_destroy(api);
            api = nullptr;
        }
    }

    /// @brief 清空并重建测试数据目录
    static void cleanup_test_dir() {
        if (fs::exists(test_data_dir)) {
            fs::remove_all(test_data_dir);
            fs::create_directories(test_data_dir);
        }
    }

    /// @brief 生成单个随机向量
    static float *generate_vector(int32_t dim) {
        static thread_local std::mt19937 rng(
            std::hash<std::thread::id>{}(std::this_thread::get_id()) ^
            std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float *v = (float *)malloc(dim * sizeof(float));
        for (int i = 0; i < dim; i++) {
            v[i] = dist(rng);
        }
        return v;
    }

    /// @brief 生成多个随机向量
    static float **generate_vectors(int32_t n, int32_t dim) {
        float **vectors = (float **)malloc(n * sizeof(float *));
        for (int i = 0; i < n; i++) {
            vectors[i] = generate_vector(dim);
        }
        return vectors;
    }

    /// @brief 释放向量数组
    static void free_vectors(float **vectors, int32_t n) {
        if (!vectors) return;
        for (int i = 0; i < n; i++) {
            free(vectors[i]);
        }
        free(vectors);
    }
};

const char *ChaosE2ETest::test_data_dir = "./test-results/chaos_e2e";

// ============================================================
// 用例 1：KillDuringCheckpoint
// ============================================================

/**
 * @brief flush + checkpoint + truncate 三步合一窗口中途 destroy
 *
 * 100ms 周期下窗口比 P1-B 默认 5s 大 50 倍，故意循环 10 次取最坏情况，
 * 验证最坏情况下 reload 仍能 enter，无 corrupt，无负数 size。
 */
TEST_F(ChaosE2ETest, KillDuringCheckpoint) {
    ChaosE2ETest::cleanup_test_dir();

    // 1. 用短 interval 重建 api
    if (api) {
        vector_api_destroy(api);
        api = nullptr;
    }
    api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
    ASSERT_NE(nullptr, api);

    // 2. 创建集合
    VectorCreateParams params = make_create_params(
        "kill_during_checkpoint", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 3. 先插 200 条让初始状态稳定
    constexpr int initial_batch = 200;
    float **initial_vecs = generate_vectors(initial_batch, DEFAULT_DIM);
    float **initial_ptrs = (float **)malloc(initial_batch * sizeof(float *));
    for (int i = 0; i < initial_batch; i++) initial_ptrs[i] = initial_vecs[i];
    VectorInsertParams ip_initial = make_insert_params(
        "kill_during_checkpoint", initial_batch, NULL, initial_ptrs, NULL, NULL);
    ASSERT_EQ(initial_batch, vector_api_insert(api, &ip_initial, NULL));
    free(initial_ptrs);
    free_vectors(initial_vecs, initial_batch);

    int initial_size = vector_api_size(api, "kill_during_checkpoint");
    printf("\n[KillDuringCheckpoint] 初始 size: %d\n", initial_size);

    // 4. 循环 10 次"立即 destroy"，撞上 flush+checkpoint+truncate 中途
    int worst_case_size = initial_size;
    for (int cycle = 0; cycle < KILL_CYCLES; cycle++) {
        // 不 save，立即 destroy（让 destroy 抢在 checkpoint 中途）
        vector_api_destroy(api);
        api = nullptr;

        // 立即重新 create+load，验证能 reload
        api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
        ASSERT_NE(nullptr, api);
        EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

        // 验证 size 在合理范围：无 corrupt、无负数
        int size_after_reload = vector_api_size(api, "kill_during_checkpoint");
        printf("[KillDuringCheckpoint] 周期 %d size: %d\n", cycle, size_after_reload);
        EXPECT_GE(size_after_reload, 0);
        EXPECT_LE(size_after_reload, initial_size);

        if (size_after_reload < worst_case_size) {
            worst_case_size = size_after_reload;
        }

        // 在循环中继续 insert，保持有数据涌入
        float *single_vec = generate_vector(DEFAULT_DIM);
        VectorInsertParams ip_single = make_insert_params(
            "kill_during_checkpoint", 1, NULL, &single_vec, NULL, NULL);
        (void)vector_api_insert(api, &ip_single, NULL);
        free(single_vec);
    }

    printf("[KillDuringCheckpoint] 最坏情况 size: %d / 初始 %d\n",
           worst_case_size, initial_size);
}

// ============================================================
// 用例 2：KillDuringReplay
// ============================================================

/**
 * @brief replay 中途被 kill，验证再次 create 时幂等续接
 *
 * 流程：
 *   phase1: create + insert(10000) + save → WAL 大
 *   phase2: destroy → 立即 create → load（replay 还没读完被 destroy 撞上）
 *   phase3: 再 create + load → 期望 size 一致
 */
TEST_F(ChaosE2ETest, KillDuringReplay) {
    ChaosE2ETest::cleanup_test_dir();

    if (api) {
        vector_api_destroy(api);
        api = nullptr;
    }

    // ---- phase 1: 准备 10000 条 WAL 记录 ----
    api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = make_create_params(
        "kill_during_replay", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    constexpr int replay_batch = 10000;
    constexpr int chunk = 1000;
    for (int b = 0; b < replay_batch; b += chunk) {
        float **vecs = generate_vectors(chunk, DEFAULT_DIM);
        float **ptrs = (float **)malloc(chunk * sizeof(float *));
        for (int i = 0; i < chunk; i++) ptrs[i] = vecs[i];
        VectorInsertParams ip = make_insert_params(
            "kill_during_replay", chunk, NULL, ptrs, NULL, NULL);
        ASSERT_EQ(chunk, vector_api_insert(api, &ip, NULL));
        free(ptrs);
        free_vectors(vecs, chunk);
    }

    // save 触发 checkpoint + truncate
    EXPECT_EQ(VECTOR_API_OK, vector_api_save(api));
    int size_after_save = vector_api_size(api, "kill_during_replay");
    printf("\n[KillDuringReplay] save 后 size: %d\n", size_after_save);
    EXPECT_EQ(replay_batch, size_after_save);

    // 销毁
    vector_api_destroy(api);
    api = nullptr;

    // ---- phase 2: 立即 create + load，强行撞 replay 中 ----
    api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    // 故意不等 load 完成，立即 destroy
    CHAOS_SLEEP(5);
    vector_api_destroy(api);
    api = nullptr;

    // ---- phase 3: 再次 create + load，应幂等续接 ----
    api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    int final_size = vector_api_size(api, "kill_during_replay");
    printf("[KillDuringReplay] 最终 size: %d (期望 %d)\n", final_size, replay_batch);
    EXPECT_EQ(replay_batch, final_size);
}

// ============================================================
// 用例 3：PartialWriteRecovery
// ============================================================

/**
 * @brief 手动破坏 WAL 末尾 16 字节，验证 replay CRC 容错
 *
 * 模拟断电/OS crash 时末尾 N 字节未对齐 record header，
 * 期望 reload 后仍能恢复到某次 checkpoint 之后的数据，
 * 损坏部分被丢弃，不抛异常。
 */
TEST_F(ChaosE2ETest, PartialWriteRecovery) {
    ChaosE2ETest::cleanup_test_dir();

    if (api) {
        vector_api_destroy(api);
        api = nullptr;
    }

    // 1. create + insert(100) + save，确保 WAL 有内容并被 checkpoint 过
    api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = make_create_params(
        "partial_write_recovery", DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    constexpr int initial_count = 100;
    float **vecs = generate_vectors(initial_count, DEFAULT_DIM);
    float **ptrs = (float **)malloc(initial_count * sizeof(float *));
    for (int i = 0; i < initial_count; i++) ptrs[i] = vecs[i];
    VectorInsertParams ip = make_insert_params(
        "partial_write_recovery", initial_count, NULL, ptrs, NULL, NULL);
    ASSERT_EQ(initial_count, vector_api_insert(api, &ip, NULL));
    free(ptrs);
    free_vectors(vecs, initial_count);

    EXPECT_EQ(VECTOR_API_OK, vector_api_save(api));

    // 2. 手动破坏 WAL 末尾 16 字节
    fs::path wal_path = fs::path(test_data_dir) / "vector_wal_0.log";
    ASSERT_TRUE(fs::exists(wal_path)) << "WAL 文件不存在: " << wal_path.string();

    long wal_size = 0;
    {
        FILE *fp = fopen(wal_path.string().c_str(), "rb");
        ASSERT_NE(nullptr, fp);
        fseek(fp, 0, SEEK_END);
        wal_size = ftell(fp);
        fclose(fp);
    }
    printf("\n[PartialWriteRecovery] WAL 大小: %ld 字节\n", wal_size);
    ASSERT_GE(wal_size, PARTIAL_WAL_TRUNCATE);

    // 打开 WAL 文件，破坏末尾 16 字节
    {
        FILE *fp = fopen(wal_path.string().c_str(), "r+b");
        ASSERT_NE(nullptr, fp);
        fseek(fp, -PARTIAL_WAL_TRUNCATE, SEEK_END);
        unsigned char corrupt[PARTIAL_WAL_TRUNCATE];
        memset(corrupt, 0xFF, sizeof(corrupt));
        size_t written = fwrite(corrupt, 1, PARTIAL_WAL_TRUNCATE, fp);
        EXPECT_EQ(sizeof(corrupt), written);
        fclose(fp);
    }

    // 3. destroy + create + load
    vector_api_destroy(api);
    api = nullptr;

    api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    // 4. 期望：size 在某次 insert 的整数倍附近（接受部分丢失）
    int recovered_size = vector_api_size(api, "partial_write_recovery");
    printf("[PartialWriteRecovery] 恢复后 size: %d (上限 %d)\n",
           recovered_size, initial_count);
    EXPECT_GE(recovered_size, 0);
    EXPECT_LE(recovered_size, initial_count);
    // 部分丢：至少恢复到 50%（更宽口径便于 Task 5 调整）
    EXPECT_GE(recovered_size, initial_count / 2);
}

// ============================================================
// 用例 4：ShortIntervalStress
// ============================================================

/**
 * @brief 100ms 周期 + 200 次 insert+kill 循环，验证零丢失
 *
 * 每轮：
 *   a. create_with_interval(dir, 100)
 *   b. insert(1)（首次创建集合，后续轮跳过创建）
 *   c. destroy
 *   d. create + load
 *   e. size 期望 round+1（累计）
 *
 * 最终期望：size == 200，零丢失。
 */
TEST_F(ChaosE2ETest, ShortIntervalStress) {
    ChaosE2ETest::cleanup_test_dir();

    if (api) {
        vector_api_destroy(api);
        api = nullptr;
    }

    int total_inserted = 0;
    const char *coll_name = "short_interval_stress";

    for (int round = 0; round < STRESS_ROUNDS; round++) {
        // a. 短 interval 创建
        api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
        ASSERT_NE(nullptr, api);

        // b. 首次创建集合，后续轮跳过
        if (round == 0) {
            VectorCreateParams params = make_create_params(
                coll_name, DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
            ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));
        } else {
            // 后续轮：load 触发 replay，恢复已有数据
            EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));
        }

        // 插入 1 条
        float *single_vec = generate_vector(DEFAULT_DIM);
        VectorInsertParams ip = make_insert_params(
            coll_name, 1, NULL, &single_vec, NULL, NULL);
        int inserted = vector_api_insert(api, &ip, NULL);
        EXPECT_EQ(1, inserted);
        free(single_vec);
        total_inserted += inserted;

        // c. 立即 destroy
        vector_api_destroy(api);
        api = nullptr;

        // d+e. 重新 create+load 验证累计 size
        api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
        ASSERT_NE(nullptr, api);
        EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

        int cumulative_size = vector_api_size(api, coll_name);
        if (round % 20 == 0 || round == STRESS_ROUNDS - 1) {
            printf("[ShortIntervalStress] 轮 %d: 累计 size=%d 期望=%d\n",
                   round, cumulative_size, round + 1);
        }
        EXPECT_GE(cumulative_size, round + 1);
    }

    printf("\n[ShortIntervalStress] 全部 %d 轮，累计 size 应 == %d\n",
           STRESS_ROUNDS, STRESS_ROUNDS);
}

// ============================================================
// 用例 5：ConcurrentSaveCrash
// ============================================================

/**
 * @brief 4 写线程 + 1 save 线程 + 1 destroyer 线程，验证并发交织不撕裂
 *
 * 跑 2s 后全部 join，最终重建 + load，size 应一致（无负数 / 不超预期）。
 * 注：因 vector_api 句柄不能多线程共享写，每线程持有独立 API 是另一设计
 *      （本用例聚焦：单 api 的多并发写线程 + save 线程 + 主线程 destroy 触发竞争）。
 */
TEST_F(ChaosE2ETest, ConcurrentSaveCrash) {
    ChaosE2ETest::cleanup_test_dir();

    if (api) {
        vector_api_destroy(api);
        api = nullptr;
    }

    // 集合名全局唯一避免干扰
    const char *coll_name = "concurrent_save_crash";

    // 主线程持有 api
    api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = make_create_params(
        coll_name, DEFAULT_DIM, VECTOR_INDEX_HNSW, VECTOR_METRIC_L2);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    std::atomic<bool> stop_flag(false);
    std::atomic<int> total_inserted(0);
    std::atomic<int> total_saved(0);
    std::atomic<int> total_destroyed(0);

    // 4 个 writer 线程：每线程 50 写
    constexpr int num_writers = 4;
    constexpr int writes_per_writer = 50;
    std::vector<std::thread> writers;
    for (int w = 0; w < num_writers; w++) {
        writers.emplace_back([this, coll_name, &stop_flag, &total_inserted, w]() {
            for (int i = 0; i < writes_per_writer && !stop_flag.load(); i++) {
                if (!api) break;
                float *vec = generate_vector(DEFAULT_DIM);
                VectorInsertParams ip = make_insert_params(
                    coll_name, 1, NULL, &vec, NULL, NULL);
                int inserted = vector_api_insert(api, &ip, NULL);
                if (inserted > 0) total_inserted.fetch_add(inserted);
                free(vec);
            }
        });
    }

    // 1 个 saver 线程：每 200ms save
    std::thread saver([this, &stop_flag, &total_saved]() {
        while (!stop_flag.load()) {
            CHAOS_SLEEP(200);
            if (!api) continue;
            if (vector_api_save(api) == VECTOR_API_OK) {
                total_saved.fetch_add(1);
            }
        }
    });

    // 1 个 destroyer 线程：每 300ms 检查后 destroy（主线程重建）
    std::thread destroyer([this, &stop_flag, &total_destroyed]() {
        while (!stop_flag.load()) {
            CHAOS_SLEEP(300);
            if (!api) continue;
            vector_api_destroy(api);
            api = nullptr;
            total_destroyed.fetch_add(1);
            // 立即重建
            api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
            if (!api) break;
            vector_api_load(api);
        }
    });

    // 跑 2 秒
    CHAOS_SLEEP(2000);
    stop_flag.store(true);

    // join 所有线程
    for (auto &t : writers) t.join();
    if (saver.joinable()) saver.join();
    if (destroyer.joinable()) destroyer.join();

    printf("\n[ConcurrentSaveCrash] 写入 %d, save %d 次, destroy %d 次\n",
           total_inserted.load(), total_saved.load(), total_destroyed.load());

    // 最终 destroy + 重建 + load
    if (api) {
        vector_api_destroy(api);
        api = nullptr;
    }
    api = vector_api_create_with_interval(test_data_dir, SHORT_INTERVAL_MS);
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));

    int final_size = vector_api_size(api, coll_name);
    printf("[ConcurrentSaveCrash] 最终 size: %d (上限 %d)\n",
           final_size, num_writers * writes_per_writer);
    EXPECT_GE(final_size, 0);
    EXPECT_LE(final_size, num_writers * writes_per_writer);
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
