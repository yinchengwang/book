/**
 * @file test_kv.cpp
 * @brief KV API 测试
 */

#include <gtest/gtest.h>
#include "db/kv.h"
#include <cstdio>
#include <cstring>

// 每个测试用不同的数据库文件
const char* test_file = "test_kv.db";

TEST(KVTest, SimplePutGet) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    const char *key = "hello";
    const char *val = "world";
    EXPECT_EQ(KV_OK, kv_put(db, key, strlen(key), val, strlen(val)));

    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, key, strlen(key), &out_value, &out_len));
    EXPECT_EQ(strlen(val), out_len);
    EXPECT_EQ(0, memcmp(val, out_value, out_len));
    free(out_value);

    kv_close(db);
}

TEST(KVTest, Scan) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "a", 1, "1", 1);
    kv_put(db, "b", 1, "2", 1);
    kv_put(db, "c", 1, "3", 1);

    kv_iter_t *iter = kv_scan(db, NULL, 0, NULL, 0);
    ASSERT_NE(nullptr, iter);

    int count = 0;
    while (kv_iter_next(iter) == KV_OK) {
        count++;
    }
    kv_iter_free(iter);

    EXPECT_EQ(3, count);
    kv_close(db);
}

TEST(KVTest, Delete) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    const char *key = "to_delete";
    kv_put(db, key, strlen(key), "data", 4);

    EXPECT_TRUE(kv_exists(db, key, strlen(key)));
    kv_delete(db, key, strlen(key));
    EXPECT_FALSE(kv_exists(db, key, strlen(key)));

    kv_close(db);
}
