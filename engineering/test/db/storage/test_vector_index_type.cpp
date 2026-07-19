/**
 * @file test_vector_index_type.cpp
 * @brief vector_engine active_index_type 字段测试（Task 2.21）
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/vector_engine.h"
}

TEST(VectorIndexTypeTest, ConstantsDefined) {
    EXPECT_EQ(VEC_INDEX_NONE, 0);
    EXPECT_EQ(VEC_INDEX_HNSW, 1);
    EXPECT_EQ(VEC_INDEX_IVF_PQ, 2);
}

TEST(VectorIndexTypeTest, StructHasActiveIndexType) {
    /* 验证结构体字段存在（编译时检查） */
    vector_engine_db_t db;
    memset(&db, 0, sizeof(db));
    db.active_index_type = VEC_INDEX_NONE;
    EXPECT_EQ(db.active_index_type, VEC_INDEX_NONE);

    db.active_index_type = VEC_INDEX_HNSW;
    EXPECT_EQ(db.active_index_type, VEC_INDEX_HNSW);

    db.active_index_type = VEC_INDEX_IVF_PQ;
    EXPECT_EQ(db.active_index_type, VEC_INDEX_IVF_PQ);
}