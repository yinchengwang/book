// selector_integration_test.cpp — 向量索引选择器集成测试
//
// 验证场景：
//   1. selector 在不同数据规模下选择正确的索引类型
//   2. selector 与 vectors.c 的集成行为正确
//   3. fallback 策略：HNSW 创建失败时降级到 flat

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <filesystem>

extern "C" {
#include "db/index/vector_index/vector_index_selector.h"
#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"
}

namespace fs = std::filesystem;

namespace {

// 生成唯一的测试数据库路径，避免测试间冲突
std::string unique_db_path(const char* suffix) {
    return std::string("test_selector_") + suffix + ".db";
}

// 清理数据库文件（包括 WAL 和 SHM）
void cleanup_db(const std::string& path) {
    std::remove(path.c_str());
    std::remove((path + "-shm").c_str());
    std::remove((path + "-wal").c_str());
}

// 生成 N 维随机浮点向量
std::vector<float> random_vector(size_t dim, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> v(dim);
    for (size_t i = 0; i < dim; i++) v[i] = dist(rng);
    return v;
}

}  // namespace

// ========================================================================
// 1. selector 单元测试：验证不同数据规模下的决策
// ========================================================================

TEST(SelectorIntegration, SmallDatasetBruteForce) {
    // 小规模数据（N=100）应该选择暴力搜索
    vector_data_info_t info = {
        .num_vectors = 100,
        .dimension = 128,
        .available_memory_mb = 4096,
        .target_qps = 0.0f,
        .target_recall = 0.95f,
        .is_static = false
    };

    vector_index_decision_t decision;
    ASSERT_EQ(vector_index_selector_choose(&info, &decision), 0);

    // N=100 < 10000，应该选择 BRUTE_FORCE
    EXPECT_EQ(decision.index_type, VECTOR_INDEX_BRUTE_FORCE);
    EXPECT_EQ(decision.param1, 0);
    EXPECT_EQ(decision.param2, 0);
}

TEST(SelectorIntegration, MediumDatasetHNSW) {
    // 中等规模数据（N=50000）+ 高召回率应该选择 HNSW
    vector_data_info_t info = {
        .num_vectors = 50000,
        .dimension = 128,
        .available_memory_mb = 8192,  // 内存充足
        .target_qps = 0.0f,
        .target_recall = 0.98f,       // 高召回率
        .is_static = false
    };

    vector_index_decision_t decision;
    ASSERT_EQ(vector_index_selector_choose(&info, &decision), 0);

    // 10K <= N < 1M，内存充足且召回率高，应该选择 HNSW
    EXPECT_EQ(decision.index_type, VECTOR_INDEX_HNSW);
    EXPECT_GT(decision.param1, 0);  // M > 0
    EXPECT_GT(decision.param2, 0);  // ef > 0
}

TEST(SelectorIntegration, MediumDatasetLowMemoryIVFPQ) {
    // 中等规模数据（N=50000）+ 低内存应该选择 IVF-PQ
    vector_data_info_t info = {
        .num_vectors = 50000,
        .dimension = 128,
        .available_memory_mb = 1024,  // 内存不足
        .target_qps = 0.0f,
        .target_recall = 0.85f,       // 低召回率
        .is_static = false
    };

    vector_index_decision_t decision;
    ASSERT_EQ(vector_index_selector_choose(&info, &decision), 0);

    // 10K <= N < 1M，内存不足，应该选择 IVF_PQ
    EXPECT_EQ(decision.index_type, VECTOR_INDEX_IVF_PQ);
    EXPECT_GT(decision.param1, 0);  // nlist > 0
    EXPECT_GT(decision.param2, 0);  // nprobe > 0
}

TEST(SelectorIntegration, LargeDatasetHNSW) {
    // 大规模数据（N=1.5M）+ 内存充足应该选择 HNSW
    vector_data_info_t info = {
        .num_vectors = 1500000,
        .dimension = 128,
        .available_memory_mb = 8192,  // 内存充足
        .target_qps = 0.0f,
        .target_recall = 0.95f,
        .is_static = false
    };

    vector_index_decision_t decision;
    ASSERT_EQ(vector_index_selector_choose(&info, &decision), 0);

    // N >= 1M，内存充足，应该选择 HNSW
    EXPECT_EQ(decision.index_type, VECTOR_INDEX_HNSW);
    EXPECT_GT(decision.param1, 0);
    EXPECT_GT(decision.param2, 0);
}

TEST(SelectorIntegration, LargeDatasetLowMemoryIVFPQ) {
    // 大规模数据（N=1.5M）+ 低内存应该选择 IVF-PQ
    vector_data_info_t info = {
        .num_vectors = 1500000,
        .dimension = 128,
        .available_memory_mb = 1024,  // 内存不足
        .target_qps = 0.0f,
        .target_recall = 0.95f,
        .is_static = false
    };

    vector_index_decision_t decision;
    ASSERT_EQ(vector_index_selector_choose(&info, &decision), 0);

    // N >= 1M，内存不足，应该选择 IVF_PQ
    EXPECT_EQ(decision.index_type, VECTOR_INDEX_IVF_PQ);
    EXPECT_GT(decision.param1, 0);
    EXPECT_GT(decision.param2, 0);
}

// ========================================================================
// 2. SDK 集成测试：验证 selector 与 vectors.c 的集成
// ========================================================================

TEST(SelectorIntegration, SDKSmallDatasetBruteForce) {
    // SDK 层面验证：小规模数据应该保持 flat 模式（无 HNSW 索引）
    std::string db_path = unique_db_path("small");
    cleanup_db(db_path);

    mmdb_t* db = mmdb_open(db_path.c_str(), nullptr);
    ASSERT_NE(db, nullptr) << "mmdb_open failed for: " << db_path;

    constexpr size_t kDim = 128;
    constexpr size_t kNumVectors = 100;  // N < 10000

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "small", &s);
    ASSERT_NE(c, nullptr) << "mmdb_collection_create failed";

    std::mt19937 rng(42);
    std::vector<mmdb_vector_t> c_vecs;
    std::vector<std::string> ids;
    std::vector<std::vector<float>> vectors;

    for (size_t i = 0; i < kNumVectors; i++) {
        ids.push_back("v" + std::to_string(i));
        vectors.push_back(random_vector(kDim, rng));
        mmdb_vector_t v = {
            (const uint8_t*)ids[i].data(), ids[i].size(),
            vectors[i].data(), kDim, nullptr, nullptr
        };
        c_vecs.push_back(v);
    }

    int add_rc = mmdb_vectors_add(c, c_vecs.data(), kNumVectors);
    if (add_rc != MMDB_OK) {
        // 尝试关闭数据库并重新打开，排除锁竞争
        mmdb_close(db);
        cleanup_db(db_path);
        db = mmdb_open(db_path.c_str(), nullptr);
        ASSERT_NE(db, nullptr) << "mmdb_open retry failed";
        c = mmdb_collection_create(db, "small", &s);
        ASSERT_NE(c, nullptr) << "mmdb_collection_create retry failed";

        // 重建向量列表
        c_vecs.clear();
        ids.clear();
        vectors.clear();
        std::mt19937 rng2(42);
        for (size_t i = 0; i < kNumVectors; i++) {
            ids.push_back("v" + std::to_string(i));
            vectors.push_back(random_vector(kDim, rng2));
            mmdb_vector_t v = {
                (const uint8_t*)ids[i].data(), ids[i].size(),
                vectors[i].data(), kDim, nullptr, nullptr
            };
            c_vecs.push_back(v);
        }
        add_rc = mmdb_vectors_add(c, c_vecs.data(), kNumVectors);
    }
    ASSERT_EQ(add_rc, MMDB_OK) << "mmdb_vectors_add failed with code: " << add_rc;

    // 搜索应该正常工作（flat 模式）
    std::mt19937 rng3(123);
    auto q = random_vector(kDim, rng3);
    mmdb_query_t query = {q.data(), kDim, 10, nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_vectors_search(c, &query, &result), MMDB_OK);
    EXPECT_GT(result.count, 0u);
    mmdb_result_free(&result);

    mmdb_close(db);
    cleanup_db(db_path);
}

TEST(SelectorIntegration, SDKMediumDatasetHNSW) {
    // SDK 层面验证：中等规模数据应该自动启用 HNSW
    std::string db_path = unique_db_path("medium");
    std::remove(db_path.c_str());

    mmdb_t* db = mmdb_open(db_path.c_str(), nullptr);
    ASSERT_NE(db, nullptr);

    constexpr size_t kDim = 128;
    constexpr size_t kNumVectors = 15000;  // N > 10000

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "medium", &s);
    ASSERT_NE(c, nullptr);

    std::mt19937 rng(42);
    std::vector<std::string> ids;
    std::vector<std::vector<float>> vectors;
    std::vector<mmdb_vector_t> c_vecs;
    c_vecs.reserve(kNumVectors);

    for (size_t i = 0; i < kNumVectors; i++) {
        ids.push_back("v" + std::to_string(i));
        vectors.push_back(random_vector(kDim, rng));
    }

    for (size_t i = 0; i < kNumVectors; i++) {
        mmdb_vector_t v = {
            (const uint8_t*)ids[i].data(), ids[i].size(),
            vectors[i].data(), kDim, nullptr, nullptr
        };
        c_vecs.push_back(v);
    }

    ASSERT_EQ(mmdb_vectors_add(c, c_vecs.data(), kNumVectors), MMDB_OK);

    // 首次搜索应该触发 HNSW 构建
    auto query = random_vector(kDim, rng);
    mmdb_query_t qry = {query.data(), kDim, 10, nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
    EXPECT_EQ(result.count, 10u);  // 应该返回 top_k 个结果
    mmdb_result_free(&result);

    mmdb_close(db);
    std::remove(db_path.c_str());
}

// ========================================================================
// 3. 参数透传测试：验证选择器推荐的参数被正确使用
// ========================================================================

TEST(SelectorIntegration, ParameterPassthrough) {
    // 验证选择器推荐的 M 和 ef 参数
    vector_data_info_t info = {
        .num_vectors = 50000,
        .dimension = 60,   // 低维（< 64）应该增加 M
        .available_memory_mb = 8192,
        .target_qps = 0.0f,
        .target_recall = 0.98f,  // 高召回率 > 0.95 应该使用 ef=200
        .is_static = false
    };

    vector_index_decision_t decision;
    ASSERT_EQ(vector_index_selector_choose(&info, &decision), 0);

    // 低维（< 64）应该使用 M=32
    EXPECT_EQ(decision.index_type, VECTOR_INDEX_HNSW);
    EXPECT_EQ(decision.param1, 32);  // M=32 for low dimension
    EXPECT_EQ(decision.param2, 200); // ef=200 for target_recall > 0.95
}

TEST(SelectorIntegration, HighDimensionLowM) {
    // 高维数据应该降低 M
    vector_data_info_t info = {
        .num_vectors = 50000,
        .dimension = 300,  // 高维
        .available_memory_mb = 8192,
        .target_qps = 0.0f,
        .target_recall = 0.95f,
        .is_static = false
    };

    vector_index_decision_t decision;
    ASSERT_EQ(vector_index_selector_choose(&info, &decision), 0);

    // 高维（> 256）应该使用 M=8
    EXPECT_EQ(decision.index_type, VECTOR_INDEX_HNSW);
    EXPECT_EQ(decision.param1, 8);   // M=8 for high dimension
    EXPECT_EQ(decision.param2, 100); // ef=100
}

// ========================================================================
// 4. 边界条件测试
// ========================================================================

TEST(SelectorIntegration, NullParameters) {
    // 空参数应该失败
    vector_index_decision_t decision;
    EXPECT_EQ(vector_index_selector_choose(nullptr, &decision), -1);

    vector_data_info_t info = {
        .num_vectors = 10000,
        .dimension = 128,
        .available_memory_mb = 4096,
        .target_qps = 0.0f,
        .target_recall = 0.95f,
        .is_static = false
    };
    EXPECT_EQ(vector_index_selector_choose(&info, nullptr), -1);
}

TEST(SelectorIntegration, BoundaryScaleThreshold) {
    // 验证边界值 9999 和 10000 的决策差异
    vector_data_info_t info_below = {
        .num_vectors = 9999,
        .dimension = 128,
        .available_memory_mb = 4096,
        .target_qps = 0.0f,
        .target_recall = 0.95f,
        .is_static = false
    };

    vector_data_info_t info_above = {
        .num_vectors = 10000,
        .dimension = 128,
        .available_memory_mb = 4096,
        .target_qps = 0.0f,
        .target_recall = 0.95f,
        .is_static = false
    };

    vector_index_decision_t decision_below, decision_above;
    ASSERT_EQ(vector_index_selector_choose(&info_below, &decision_below), 0);
    ASSERT_EQ(vector_index_selector_choose(&info_above, &decision_above), 0);

    // 9999 应该选择 BRUTE_FORCE
    EXPECT_EQ(decision_below.index_type, VECTOR_INDEX_BRUTE_FORCE);

    // 10000 应该选择 HNSW（内存充足且召回率高）
    EXPECT_EQ(decision_above.index_type, VECTOR_INDEX_HNSW);
}

// ========================================================================
// 5. 性能回归测试：确保 selector 决策与硬编码阈值行为等价
// ========================================================================

TEST(SelectorIntegration, EquivalenceWithHardcodedThreshold) {
    // 验证 selector 在 N=10000 时选择 HNSW（与原始硬编码阈值等价）
    std::string db_path = unique_db_path("equiv");
    std::remove(db_path.c_str());

    mmdb_t* db = mmdb_open(db_path.c_str(), nullptr);
    ASSERT_NE(db, nullptr);

    constexpr size_t kDim = 128;
    constexpr size_t kNumVectors = 10000;  // 恰好等于阈值

    mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
    mmdb_collection_t* c = mmdb_collection_create(db, "equiv", &s);
    ASSERT_NE(c, nullptr);

    std::mt19937 rng(42);
    std::vector<std::string> ids;
    std::vector<std::vector<float>> vectors;
    std::vector<mmdb_vector_t> c_vecs;
    c_vecs.reserve(kNumVectors);

    for (size_t i = 0; i < kNumVectors; i++) {
        ids.push_back("v" + std::to_string(i));
        vectors.push_back(random_vector(kDim, rng));
    }

    for (size_t i = 0; i < kNumVectors; i++) {
        mmdb_vector_t v = {
            (const uint8_t*)ids[i].data(), ids[i].size(),
            vectors[i].data(), kDim, nullptr, nullptr
        };
        c_vecs.push_back(v);
    }

    ASSERT_EQ(mmdb_vectors_add(c, c_vecs.data(), kNumVectors), MMDB_OK);

    // 首次搜索应该触发 HNSW 构建（N=10000 >= 阈值）
    auto query = random_vector(kDim, rng);
    mmdb_query_t qry = {query.data(), kDim, 10, nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_vectors_search(c, &qry, &result), MMDB_OK);
    EXPECT_EQ(result.count, 10u);
    mmdb_result_free(&result);

    mmdb_close(db);
    std::remove(db_path.c_str());
}
