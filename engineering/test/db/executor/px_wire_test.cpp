#include <gtest/gtest.h>
#include <vector>
#include <cstring>

extern "C" {
#include "db/executor/px_wire.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

static VectorBlock *make_mixed_block() {
    VectorBlock *b = vector_block_create(4, 3);
    int32_t *c0 = (int32_t *)malloc(sizeof(int32_t) * 4);
    int64_t *c1 = (int64_t *)malloc(sizeof(int64_t) * 4);
    const char **c2 = (const char **)malloc(sizeof(char *) * 4);
    for (int i = 0; i < 4; i++) { c0[i] = i * 7; c1[i] = 1000000LL * i; }
    c2[0] = "alpha"; c2[1] = "b"; c2[2] = ""; c2[3] = "delta-long-string";
    vector_block_set_column(b, 0, c0, sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);
    vector_block_set_column(b, 1, c1, sizeof(int64_t));
    vector_block_set_column_type(b, 1, COLUMN_INT64);
    vector_block_set_column(b, 2, (void *)c2, sizeof(char *));
    vector_block_set_column_type(b, 2, COLUMN_STRING);
    vector_block_set_num_rows(b, 4);
    vector_block_set_null(b, 2, true);            /* 第 2 行整行 null */
    return b;
}

TEST(PxWire, RoundtripPreservesAllColumnKinds) {
    VectorBlock *src = make_mixed_block();
    uint8_t *buf = nullptr;
    uint32_t size = 0;
    ASSERT_EQ(px_wire_serialize(src, 42, 0, nullptr, &buf, &size), 0);
    ASSERT_NE(buf, nullptr);
    ASSERT_GT(size, 0u);

    uint32_t seq = 0, flags = 0;
    VectorBlock *dst = px_wire_deserialize(buf, size, &seq, &flags);
    ASSERT_NE(dst, nullptr);
    EXPECT_EQ(seq, 42u);
    EXPECT_EQ(flags, 0u);
    ASSERT_EQ(dst->num_rows, 4);
    ASSERT_EQ(dst->num_columns, 3);

    EXPECT_EQ(vector_block_get_column_type(dst, 0), COLUMN_INT32);
    EXPECT_EQ(vector_block_get_column_type(dst, 1), COLUMN_INT64);
    EXPECT_EQ(vector_block_get_column_type(dst, 2), COLUMN_STRING);

    int32_t *c0 = (int32_t *)dst->columns[0];
    int64_t *c1 = (int64_t *)dst->columns[1];
    const char **c2 = (const char **)dst->columns[2];
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(c0[i], i * 7);
        EXPECT_EQ(c1[i], 1000000LL * i);
    }
    EXPECT_STREQ(c2[0], "alpha");
    EXPECT_STREQ(c2[1], "b");
    EXPECT_STREQ(c2[2], "");
    EXPECT_STREQ(c2[3], "delta-long-string");
    EXPECT_TRUE(vector_block_is_null(dst, 2));
    EXPECT_FALSE(vector_block_is_null(dst, 1));

    vector_block_destroy(dst);
    vector_block_destroy(src);
    free(buf);
}

TEST(PxWire, LastFlagFrameHasNoBlock) {
    uint8_t *buf = nullptr;
    uint32_t size = 0;
    ASSERT_EQ(px_wire_serialize(nullptr, 7, PXW_FLAG_LAST, nullptr, &buf, &size), 0);
    uint32_t seq, flags;
    VectorBlock *b = px_wire_deserialize(buf, size, &seq, &flags);
    EXPECT_EQ(b, nullptr);
    EXPECT_EQ(seq, 7u);
    EXPECT_EQ(flags & PXW_FLAG_LAST, PXW_FLAG_LAST);
    free(buf);
}

TEST(PxWire, CorruptedCrcRejected) {
    VectorBlock *src = make_mixed_block();
    uint8_t *buf = nullptr;
    uint32_t size = 0;
    ASSERT_EQ(px_wire_serialize(src, 1, 0, nullptr, &buf, &size), 0);
    buf[size / 2] ^= 0xFF;                        /* 翻转中间一字节 */
    uint32_t seq, flags;
    VectorBlock *b = px_wire_deserialize(buf, size, &seq, &flags);
    EXPECT_EQ(b, nullptr);
    EXPECT_EQ(flags, 0xFFFFFFFFu);                /* CRC 失败哨兵 */
    free(buf);
    vector_block_destroy(src);
}
