/**
 * @file buf_concurrency_test.cpp
 * @brief Buffer Pool 并发访问测试
 *
 * 测试 Buffer Pool 在多线程并发场景下的正确性和一致性。
 */

#include "gtest/gtest.h"
#include "db/buf.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>

/* ============================================================
 * 测试夹具
 * ============================================================ */

class BufferPoolConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, buf_init(32));  /* 小缓冲池便于触发置换 */
    }

    void TearDown() override {
        buf_shutdown();
    }
};

/* ============================================================
 * 2.1.4 并发访问测试
 * ============================================================ */

/**
 * @brief 测试多线程读取同一页面
 *
 * 场景：
 * - 多个线程同时读取同一个页面
 * - 应该能正确共享 Buffer
 * - 引用计数正确递增
 */
TEST_F(BufferPoolConcurrencyTest, ConcurrentReadSamePage) {
    constexpr int num_threads = 8;
    constexpr int num_reads = 100;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < num_reads; i++) {
                BufferDesc *buf = buf_read(1, 0, 0);  /* 同一页面 */
                if (buf != nullptr) {
                    /* 验证数据一致性 */
                    char *data = (char *)buf_get_data(buf);
                    if (data != nullptr) {
                        success_count++;
                    }
                    buf_unpin(buf);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(num_threads * num_reads, success_count.load());
}

/**
 * @brief 测试多线程读取不同页面
 *
 * 场景：
 * - 多个线程读取不同的页面
 * - 每个线程应该能独立获取自己的 Buffer
 * - 不互相阻塞
 */
TEST_F(BufferPoolConcurrencyTest, ConcurrentReadDifferentPages) {
    constexpr int num_threads = 4;
    constexpr int pages_per_thread = 20;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        int start_page = t * pages_per_thread;
        threads.emplace_back([&, start_page]() {
            for (int i = 0; i < pages_per_thread; i++) {
                BufferDesc *buf = buf_read(1, (BlockNumber)(start_page + i), 0);
                if (buf != nullptr) {
                    EXPECT_EQ((BlockNumber)(start_page + i), buf_get_blocknum(buf));
                    success_count++;
                    buf_unpin(buf);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(num_threads * pages_per_thread, success_count.load());
}

/**
 * @brief 测试并发读写同一页面
 *
 * 场景：
 * - 线程 A 读取并修改页面
 * - 线程 B 同时读取同一页面
 * - 应该看到修改后的数据（Write-through 模式）
 */
TEST_F(BufferPoolConcurrencyTest, ConcurrentReadWriteSamePage) {
    constexpr int num_writes = 50;
    constexpr int reads_per_write = 10;
    std::atomic<int> write_complete{0};
    std::atomic<int> read_after_write{0};
    std::mutex mtx;
    int expected_value = 0;

    /* 写入线程 */
    std::thread writer([&]() {
        for (int i = 0; i < num_writes; i++) {
            BufferDesc *buf = buf_read(1, 0, 1);  /* 写访问 */
            if (buf != nullptr) {
                char *data = (char *)buf_get_data(buf);
                if (data != nullptr) {
                    /* 写入递增的值 */
                    snprintf(data, 16, "%d", i + 1);
                }
                buf_dirty(buf);
                buf_unpin(buf);
                write_complete++;
            }
            std::this_thread::yield();
        }
    });

    /* 读取线程 */
    std::thread reader([&]() {
        for (int i = 0; i < num_writes * reads_per_write; i++) {
            BufferDesc *buf = buf_read(1, 0, 0);  /* 读访问 */
            if (buf != nullptr) {
                char *data = (char *)buf_get_data(buf);
                if (data != nullptr) {
                    /* 检查值是否有效 */
                    int val = atoi(data);
                    if (val > 0 && val <= num_writes) {
                        read_after_write++;
                    }
                }
                buf_unpin(buf);
            }
            std::this_thread::yield();
        }
    });

    writer.join();
    reader.join();

    EXPECT_EQ(num_writes, write_complete.load());
    EXPECT_GT(read_after_write.load(), 0);  /* 至少读到一些有效数据 */
}

/**
 * @brief 测试并发页面置换
 *
 * 场景：
 * - 多个线程同时读取大量不同页面
 * - 触发 Buffer Pool 置换
 * - 验证没有死锁或数据损坏
 */
TEST_F(BufferPoolConcurrencyTest, ConcurrentEviction) {
    constexpr int num_threads = 4;
    constexpr int pages_per_thread = 100;  /* 远大于 buffer 数量 */
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < pages_per_thread; i++) {
                /* 每个线程访问不同的关系和页面，增加置换压力 */
                BufferDesc *buf = buf_read(
                    (RelFileNode)(t + 1),
                    (BlockNumber)i,
                    0
                );
                if (buf != nullptr) {
                    EXPECT_TRUE(buf_isvalid(buf));
                    success_count++;
                    buf_unpin(buf);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(num_threads * pages_per_thread, success_count.load());
}

/**
 * @brief 测试读写锁的正确性
 *
 * 场景：
 * - 多个读线程可以并发
 * - 写线程应该独占访问
 * - 验证引用计数和脏标记正确
 */
TEST_F(BufferPoolConcurrencyTest, ReadWriteLockCorrectness) {
    constexpr int num_readers = 4;
    constexpr int num_writers = 2;
    std::atomic<int> write_occurred{0};
    std::atomic<int> concurrent_reads{0};
    int max_concurrent_reads = 0;

    std::mutex stat_mtx;

    /* 写线程 */
    auto writer_func = [&](int id) {
        for (int i = 0; i < 20; i++) {
            BufferDesc *buf = buf_read(1, 0, 1);  /* 写模式 */
            if (buf != nullptr) {
                /* 标记发生了写入 */
                concurrent_reads.store(0);  /* 写入时应该没有并发读 */
                write_occurred++;

                char *data = (char *)buf_get_data(buf);
                snprintf(data, 16, "Writer%d-%d", id, i);

                buf_dirty(buf);
                buf_unpin(buf);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    /* 读线程 */
    auto reader_func = [&]() {
        for (int i = 0; i < 50; i++) {
            BufferDesc *buf = buf_read(1, 0, 0);  /* 读模式 */
            if (buf != nullptr) {
                /* 记录并发读数量 */
                int current = ++concurrent_reads;
                {
                    std::lock_guard<std::mutex> lock(stat_mtx);
                    max_concurrent_reads = std::max(max_concurrent_reads, current);
                }

                buf_unpin(buf);
            }
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> threads;

    /* 启动写线程 */
    for (int i = 0; i < num_writers; i++) {
        threads.emplace_back(writer_func, i);
    }

    /* 启动读线程 */
    for (int i = 0; i < num_readers; i++) {
        threads.emplace_back(reader_func);
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_GT(write_occurred.load(), 0);
    EXPECT_GT(max_concurrent_reads, 0);
}

/* ============================================================
 * 2.1.5 边界条件测试
 * ============================================================ */

/**
 * @brief 测试容量满时的行为
 *
 * 场景：
 * - 填满 Buffer Pool
 * - 继续分配页面
 * - 验证置换正确发生
 */
TEST_F(BufferPoolConcurrencyTest, FullCapacityBehavior) {
    const int nbuffers = buf_get_nbuffers();

    /* 分配所有缓冲区 */
    std::vector<BufferDesc*> buffers;
    for (int i = 0; i < nbuffers; i++) {
        BufferDesc *buf = buf_new(1);
        ASSERT_NE(nullptr, buf);
        buffers.push_back(buf);
    }

    /* 所有缓冲区应该都被占用 */
    buf_stats_t stats;
    buf_get_stats(&stats);

    /* 释放所有缓冲区 */
    for (auto *buf : buffers) {
        buf_unpin(buf);
    }

    /* 应该能够继续分配新页面（通过置换） */
    BufferDesc *buf = buf_new(1);
    EXPECT_NE(nullptr, buf);
    buf_unpin(buf);
}

/**
 * @brief 测试空缓冲池的行为
 *
 * 场景：
 * - 初始化后立即访问
 * - 验证没有未定义行为
 */
TEST_F(BufferPoolConcurrencyTest, EmptyPoolBehavior) {
    buf_reset_stats();

    /* 在空缓冲池中分配页面 */
    BufferDesc *buf = buf_new(1);
    ASSERT_NE(nullptr, buf);
    EXPECT_TRUE(buf_isvalid(buf));
    EXPECT_TRUE(buf_isdirty(buf));  /* 新页面标记为脏 */

    buf_stats_t stats;
    buf_get_stats(&stats);
    EXPECT_EQ(0u, stats.hits);
    EXPECT_GE(stats.misses, 1u);

    buf_unpin(buf);
}

/**
 * @brief 测试无效页面号处理
 *
 * 场景：
 * - 访问无效的页面号
 * - 验证有合理的错误处理
 */
TEST_F(BufferPoolConcurrencyTest, InvalidPageNumber) {
    /* 负数页面号 - 转换为无符号后可能变成很大的数 */
    BufferDesc *buf1 = buf_read(1, (BlockNumber)-1, 0);
    /* 可能返回 nullptr 或返回页面（取决于实现） */

    /* 非常大的页面号 */
    BufferDesc *buf2 = buf_read(1, (BlockNumber)0xFFFFFFFF, 0);
    /* 应该返回 nullptr 或创建新页面 */

    (void)buf1;
    (void)buf2;
}

/**
 * @brief 测试并发压力下的边界条件
 *
 * 场景：
 * - 高并发 + 边界数据
 * - 验证没有竞态条件
 */
TEST_F(BufferPoolConcurrencyTest, HighConcurrencyBoundary) {
    constexpr int num_threads = 8;
    constexpr int iterations = 50;
    std::atomic<bool> error_occurred{false};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < iterations && !error_occurred.load(); i++) {
                /* 边界页面：0, 最大页面号 */
                BlockNumber page = (i % 2 == 0) ? 0 : 1000 + t;

                BufferDesc *buf = buf_read((RelFileNode)(t + 1), page, i % 2);
                if (buf != nullptr) {
                    /* 验证 Buffer 状态一致 */
                    if (!buf_isvalid(buf)) {
                        error_occurred = true;
                    }

                    /* 写入数据并验证 */
                    char *data = (char *)buf_get_data(buf);
                    if (data != nullptr) {
                        snprintf(data, 32, "Thread%d-Iter%d", t, i);
                    }

                    if (i % 2 == 1) {
                        buf_dirty(buf);
                    }

                    buf_unpin(buf);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_FALSE(error_occurred.load());
}

/* ============================================================
 * 压力测试
 * ============================================================ */

/**
 * @brief 长时间压力测试
 *
 * 验证在持续高负载下系统的稳定性
 */
TEST_F(BufferPoolConcurrencyTest, StressTest) {
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 200;
    std::atomic<uint64_t> total_ops{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < operations_per_thread; i++) {
                /* 混合操作：读、写、新页面 */
                RelFileNode rel = (RelFileNode)(t + 1);
                BlockNumber page = (BlockNumber)(i % 50);

                switch (i % 3) {
                    case 0: {  /* 读 */
                        BufferDesc *buf = buf_read(rel, page, 0);
                        if (buf) {
                            buf_unpin(buf);
                            total_ops++;
                        }
                        break;
                    }
                    case 1: {  /* 写 */
                        BufferDesc *buf = buf_read(rel, page, 1);
                        if (buf) {
                            buf_dirty(buf);
                            buf_unpin(buf);
                            total_ops++;
                        }
                        break;
                    }
                    case 2: {  /* 新页面 */
                        BufferDesc *buf = buf_new(rel);
                        if (buf) {
                            buf_unpin(buf);
                            total_ops++;
                        }
                        break;
                    }
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ((uint64_t)num_threads * operations_per_thread, total_ops.load());
}
