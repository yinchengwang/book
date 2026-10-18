/**
 * @file vdb_checkpoint_test.cpp
 * @brief Vector CheckpointManager 独立单元测试（P1-B）
 *
 * 与 vdb_wal_recovery_test.cpp 风格保持一致，便于在 TDD 循环中独立运行。
 *
 * 测试覆盖 5 个场景：
 *   1. CheckpointTriggeredAfterNRecords — 批量插入触发 checkpoint
 *   2. CheckpointLsn起点正确          — checkpoint 后 LSN 起点正确推进
 *   3. TruncateAfterCheckpoint        — checkpoint 后 WAL 文件被截断
 *   4. IdempotentInsert               — 重复 load 同 snapshot 不重复插入
 *   5. ConcurrentInsertWithCheckpoint — 并发插入 + 后台 CheckpointManager
 *
 * 注：本文件为 P1-B 任务 3（TDD 红阶段）的占位测试。
 *     当前 vector_api 尚未集成 vector_wal，CheckpointManager 也未实现，
 *     因此本组测试在编译通过后将全部 FAIL，给后续 Task 4-7 提供"转绿"目标。
 */
#include <gtest/gtest.h>
#include <db/api/vector_api.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define getpid _getpid
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

class VDBCheckpointTest : public ::testing::Test {
protected:
    std::string test_data_dir;
    VectorAPI *api = nullptr;

    void SetUp() override {
        // 每个用例独立 tmp 目录（与 P1-A vdb_wal_recovery_test 一致）
        char buf[256];
        std::snprintf(buf, sizeof(buf), "/tmp/vdb_checkpoint_test_%d", getpid());
        test_data_dir = buf;
        std::string cmd = "rm -rf " + test_data_dir + " && mkdir -p " + test_data_dir;
        std::system(cmd.c_str());
    }

    void TearDown() override {
        if (api) { vector_api_destroy(api); api = nullptr; }
        std::string cmd = "rm -rf " + test_data_dir;
        std::system(cmd.c_str());
    }

    // 构造 helper（与 P1-A 风格对齐）
    static VectorCreateParams make_create_params(const char* name, int32_t dim) {
        VectorCreateParams p;
        p.name = name;
        p.dimension = dim;
        p.index_type = VECTOR_INDEX_HNSW;
        p.metric_type = VECTOR_METRIC_L2;
        p.index_ef_search = 100;
        p.index_m = 16;
        p.index_ef_construction = 200;
        return p;
    }

    static VectorInsertParams make_insert_params(const char* coll, int32_t n,
                                                  float** vecs) {
        VectorInsertParams p;
        p.collection = coll;
        p.n = n;
        p.ids = NULL;        // 让 coll->next_id++ 自动分配
        p.vectors = vecs;
        p.metadata = NULL;
        p.metadata_sizes = NULL;
        return p;
    }

    // 固定种子生成确定性向量
    static float** generate_vectors(int32_t n, int32_t dim, uint32_t seed = 42) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float** v = (float**)calloc(n, sizeof(float*));
        for (int i = 0; i < n; i++) {
            v[i] = (float*)calloc(dim, sizeof(float));
            for (int j = 0; j < dim; j++) v[i][j] = dist(rng);
        }
        return v;
    }

    static void free_vectors(float** v, int32_t n) {
        if (!v) return;
        for (int i = 0; i < n; i++) free(v[i]);
        free(v);
    }

    // 查找目录下所有 .wal 文件，返回总字节数（找不到文件返回 0）
    static uint64_t total_wal_file_size(const std::string& dir) {
        uint64_t total = 0;
        if (!fs::exists(dir)) return 0;
        for (auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            const std::string name = entry.path().filename().string();
            if (name.size() >= 4 &&
                name.compare(name.size() - 4, 4, ".wal") == 0) {
                total += static_cast<uint64_t>(
                    fs::file_size(entry.path()));
            }
        }
        return total;
    }

    // 查找目录下是否存在 .wal 文件
    static bool has_any_wal_file(const std::string& dir) {
        if (!fs::exists(dir)) return false;
        for (auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            const std::string name = entry.path().filename().string();
            if (name.size() >= 4 &&
                name.compare(name.size() - 4, 4, ".wal") == 0) {
                return true;
            }
        }
        return false;
    }
};

// ============================================================
// 测试 1: 批量插入触发 checkpoint
// ============================================================
TEST_F(VDBCheckpointTest, CheckpointTriggeredAfterNRecords) {
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = make_create_params("ckpt_trigger", 128);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 插 1000 条：触发 CheckpointManager 自动 checkpoint 阈值（默认 N=500）
    float** vecs = generate_vectors(1000, 128, 1001);
    VectorInsertParams ip = make_insert_params("ckpt_trigger", 1000, vecs);
    ASSERT_EQ(1000, vector_api_insert(api, &ip, NULL));
    free_vectors(vecs, 1000);

    // 触发 save：此时 CheckpointManager 应已记录 checkpoint_lsn
    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));

    // 期望 1：数据完整保存（间接验证 checkpoint 没丢数据）
    vector_api_destroy(api);
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);
    ASSERT_EQ(VECTOR_API_OK, vector_api_load(api));
    EXPECT_EQ(1000, vector_api_size(api, "ckpt_trigger"));

    // 期望 2：WAL 文件应在 checkpoint 之后存在
    //         （vector_api 集成 vector_wal 后，P1-B Task 4-6 会真正写入 .wal）
    //         当前阶段此断言必然 FAIL（红）。
    EXPECT_TRUE(has_any_wal_file(test_data_dir));
}

// ============================================================
// 测试 2: Checkpoint LSN 起点正确（先 100 → checkpoint → 100 → 重启 = 200）
// ============================================================
TEST_F(VDBCheckpointTest, CheckpointLsn起点正确) {
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = make_create_params("ckpt_lsn", 128);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 阶段 1：插 100 条 → save（含 checkpoint 触发）
    float** vecs1 = generate_vectors(100, 128, 2001);
    VectorInsertParams ip = make_insert_params("ckpt_lsn", 100, vecs1);
    ASSERT_EQ(100, vector_api_insert(api, &ip, NULL));
    free_vectors(vecs1, 100);
    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));

    // 阶段 2：再插 100 条（不 save，模拟 checkpoint 后继续写）
    float** vecs2 = generate_vectors(100, 128, 2002);
    ip.n = 100;
    ip.vectors = vecs2;
    ASSERT_EQ(100, vector_api_insert(api, &ip, NULL));
    free_vectors(vecs2, 100);

    // 模拟"崩溃"：不 save 直接销毁
    vector_api_destroy(api);
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);
    ASSERT_EQ(VECTOR_API_OK, vector_api_load(api));

    // 期望：从 checkpoint_lsn 重放后 = 200（100 checkpoint + 100 WAL）
    //  当前 vector_api 无 WAL 重放 → 期望 = 100，红
    EXPECT_EQ(200, vector_api_size(api, "ckpt_lsn"));
}

// ============================================================
// 测试 3: checkpoint 后 truncate → WAL 文件显著减小
// ============================================================
TEST_F(VDBCheckpointTest, TruncateAfterCheckpoint) {
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = make_create_params("ckpt_truncate", 128);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 插 1000 条（产生足够大的 WAL 体量以便观察到 truncate 效果）
    float** vecs = generate_vectors(1000, 128, 3001);
    VectorInsertParams ip = make_insert_params("ckpt_truncate", 1000, vecs);
    ASSERT_EQ(1000, vector_api_insert(api, &ip, NULL));
    free_vectors(vecs, 1000);

    // 触发 save（隐含 checkpoint + truncate）
    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));

    // 期望：truncate 后 WAL 文件大小应小于头部大小（仅留 checkpoint 标记）
    //  当前 vector_api 尚未集成 WAL → 无 .wal 文件 → size = 0
    //  但即便有 WAL，checkpoint+truncate 后也应 < 原始大小
    uint64_t wal_size = total_wal_file_size(test_data_dir);
    EXPECT_GT(wal_size, 0u);          // 期望 WAL 存在
    EXPECT_LT(wal_size, 1024u * 100); // 期望 truncate 后 < 100KB
}

// ============================================================
// 测试 4: 重复 load 同 snapshot → 幂等（size 不变）
// ============================================================
TEST_F(VDBCheckpointTest, IdempotentInsert) {
    // 阶段 1：建立 100 条 snapshot 并 save
    {
        api = vector_api_create(test_data_dir.c_str());
        ASSERT_NE(nullptr, api);

        VectorCreateParams params = make_create_params("ckpt_idempotent", 128);
        ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

        float** vecs = generate_vectors(100, 128, 4001);
        VectorInsertParams ip = make_insert_params("ckpt_idempotent", 100, vecs);
        ASSERT_EQ(100, vector_api_insert(api, &ip, NULL));
        free_vectors(vecs, 100);
        ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));

        vector_api_destroy(api);
        api = nullptr;
    }

    // 阶段 2：第一次 load → 100 条
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);
    ASSERT_EQ(VECTOR_API_OK, vector_api_load(api));
    EXPECT_EQ(100, vector_api_size(api, "ckpt_idempotent"));
    vector_api_destroy(api);
    api = nullptr;

    // 阶段 3：再次 load（idempotent insert 路径）
    //  当前 vector_api 无 WAL 重放 → size = 0，红
    //  后续 Task 6 实现 idempotent replay 后应保持 = 100
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);
    ASSERT_EQ(VECTOR_API_OK, vector_api_load(api));
    EXPECT_EQ(100, vector_api_size(api, "ckpt_idempotent"));
    // 期望：WAL 文件存在（P1-B Task 4-6 集成 WAL 后此断言转绿）
    EXPECT_TRUE(has_any_wal_file(test_data_dir));
}

// ============================================================
// 测试 5: 并发插入 + 后台 CheckpointManager 触发 → 无丢失无重复
// ============================================================
TEST_F(VDBCheckpointTest, ConcurrentInsertWithCheckpoint) {
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);

    VectorCreateParams params = make_create_params("ckpt_concurrent", 128);
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &params));

    // 2 个线程各插 500 条（独立种子以避免确定性重合）
    constexpr int32_t PER_THREAD = 500;
    std::atomic<int32_t> total_inserted{0};

    auto worker = [&](uint32_t seed) {
        float** vecs = generate_vectors(PER_THREAD, 128, seed);
        VectorInsertParams ip = make_insert_params("ckpt_concurrent",
                                                   PER_THREAD, vecs);
        int n = vector_api_insert(api, &ip, NULL);
        total_inserted.fetch_add(n);
        free_vectors(vecs, PER_THREAD);
    };

    std::thread t1(worker, 5001);
    std::thread t2(worker, 5002);
    t1.join();
    t2.join();

    // 期望：总插入成功 = 1000
    EXPECT_EQ(1000, total_inserted.load());

    // save 触发后台 CheckpointManager
    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));

    // 重启验证：无丢失无重复
    vector_api_destroy(api);
    api = vector_api_create(test_data_dir.c_str());
    ASSERT_NE(nullptr, api);
    ASSERT_EQ(VECTOR_API_OK, vector_api_load(api));

    // 兜底断言：至少 1000（即便 Task 7 尚未实现 CheckpointManager 也不会假阴性报错）
    // 严格断言：= 1000（P1-B 目标无丢失无重复）
    int32_t size = vector_api_size(api, "ckpt_concurrent");
    EXPECT_GE(size, 1000);
    EXPECT_EQ(1000, size);
    // 期望：WAL 文件存在（CheckpointManager 触发后必有 WAL 落盘）
    EXPECT_TRUE(has_any_wal_file(test_data_dir));
}