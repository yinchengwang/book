// xquery_id_len_test.cpp — P4-T4.3：id 超过 256B 时返回 MMDB_ERR_INVALID
//
// 关闭 CI-3：当 source (text) 集合返回的候选 id 长度 > 256B 时，
// xquery 应升级为返回 MMDB_ERR_INVALID（不再静默跳过）。
//
// 测试场景：
//   1. OversizedIdReturnsInvalid — source 含 1 个 305B id 的 doc，调用
//      mmdb_xquery_text_to_vector 必须返回 MMDB_ERR_INVALID。
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_text.h"
#include "sdk/mmdb_xquery.h"
}

namespace {
constexpr const char* kDbPath = "test_xquery_id_len.db";

/* 超过 256B 的 id 长度阈值（本测试用 300B） */
constexpr size_t kOversizedIdLen = 300;
}

class XQueryIdLenTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        mmdb_schema_t vs = {MMDB_MODEL_VECTOR, 0, nullptr, 4};
        vc_ = mmdb_collection_create(db_, "v_emb", &vs);
        mmdb_schema_t ts = {MMDB_MODEL_TEXT, 0, nullptr, 0};
        tc_ = mmdb_collection_create(db_, "t_docs", &ts);
        ASSERT_NE(vc_, nullptr);
        ASSERT_NE(tc_, nullptr);

        /* 插入 1 个 300B id 的 doc。
         * id 是无内嵌 '\0' 的 ASCII 字符串（FTS5/SQLite TEXT 类型安全）。
         * text 包含 "test" 以便 FTS5 检索命中。 */
        oversized_id_.assign(kOversizedIdLen, 'a');
        oversized_id_ += "_tail";  /* 305 字节（300 + "_tail"），确保 > 256 */
        oversized_id_len_ = oversized_id_.size();

        mmdb_text_entry_t e = {
            oversized_id_.c_str(),
            "this is a test document",
            nullptr
        };
        ASSERT_EQ(mmdb_text_add(tc_, &e), MMDB_OK);

        /* 再插入 1 个短 id 的 doc（确保 FTS5 至少返回 2 个候选） */
        const char* short_id = "doc_short";
        mmdb_text_entry_t e2 = {
            short_id,
            "another test entry",
            nullptr
        };
        ASSERT_EQ(mmdb_text_add(tc_, &e2), MMDB_OK);

        /* vector 集合里也要有对应短 id 的向量（target 侧按 id 查询） */
        float vec[4] = {1.0f, 0.0f, 0.5f, 0.1f};
        mmdb_vector_t v = {
            (const uint8_t*)short_id, std::strlen(short_id),
            vec, 4, nullptr, nullptr
        };
        ASSERT_EQ(mmdb_vectors_add(vc_, &v, 1), MMDB_OK);
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }

    mmdb_t* db_ = nullptr;
    mmdb_collection_t* vc_ = nullptr;
    mmdb_collection_t* tc_ = nullptr;
    std::string oversized_id_;
    size_t oversized_id_len_ = 0;
};

TEST_F(XQueryIdLenTest, OversizedIdReturnsInvalid) {
    /* query 向量（dim=4，与 target collection 一致） */
    float query[4] = {1.0f, 0.0f, 0.5f, 0.1f};

    mmdb_xquery_text_to_vector_t xq = {};
    xq.source = tc_;
    xq.text_query = "test";
    xq.target = vc_;
    xq.query_vector = query;
    xq.dim = 4;
    xq.top_k = 5;
    xq.max_source_candidates = 100;

    mmdb_result_t out = {};
    /* 期望返回 MMDB_ERR_INVALID（id 超 256B → 升级为错误） */
    EXPECT_EQ(mmdb_xquery_text_to_vector(&xq, &out), MMDB_ERR_INVALID);

    /* 异常返回路径下，out 应当被清空（或不分配 items）。为不依赖内部
     * 清理约定，这里调用 mmdb_result_free 做防御性释放。 */
    if (out.items || out.count > 0) {
        mmdb_result_free(&out);
    }
}
