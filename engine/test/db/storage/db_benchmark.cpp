/**
 * @file db_benchmark.cpp
 * @brief 数据库核心模块性能基准测试
 *
 * 测试 Buffer Pool、BTree、WAL、MVCC 的吞吐量、延迟和并发性能。
 */

#include "gtest/gtest.h"
#include "db/buf.h"
#include "db/btreeam.h"
#include "db/rel.h"
#include "db/wal.h"
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>

/* ============================================================
 * 基准测试工具函数
 * ============================================================ */

/**
 * @brief 计算基准测试结果
 */
struct BenchmarkResult {
    double mean_ms;
    double stddev_ms;
    double min_ms;
    double max_ms;
    double throughput;  /* ops/sec */
};

template<typename Func>
BenchmarkResult run_benchmark(const char *name, int iterations, Func func) {
    std::vector<double> times;

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(elapsed);
    }

    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    double mean = sum / times.size();

    double sq_sum = 0;
    for (double t : times) {
        sq_sum += (t - mean) * (t - mean);
    }
    double stddev = std::sqrt(sq_sum / times.size());

    BenchmarkResult result;
    result.mean_ms = mean;
    result.stddev_ms = stddev;
    result.min_ms = *std::min_element(times.begin(), times.end());
    result.max_ms = *std::max_element(times.begin(), times.end());
    result.throughput = 1000.0 / mean;

    printf("[Benchmark] %s: %.2f ms/op (σ=%.2f), %.0f ops/sec\n",
           name, mean, stddev, result.throughput);

    return result;
}

/* ============================================================
 * 2.5.1 Buffer Pool 吞吐基准测试
 * ============================================================ */

class BufferPoolBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, buf_init(256));
        buf_reset_stats();
    }

    void TearDown() override {
        buf_shutdown();
    }
};

/**
 * @brief 单页面读取吞吐量测试
 */
TEST_F(BufferPoolBenchmarkTest, SinglePageReadThroughput) {
    constexpr int iterations = 10000;

    auto result = run_benchmark("buf_read (single page)", iterations, [&]() {
        BufferDesc *buf = buf_read(1, 0, 0);
        if (buf) buf_unpin(buf);
    });

    EXPECT_GT(result.throughput, 10000);
}

/**
 * @brief 顺序页面读取吞吐量测试
 */
TEST_F(BufferPoolBenchmarkTest, SequentialPageReadThroughput) {
    constexpr int iterations = 5000;

    auto result = run_benchmark("buf_read (sequential)", iterations, [&]() {
        static int page = 0;
        BufferDesc *buf = buf_read(1, (BlockNumber)(page++), 0);
        if (buf) buf_unpin(buf);
        if (page >= 1000) page = 0;
    });

    EXPECT_GT(result.throughput, 5000);
}

/**
 * @brief 随机页面读取吞吐量测试
 */
TEST_F(BufferPoolBenchmarkTest, RandomPageReadThroughput) {
    constexpr int iterations = 5000;

    auto result = run_benchmark("buf_read (random)", iterations, [&]() {
        BlockNumber page = (BlockNumber)(rand() % 10000);
        BufferDesc *buf = buf_read(1, page, 0);
        if (buf) buf_unpin(buf);
    });

    EXPECT_GT(result.throughput, 3000);
}

/**
 * @brief 缓存命中吞吐量测试
 */
TEST_F(BufferPoolBenchmarkTest, CacheHitThroughput) {
    constexpr int iterations = 10000;

    /* 预热：读取页面确保在缓存中 */
    BufferDesc *warmup = buf_read(1, 42, 0);
    if (warmup) buf_unpin(warmup);

    auto result = run_benchmark("buf_read (cache hit)", iterations, [&]() {
        BufferDesc *buf = buf_read(1, 42, 0);
        if (buf) buf_unpin(buf);
    });

    /* 缓存命中应该比未命中快很多 */
    EXPECT_GT(result.throughput, 50000);
}

/**
 * @brief 脏页写入吞吐量测试
 */
TEST_F(BufferPoolBenchmarkTest, DirtyPageWriteThroughput) {
    constexpr int iterations = 5000;

    auto result = run_benchmark("buf_read + dirty", iterations, [&]() {
        static int page = 0;
        BufferDesc *buf = buf_read(1, (BlockNumber)(page++), 1);
        if (buf) {
            char *data = (char *)buf_get_data(buf);
            if (data) {
                snprintf(data, 32, "Benchmark-%d", page);
            }
            buf_dirty(buf);
            buf_unpin(buf);
        }
        if (page >= 1000) page = 0;
    });

    EXPECT_GT(result.throughput, 2000);
}

/* ============================================================
 * 2.5.2 BTree 插入/查询基准测试
 * ============================================================ */

class BTreeBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, btreeam_init());
        ASSERT_EQ(0, rel_init());

        rel_ = relation_opennode(1000, REL_OPEN_READWRITE);
        ASSERT_NE(nullptr, rel_);
        btcreate(rel_);

        btreeam_reset_stats();
    }

    void TearDown() override {
        btdestroy(rel_);
        relation_close(rel_, 0);
        btreeam_shutdown();
    }

    Relation rel_;
};

/**
 * @brief 顺序插入吞吐量测试
 */
TEST_F(BTreeBenchmarkTest, SequentialInsertThroughput) {
    constexpr int iterations = 1000;

    auto result = run_benchmark("btinsert (sequential)", iterations, [&]() {
        static int key = 0;
        int k = key++;
        btinsert(rel_, (const void **)&k, 1, &k);
    });

    EXPECT_GT(result.throughput, 5000);
}

/**
 * @brief 随机插入吞吐量测试
 */
TEST_F(BTreeBenchmarkTest, RandomInsertThroughput) {
    constexpr int iterations = 1000;

    std::vector<int> keys;
    for (int i = 0; i < iterations; i++) {
        keys.push_back(rand() % 100000);
    }

    auto result = run_benchmark("btinsert (random)", iterations, [&]() {
        static size_t idx = 0;
        int k = keys[idx++ % keys.size()];
        btinsert(rel_, (const void **)&k, 1, &k);
    });

    EXPECT_GT(result.throughput, 3000);
}

/**
 * @brief 范围扫描吞吐量测试
 */
TEST_F(BTreeBenchmarkTest, RangeScanThroughput) {
    constexpr int num_keys = 10000;
    constexpr int iterations = 100;

    /* 先插入数据 */
    for (int i = 0; i < num_keys; i++) {
        int k = i;
        btinsert(rel_, (const void **)&k, 1, &k);
    }

    auto result = run_benchmark("bt_range_scan (100 items)", iterations, [&]() {
        BTScanDesc scan = bt_beginscan(rel_, 0, NULL);
        int count = 0;
        while (bt_getnext(scan, ForwardScanDirection) != nullptr && count < 100) {
            count++;
        }
        bt_endscan(scan);
    });

    EXPECT_GT(result.throughput, 100);
}

/**
 * @brief 批量加载吞吐量测试
 */
TEST_F(BTreeBenchmarkTest, BulkLoadThroughput) {
    constexpr int num_keys = 5000;
    constexpr int iterations = 10;

    auto result = run_benchmark("btbuild (bulk load)", iterations, [&]() {
        btdestroy(rel_);
        btcreate(rel_);

        void *tuples[1000];
        int keys[1000];

        for (int batch = 0; batch < num_keys / 1000; batch++) {
            for (int i = 0; i < 1000; i++) {
                keys[i] = batch * 1000 + i;
                tuples[i] = &keys[i];
            }
            btbuild(rel_, tuples, 1000);
        }
    });

    EXPECT_GT(result.throughput, 1);
}

/* ============================================================
 * 2.5.3 并发性能基准测试
 * ============================================================ */

class ConcurrencyBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, buf_init(128));
        ASSERT_EQ(0, btreeam_init());
        ASSERT_EQ(0, rel_init());
    }

    void TearDown() override {
        buf_shutdown();
        btreeam_shutdown();
    }
};

/**
 * @brief 多线程 Buffer 访问吞吐量测试
 */
TEST_F(ConcurrencyBenchmarkTest, MultiThreadedBufferAccess) {
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 5000;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ops_per_thread; i++) {
                BufferDesc *buf = buf_read((RelFileNode)(t + 1), (BlockNumber)i, 0);
                if (buf) buf_unpin(buf);
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    double throughput = (num_threads * ops_per_thread * 1000.0) / elapsed;

    printf("[Benchmark] Multi-threaded buf: %.0f ops/sec\n", throughput);
    EXPECT_GT(throughput, 5000);
}

/**
 * @brief 锁竞争测试
 */
TEST_F(ConcurrencyBenchmarkTest, LockContention) {
    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 2000;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ops_per_thread; i++) {
                BufferDesc *buf = buf_read(1, 0, 0);  /* 同一页面 */
                if (buf) buf_unpin(buf);
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    double throughput = (num_threads * ops_per_thread * 1000.0) / elapsed;

    printf("[Benchmark] Lock contention test: %.0f ops/sec\n", throughput);
    GTEST_SUCCEED();
}

/* ============================================================
 * 2.5.4 存储空间基准测试
 * ============================================================ */

class StorageSpaceBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, btreeam_init());
        ASSERT_EQ(0, rel_init());
    }

    void TearDown() override {
        btreeam_shutdown();
    }
};

/**
 * @brief BTree 空间开销测试
 */
TEST_F(StorageSpaceBenchmarkTest, BTreeSpaceOverhead) {
    Relation rel = relation_opennode(2000, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);
    btcreate(rel);

    constexpr int num_keys = 1000;

    for (int i = 0; i < num_keys; i++) {
        int key = i;
        btinsert(rel, (const void **)&key, 1, &key);
    }

    btreeam_reset_stats();
    BTREEAMStats stats;
    btreeam_get_stats(&stats);

    printf("[Benchmark] BTree stats: insertions=%lu, pages_allocated=%lu\n",
           (unsigned long)stats.insertions, (unsigned long)stats.pages_allocated);

    if (stats.pages_allocated > 0) {
        double bytes_per_key = (stats.pages_allocated * 8192.0) / num_keys;
        printf("[Benchmark] Space per key: %.2f bytes\n", bytes_per_key);
    }

    btdestroy(rel);
    relation_close(rel, 0);

    GTEST_SUCCEED();
}

/**
 * @brief WAL 空间开销测试
 */
TEST_F(StorageSpaceBenchmarkTest, WALSpaceOverhead) {
    const char *wal_file = "benchmark_wal.db";
    std::remove(wal_file);

    wal_t *wal = wal_create(wal_file, 8192);
    ASSERT_NE(nullptr, wal);

    constexpr int num_records = 1000;
    for (int i = 0; i < num_records; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        wal_write_insert(wal, 1, key, strlen(key), value, strlen(value));
    }

    wal_stats_t stats;
    wal_get_stats(wal, &stats);

    printf("[Benchmark] WAL stats: records=%lu, bytes=%lu\n",
           (unsigned long)stats.total_records, (unsigned long)stats.total_bytes);

    if (stats.total_records > 0) {
        double bytes_per_record = (double)stats.total_bytes / stats.total_records;
        printf("[Benchmark] WAL overhead per record: %.2f bytes\n", bytes_per_record);
    }

    wal_close(wal);
    std::remove(wal_file);

    GTEST_SUCCEED();
}
