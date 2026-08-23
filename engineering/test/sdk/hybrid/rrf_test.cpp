// rrf_test.cpp — P3-T1.1：Reciprocal Rank Fusion 多通道融合算法单元测试
//
// 算法公式：score(d) = sum_i (1 / (k + rank_i(d)))，默认 k=60（Cormack 2009）
#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "sdk/impl/hybrid_search.h"
}

TEST(RRF, FuseTwoChannels) {
    mmdb_rrf_doc_t docs[3] = {};
    /* doc A: 通道 1 排名 1，通道 2 排名 3 */
    docs[0].id = (const uint8_t*)"A"; docs[0].id_len = 1;
    docs[0].source_ranks[0] = 1; docs[0].source_ranks[1] = 3;
    docs[0].source_count = 2;
    /* doc B: 通道 1 排名 2，通道 2 排名 1 */
    docs[1].id = (const uint8_t*)"B"; docs[1].id_len = 1;
    docs[1].source_ranks[0] = 2; docs[1].source_ranks[1] = 1;
    docs[1].source_count = 2;
    /* doc C: 通道 1 排名 3，通道 2 未命中 */
    docs[2].id = (const uint8_t*)"C"; docs[2].id_len = 1;
    docs[2].source_ranks[0] = 3; docs[2].source_count = 1;

    mmdb_rrf_config_t cfg;
    mmdb_rrf_config_init(&cfg);
    ASSERT_EQ(cfg.k, 60);
    ASSERT_EQ(mmdb_rrf_fuse(docs, 3, &cfg), 0);

    /* A 得分 = 1/61 + 1/63 ≈ 0.0323
       B 得分 = 1/62 + 1/61 ≈ 0.0323
       C 得分 = 1/63 ≈ 0.0159 */
    EXPECT_GT(docs[0].rrf_score, docs[2].rrf_score);
    EXPECT_GT(docs[1].rrf_score, docs[2].rrf_score);
}

TEST(RRF, EmptyInput) {
    mmdb_rrf_config_t cfg;
    mmdb_rrf_config_init(&cfg);
    /* 空文档列表应返回 0，不崩溃 */
    EXPECT_EQ(mmdb_rrf_fuse(nullptr, 0, &cfg), 0);
}

TEST(RRF, SingleChannel) {
    mmdb_rrf_doc_t docs[2] = {};
    docs[0].id = (const uint8_t*)"X"; docs[0].id_len = 1;
    docs[0].source_ranks[0] = 1; docs[0].source_count = 1;
    docs[1].id = (const uint8_t*)"Y"; docs[1].id_len = 1;
    docs[1].source_ranks[0] = 2; docs[1].source_count = 1;

    mmdb_rrf_config_t cfg;
    mmdb_rrf_config_init(&cfg);
    ASSERT_EQ(mmdb_rrf_fuse(docs, 2, &cfg), 0);

    /* X 排名靠前 → 得分更高 */
    EXPECT_GT(docs[0].rrf_score, docs[1].rrf_score);
}

TEST(RRF, ExceedsEightChannels) {
    /* source_ranks[8] 容量上限：超过 8 个通道时只融合前 8 个 */
    mmdb_rrf_doc_t doc = {};
    doc.id = (const uint8_t*)"Z"; doc.id_len = 1;
    for (size_t i = 0; i < 12; i++) {
        doc.source_ranks[i] = 1;  /* 前 8 个填 rank=1，其余应被忽略 */
    }
    doc.source_count = 12;  /* 故意超过 8 */

    mmdb_rrf_config_t cfg;
    mmdb_rrf_config_init(&cfg);
    ASSERT_EQ(mmdb_rrf_fuse(&doc, 1, &cfg), 0);

    /* 实际累加 8 次 1/(60+1) ≈ 0.1311（不是 12 次） */
    EXPECT_NEAR(doc.rrf_score, 8.0 / 61.0, 1e-9);
}

TEST(RRF, ZeroRankTreatedAsMiss) {
    /* source_ranks[i] == 0 表示该通道未命中，不应贡献得分 */
    mmdb_rrf_doc_t doc = {};
    doc.id = (const uint8_t*)"W"; doc.id_len = 1;
    doc.source_ranks[0] = 5;
    doc.source_ranks[1] = 0;  /* 未命中 */
    doc.source_ranks[2] = 3;
    doc.source_count = 3;

    mmdb_rrf_config_t cfg;
    mmdb_rrf_config_init(&cfg);
    ASSERT_EQ(mmdb_rrf_fuse(&doc, 1, &cfg), 0);

    /* 期望 = 1/65 + 1/63 ≈ 0.0312 */
    double expected = 1.0 / 65.0 + 1.0 / 63.0;
    EXPECT_NEAR(doc.rrf_score, expected, 1e-9);
}