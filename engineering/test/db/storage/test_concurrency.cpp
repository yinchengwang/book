/**
 * @file test_concurrency.cpp
 * @brief 多模态存储引擎并发安全测试
 *
 * 测试原子操作和基础并发功能。
 */
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

/* ========================================================================
 * 原子操作测试
 * ======================================================================== */

class ConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(ConcurrencyTest, AtomicIncrement) {
    std::atomic<int> counter(0);

    const int num_threads = 10;
    const int increments_per_thread = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < increments_per_thread; j++) {
                counter.fetch_add(1);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), num_threads * increments_per_thread);
}

TEST_F(ConcurrencyTest, AtomicCompareExchange) {
    std::atomic<int> value(100);

    int expected = 100;
    bool result = std::atomic_compare_exchange_strong(&value, &expected, 200);
    EXPECT_TRUE(result);
    EXPECT_EQ(value.load(), 200);
    EXPECT_EQ(expected, 100);

    expected = 100;
    result = std::atomic_compare_exchange_strong(&value, &expected, 300);
    EXPECT_FALSE(result);
    EXPECT_EQ(value.load(), 200);
}

TEST_F(ConcurrencyTest, AtomicLoadStore) {
    std::atomic<int> value(0);

    value.store(42, std::memory_order_relaxed);
    EXPECT_EQ(value.load(std::memory_order_relaxed), 42);

    value.store(100, std::memory_order_seq_cst);
    EXPECT_EQ(value.load(std::memory_order_seq_cst), 100);
}

TEST_F(ConcurrencyTest, ConcurrentCounter) {
    std::atomic<uint64_t> total(0);

    const int num_producers = 8;
    const int items_per_producer = 1000;
    std::vector<std::thread> producers;

    for (int p = 0; p < num_producers; p++) {
        producers.emplace_back([&]() {
            for (int i = 0; i < items_per_producer; i++) {
                total.fetch_add(1);
            }
        });
    }

    for (auto &t : producers) {
        t.join();
    }

    EXPECT_EQ(total.load(), (uint64_t)num_producers * items_per_producer);
}

TEST_F(ConcurrencyTest, ReadWriteConflict) {
    std::atomic<int> shared_value(0);
    std::atomic<int> write_count(0);
    std::atomic<int> read_count(0);

    const int num_writers = 2;
    const int num_readers = 4;
    const int operations = 100;

    std::vector<std::thread> threads;

    for (int w = 0; w < num_writers; w++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < operations; i++) {
                shared_value.store(i, std::memory_order_seq_cst);
                write_count.fetch_add(1);
            }
        });
    }

    for (int r = 0; r < num_readers; r++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < operations; i++) {
                volatile int v = shared_value.load(std::memory_order_seq_cst);
                (void)v;
                read_count.fetch_add(1);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(write_count.load(), num_writers * operations);
    EXPECT_EQ(read_count.load(), num_readers * operations);
}

TEST_F(ConcurrencyTest, SpinLockSimulation) {
    std::atomic<bool> locked(false);
    std::atomic<int> success_count(0);
    std::atomic<int> timeout_count(0);

    const int num_threads = 8;
    const int attempts = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < attempts; i++) {
                bool expected = false;
                if (locked.compare_exchange_strong(expected, true)) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    locked.store(false);
                    success_count.fetch_add(1);
                } else {
                    timeout_count.fetch_add(1);
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load() + timeout_count.load(), num_threads * attempts);
}

TEST_F(ConcurrencyTest, ReadWritePattern) {
    std::atomic<int> data(0);
    std::atomic<int> writes_total(0);
    std::atomic<int> reads_total(0);

    const int num_writers = 2;
    const int num_readers = 4;
    const int operations = 100;

    std::vector<std::thread> threads;

    for (int w = 0; w < num_writers; w++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < operations; i++) {
                data.store(i, std::memory_order_seq_cst);
                writes_total.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    for (int r = 0; r < num_readers; r++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < operations; i++) {
                volatile int v = data.load(std::memory_order_seq_cst);
                (void)v;
                reads_total.fetch_add(1);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(writes_total.load(), num_writers * operations);
    EXPECT_EQ(reads_total.load(), num_readers * operations);
}

TEST_F(ConcurrencyTest, SimpleBarrier) {
    std::atomic<int> counter(0);
    std::atomic<int> phase(0);

    auto worker = [&](int id) {
        for (int p = 0; p < 3; p++) {
            counter.fetch_add(1);
            if (id == 0) {
                phase.store(p + 1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), 12);  /* 4 threads * 3 phases */
    EXPECT_EQ(phase.load(), 3);
}
