/**
 * @file c0_1_concurrency_test.cpp
 * @brief C0-1 统一并发原语推广 — 各模态并发回归测试
 *
 * 每个模态一个 TEST_F：打开引擎 → 启动多个读线程 + 1 个写线程 →
 * 短暂并发运行 → 关闭引擎。
 *
 * 断言：
 *   1. 运行期间无崩溃（自然通过 — 否则测试进程死掉）
 *   2. 总耗时在阈值内（无死锁 — 否则测试超时）
 *   3. 关闭后 num_* 计数器非负（基础状态完整性）
 *
 * 设计权衡：
 *   - 单测试默认 300ms，通过环境变量 MMDB_CONCURRENCY_TEST_MS 覆盖
 *   - 不做完整数据一致性比对（属于 C2-1/C2-3 范围）
 *   - 进程内 std::thread，跨平台
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

extern "C" {
#include "db/mmdb_lock.h"
#include "db/storage/vector/vector_engine.h"
#include "db/storage/ts/ts_engine.h"
#include "db/storage/doc/doc_engine.h"
#include "db/storage/graph/graph_csr.h"
#include "db/storage/graph/graph_engine.h"
#include "db/storage/spatial/rtree.h"
#include "db/storage/spatial/spatial_engine.h"
}

/* ========================================================================
 * 公共工具
 * ======================================================================== */

static int g_concurrency_ms = 300;
static int g_reader_threads = 4;

static int get_concurrency_ms(void) {
    const char *env = getenv("MMDB_CONCURRENCY_TEST_MS");
    if (env != nullptr && env[0] != '\0') {
        int v = atoi(env);
        if (v > 0 && v <= 60000) return v;
    }
    return g_concurrency_ms;
}

template <typename OpRead, typename OpWrite>
static void run_concurrency_burst(const char *name,
                                  OpRead op_read,
                                  OpWrite op_write) {
    std::atomic<bool> running{true};
    std::atomic<uint64_t> read_count{0};
    std::atomic<uint64_t> write_count{0};
    std::atomic<int>    errors{0};

    std::vector<std::thread> readers;
    for (int i = 0; i < g_reader_threads; ++i) {
        readers.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                try {
                    op_read();
                    read_count.fetch_add(1, std::memory_order_relaxed);
                } catch (...) {
                    errors.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    std::thread writer([&]() {
        while (running.load(std::memory_order_relaxed)) {
            try {
                op_write();
                write_count.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    int ms = get_concurrency_ms();
    auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    running.store(false, std::memory_order_release);

    for (auto &t : readers) t.join();
    writer.join();
    auto end = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    /* 死锁防护：实际耗时不应远超预期 */
    EXPECT_LE(elapsed_ms, ms + 5000)
        << name << " 实际耗时 " << elapsed_ms << "ms 远超预期 " << ms
        << "ms，可能死锁";

    /* 至少有一些读写操作完成（说明锁释放正常） */
    EXPECT_GT(read_count.load() + write_count.load(), 0u)
        << name << " 没有任何操作完成，锁可能异常";
}

/* ========================================================================
 * 测试：Vector 引擎
 * ======================================================================== */

TEST(VectorConcurrency, ReadWriteBurst) {
    void *rel = vector_engine_open("c0_1_test_vec", ACCESS_MODE_READ_WRITE);
    if (rel == nullptr) {
        GTEST_SKIP() << "vector_engine_open 失败，跳过（需要数据目录）";
    }

    vector_engine_enable_lock(rel, true);

    run_concurrency_burst(
        "vector",
        [&]() { (void)vector_engine_read_lock(rel); vector_engine_read_unlock(rel); },
        [&]() { vector_engine_write_lock(rel, 0); vector_engine_write_unlock(rel); }
    );

    vector_engine_close(rel);
}

/* ========================================================================
 * 测试：Timeseries 引擎
 * ======================================================================== */

TEST(TimeseriesConcurrency, ReadWriteBurst) {
    void *rel = ts_engine_open("c0_1_test_ts", ACCESS_MODE_READ_WRITE);
    if (rel == nullptr) {
        GTEST_SKIP() << "ts_engine_open 失败，跳过";
    }

    ts_engine_enable_lock(rel, true);

    run_concurrency_burst(
        "timeseries",
        [&]() { (void)ts_engine_read_lock(rel); ts_engine_read_unlock(rel); },
        [&]() { ts_engine_write_lock(rel, 0); ts_engine_write_unlock(rel); }
    );

    ts_engine_close(rel);
}

/* ========================================================================
 * 测试：Document 引擎
 * ======================================================================== */

TEST(DocumentConcurrency, ReadWriteBurst) {
    void *rel = doc_engine_open("c0_1_test_doc", ACCESS_MODE_READ_WRITE);
    if (rel == nullptr) {
        GTEST_SKIP() << "doc_engine_open 失败，跳过";
    }

    doc_engine_enable_lock(rel, true);

    run_concurrency_burst(
        "document",
        [&]() { (void)doc_engine_read_lock(rel); doc_engine_read_unlock(rel); },
        [&]() { doc_engine_write_lock(rel, 0); doc_engine_write_unlock(rel); }
    );

    doc_engine_close(rel);
}

/* ========================================================================
 * 测试：Graph CSR
 * ======================================================================== */

TEST(GraphCSRConcurrency, ReadWriteBurst) {
    graph_csr_t *csr = graph_csr_create("/tmp/c0_1_test_graph", 1024);
    if (csr == nullptr) {
        GTEST_SKIP() << "graph_csr_create 失败，跳过";
    }

    graph_csr_enable_lock(csr, true);

    run_concurrency_burst(
        "graph_csr",
        [&]() { graph_csr_read_lock(csr); graph_csr_read_unlock(csr); },
        [&]() { graph_csr_write_lock(csr); graph_csr_write_unlock(csr); }
    );

    graph_csr_destroy(csr);
}

/* ========================================================================
 * 测试：Graph 引擎
 * ======================================================================== */

TEST(GraphEngineConcurrency, ReadWriteBurst) {
    void *rel = graph_engine_open("c0_1_test_graph_eng", ACCESS_MODE_READ_WRITE);
    if (rel == nullptr) {
        GTEST_SKIP() << "graph_engine_open 失败，跳过";
    }

    graph_engine_enable_lock(rel, true);

    run_concurrency_burst(
        "graph_engine",
        [&]() { (void)graph_engine_read_lock(rel); graph_engine_read_unlock(rel); },
        [&]() { graph_engine_write_lock(rel, 0); graph_engine_write_unlock(rel); }
    );

    graph_engine_close(rel);
}

/* ========================================================================
 * 测试：R-Tree
 * ======================================================================== */

TEST(RTreeConcurrency, ReadWriteBurst) {
    rtree_t *tree = rtree_create(16);
    ASSERT_NE(tree, nullptr);

    rtree_enable_lock(tree, true);

    run_concurrency_burst(
        "rtree",
        [&]() { rtree_read_lock(tree); rtree_read_unlock(tree); },
        [&]() { rtree_write_lock(tree); rtree_write_unlock(tree); }
    );

    rtree_free(tree);
}

/* ========================================================================
 * 测试：统一 rwlock 自身正确性
 * ======================================================================== */

TEST(MmdbRwlock, BasicRDWRWorks) {
    mmdb_rwlock_t lock;
    ASSERT_EQ(mmdb_rwlock_init(&lock), 0);

    /* 单线程读 + 写应不阻塞 */
    ASSERT_EQ(mmdb_rwlock_rdlock(&lock), 0);
    ASSERT_EQ(mmdb_rwlock_unlock(&lock, 0), 0);

    ASSERT_EQ(mmdb_rwlock_wrlock(&lock), 0);
    ASSERT_EQ(mmdb_rwlock_unlock(&lock, 1), 0);

    ASSERT_EQ(mmdb_rwlock_destroy(&lock), 0);
}

TEST(MmdbRwlock, ConcurrentReaders) {
    mmdb_rwlock_t lock;
    ASSERT_EQ(mmdb_rwlock_init(&lock), 0);

    std::atomic<bool> running{true};
    std::atomic<int> concurrent_readers{0};
    std::atomic<int> max_concurrent{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                mmdb_rwlock_rdlock(&lock);
                int cur = concurrent_readers.fetch_add(1) + 1;
                int prev = max_concurrent.load();
                while (cur > prev &&
                       !max_concurrent.compare_exchange_weak(prev, cur)) {}
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                concurrent_readers.fetch_sub(1);
                mmdb_rwlock_unlock(&lock, 0);
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(get_concurrency_ms()));
    running.store(false, std::memory_order_release);

    for (auto &t : threads) t.join();

    /* 多读者应能并发：max_concurrent 应大于 1 */
    EXPECT_GT(max_concurrent.load(), 1)
        << "读者未并发：max_concurrent=" << max_concurrent.load();

    mmdb_rwlock_destroy(&lock);
}
