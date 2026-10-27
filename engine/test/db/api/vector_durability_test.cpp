/**
 * @file vector_durability_test.cpp
 * @brief Vector API 持久化与崩溃恢复语义测试
 *
 * 背景：vector_api_save 原实现顺序为 flush → checkpoint → truncate → 写快照，
 * 即先丢弃 WAL 再写快照。一旦写快照失败（磁盘满 / 权限问题 / 进程被杀），
 * 已确认的写入永久丢失；后台 CheckpointManager 更是每周期无条件 truncate，
 * 而它只持有 WAL 句柄、根本看不到集合数据。
 *
 * 本文件锁定三条不变式：
 *  1. save 之后重新加载，向量不丢。
 *  2. 不 save 直接退出，重新加载靠 WAL replay 恢复。
 *  3. **快照写入失败时不得截断 WAL** —— 这是修复的核心，也是回归的判别点：
 *     修复前该用例会得到 3 条（新增的 2 条被截断丢弃），修复后应得到 5 条。
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" {
#include "db/api/vector_api.h"
}

#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif

namespace {

constexpr int32_t kDim = 4;
const std::string kDataDir = "./test_data/vector_durability";
const char *kColl = "durable";

/** 构造一个 4 维向量，第 idx 维为 1，其余为 0 */
void unit_vector(int idx, float *out) {
    for (int32_t i = 0; i < kDim; i++) {
        out[i] = (i == idx) ? 1.0f : 0.0f;
    }
}

VectorCreateParams make_create_params() {
    VectorCreateParams p;
    memset(&p, 0, sizeof(p));
    p.name = kColl;
    p.dimension = kDim;
    p.index_type = VECTOR_INDEX_BRUTE_FORCE;
    p.metric_type = VECTOR_METRIC_L2;
    return p;
}

/** 插入 n 条向量（从 unit_vector(i % kDim) 取） */
int insert_vectors(VectorAPI *api, int n) {
    float storage[8][kDim];
    float *ptrs[8];
    for (int i = 0; i < n; i++) {
        unit_vector(i % kDim, storage[i]);
        ptrs[i] = storage[i];
    }

    VectorInsertParams ip;
    memset(&ip, 0, sizeof(ip));
    ip.collection = kColl;
    ip.n = n;
    ip.vectors = ptrs;

    return vector_api_insert(api, &ip, nullptr);
}

std::string meta_path() { return kDataDir + "/collections.meta"; }

}  // namespace

class VectorDurabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::error_code ec;
        std::filesystem::remove_all(kDataDir, ec);
        std::filesystem::create_directories(kDataDir, ec);
    }

    void TearDown() override {
        // 恢复可能被改成只读的 meta 文件，避免影响后续清理
        std::error_code ec;
        chmod(meta_path().c_str(), S_IWRITE);
        std::filesystem::remove_all(kDataDir, ec);
    }

    /** interval=0 关闭后台 CheckpointManager，保证用例可确定地复现 */
    VectorAPI *open_api() {
        VectorAPI *api = vector_api_create_with_interval(kDataDir.c_str(), 0);
        EXPECT_NE(nullptr, api);
        return api;
    }
};

TEST_F(VectorDurabilityTest, SaveThenReloadPreservesVectors) {
    VectorAPI *api = open_api();
    ASSERT_NE(nullptr, api);

    VectorCreateParams cp = make_create_params();
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &cp));
    ASSERT_EQ(3, insert_vectors(api, 3));

    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));
    EXPECT_EQ(3, vector_api_size(api, kColl));

    vector_api_destroy(api);

    /* 重新打开并加载：数据应从 collections.meta 快照恢复 */
    api = open_api();
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));
    EXPECT_EQ(3, vector_api_size(api, kColl));

    vector_api_destroy(api);
}

TEST_F(VectorDurabilityTest, NoSaveReloadRecoversFromWal) {
    VectorAPI *api = open_api();
    ASSERT_NE(nullptr, api);

    VectorCreateParams cp = make_create_params();
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &cp));
    ASSERT_EQ(4, insert_vectors(api, 4));

    /* 不调用 save，直接销毁：WAL 是唯一真相，reload 必须靠 replay 恢复 */
    vector_api_destroy(api);

    api = open_api();
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));
    EXPECT_EQ(4, vector_api_size(api, kColl));

    vector_api_destroy(api);
}

/**
 * 核心回归用例：快照写失败时，WAL 不能被截断
 *
 * 步骤：save 一次（基线 3 条已落快照）→ 再插 2 条（只存在于 WAL）→
 *       把 collections.meta 置为只读使快照写入失败 → save → 恢复权限 →
 *       重新加载。
 *
 * 期望：新增的 2 条仍在（共 5 条）。修复前实现会先 truncate 再写快照，
 *       这 2 条会被永久丢弃，用例得到 3。
 */
TEST_F(VectorDurabilityTest, SnapshotFailureDoesNotTruncateWal) {
    VectorAPI *api = open_api();
    ASSERT_NE(nullptr, api);

    VectorCreateParams cp = make_create_params();
    ASSERT_EQ(VECTOR_API_OK, vector_api_create_collection(api, &cp));
    ASSERT_EQ(3, insert_vectors(api, 3));
    ASSERT_EQ(VECTOR_API_OK, vector_api_save(api));
    ASSERT_EQ(3, vector_api_size(api, kColl));

    /* 这 2 条只写进 WAL，尚未落快照 */
    ASSERT_EQ(2, insert_vectors(api, 2));
    ASSERT_EQ(5, vector_api_size(api, kColl));

    /* 让快照写入必然失败 */
    ASSERT_EQ(0, chmod(meta_path().c_str(), S_IREAD))
        << "无法把 " << meta_path() << " 置为只读，用例前提不成立";

    int rc = vector_api_save(api);
    EXPECT_NE(VECTOR_API_OK, rc) << "只读的 collections.meta 不应写成功";

    /* 恢复可写，否则后续加载/清理都会失败 */
    ASSERT_EQ(0, chmod(meta_path().c_str(), S_IWRITE));

    vector_api_destroy(api);

    /* 重新加载：若 WAL 未被截断，replay 应补回那 2 条 */
    api = open_api();
    ASSERT_NE(nullptr, api);
    EXPECT_EQ(VECTOR_API_OK, vector_api_load(api));
    EXPECT_EQ(5, vector_api_size(api, kColl))
        << "快照失败后 WAL 被截断，已确认的写入丢失";

    vector_api_destroy(api);
}
