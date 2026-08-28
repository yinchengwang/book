/**
 * @file c1_3_kv_concurrency_test.cpp
 * @brief C1-3 KV 并发正确性测试
 *
 * 场景：N 个线程并发执行"读 value → value+1 → 写 value"循环 M 次。
 * 预期：最终 value == N*M。
 * 修复前（无锁）：read-modify-write 非原子，多个线程读到同值，多写覆盖。
 * 修复后（mmdb_rwlock 包裹）：最终值正确。
 */

#include <gtest/gtest.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

extern "C" {
#include "db/kv.h"
}

namespace {

constexpr int kThreads = 4;
constexpr int kIterations = 500;
const char *kTestKey = "counter";
const char *kTestDbPath = "/tmp/c1_3_kv_test.db";

}  // namespace

TEST(KvConcurrency, PutSameKeyNoLostUpdate) {
    /* 清理可能残留的旧 db 文件 */
    remove(kTestDbPath);

    kv_t *db = kv_open(kTestDbPath);
    if (db == nullptr) {
        GTEST_SKIP() << "kv_open 失败（需要文件系统权限）";
    }

    /* 初始化 counter = 0 */
    {
        char v[32];
        snprintf(v, sizeof(v), "0");
        kv_result_t rc = kv_put(db, kTestKey, strlen(kTestKey) + 1,
                                 v, strlen(v) + 1);
        ASSERT_EQ(rc, KV_OK);
    }

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < kIterations; ++i) {
                void *val = nullptr;
                size_t val_len = 0;
                kv_result_t rc = kv_get(db, kTestKey, strlen(kTestKey) + 1,
                                         &val, &val_len);
                if (rc != KV_OK || val == nullptr) {
                    errors.fetch_add(1);
                    continue;
                }
                int v = atoi((const char *)val);
                free(val);
                v++;
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", v);
                rc = kv_put(db, kTestKey, strlen(kTestKey) + 1,
                             buf, strlen(buf) + 1);
                if (rc != KV_OK) errors.fetch_add(1);
            }
        });
    }

    for (auto &t : threads) t.join();

    /* 读最终值 */
    void *val = nullptr;
    size_t val_len = 0;
    kv_result_t rc = kv_get(db, kTestKey, strlen(kTestKey) + 1,
                             &val, &val_len);
    ASSERT_EQ(rc, KV_OK);
    int final_v = atoi((const char *)val);
    free(val);

    int expected = kThreads * kIterations;
    EXPECT_EQ(final_v, expected)
        << "并发 put 丢更新：期望 " << expected << "，实际 " << final_v
        << "（差距 " << (expected - final_v) << " 表示丢失更新次数）";
    EXPECT_EQ(errors.load(), 0);

    kv_close(db);
}
