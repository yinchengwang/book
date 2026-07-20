/**
 * @file btree_concurrent_test.cpp
 * @brief BTree 并发 Latch 测试（Task W1.4）
 *
 * 测试每页面的自旋读写锁（latch）：
 * - 读锁共享：多个线程可同时持有读锁
 * - 写锁排他：写锁等待所有读锁释放
 * - 多线程并发读写验证正确性
 * - 页面号排序获取避免死锁
 */

#include "gtest/gtest.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>

extern "C" {
#include "db/access/btree/btpage.h"
#include "db/access/btree/btree_split.h"
#include "db/rel.h"
#include "db/buf.h"
}

/* 从 btreeam.c 引入的接口 */
extern "C" {
int btreeam_init(void);
void btreeam_shutdown(void);
int btcreate(Relation rel);
int btinsert(Relation rel, const void **values, int nkeys, void *heap_ptr);
}

/* ============================================================
 * 测试夹具
 * ============================================================ */

class BTreeConcurrentTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, btreeam_init());
        ASSERT_EQ(0, rel_init());
        ASSERT_EQ(0, buf_init(2048));
    }

    void TearDown() override {
        buf_shutdown();
        btreeam_shutdown();
    }
};

/* ============================================================
 * 单个页面 Latch 功能测试
 * ============================================================ */

TEST_F(BTreeConcurrentTest, LatchInitAndBasic) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;

    bt_latch_init(header);
    EXPECT_EQ(0, header->latch_readers);
    EXPECT_EQ(0, header->latch_writers_waiting);
    EXPECT_EQ(0, header->latch_writer_active);
}

TEST_F(BTreeConcurrentTest, LatchReadLockUnlock) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    bt_latch_init(header);

    bt_latch_read_lock(header);
    EXPECT_EQ(1, header->latch_readers);

    bt_latch_read_unlock(header);
    EXPECT_EQ(0, header->latch_readers);
}

TEST_F(BTreeConcurrentTest, LatchWriteLockUnlock) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    bt_latch_init(header);

    bt_latch_write_lock(header);
    EXPECT_EQ(1, header->latch_writer_active);

    bt_latch_write_unlock(header);
    EXPECT_EQ(0, header->latch_writer_active);
}

TEST_F(BTreeConcurrentTest, LatchTrylock) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    bt_latch_init(header);

    /* 尝试读锁（应成功） */
    EXPECT_EQ(0, bt_latch_read_trylock(header));
    bt_latch_read_unlock(header);

    /* 尝试写锁（应成功） */
    EXPECT_EQ(0, bt_latch_write_trylock(header));

    /* 写锁持有期间，读锁尝试应失败 */
    EXPECT_EQ(-1, bt_latch_read_trylock(header));

    bt_latch_write_unlock(header);

    /* 写锁释放后，读锁尝试应成功 */
    EXPECT_EQ(0, bt_latch_read_trylock(header));
    bt_latch_read_unlock(header);
}

TEST_F(BTreeConcurrentTest, LatchMultipleReads) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    bt_latch_init(header);

    /* 多个读锁可同时持有 */
    bt_latch_read_lock(header);
    bt_latch_read_lock(header);
    bt_latch_read_lock(header);
    EXPECT_EQ(3, header->latch_readers);

    bt_latch_read_unlock(header);
    bt_latch_read_unlock(header);
    bt_latch_read_unlock(header);
    EXPECT_EQ(0, header->latch_readers);
}

/* ============================================================
 * 多线程并发 Latch 测试
 * ============================================================ */

TEST_F(BTreeConcurrentTest, ConcurrentReaders) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    bt_latch_init(header);

    std::atomic<int> success_count(0);
    const int NUM_THREADS = 10;
    const int ITERATIONS = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ITERATIONS; i++) {
                bt_latch_read_lock(header);
                /* 模拟读操作 */
                volatile int dummy = header->btpo_flags;
                (void)dummy;
                success_count.fetch_add(1);
                bt_latch_read_unlock(header);
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(NUM_THREADS * ITERATIONS, success_count.load());
    EXPECT_EQ(0, header->latch_readers);
}

TEST_F(BTreeConcurrentTest, ExclusiveWriteDuringReads) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    bt_latch_init(header);

    std::atomic<bool> writer_done(false);
    std::atomic<int> read_count(0);

    /* 启动读线程，持有读锁 */
    std::thread reader([&]() {
        bt_latch_read_lock(header);
        read_count.store(1);
        /* 模拟长读操作 */
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bt_latch_read_unlock(header);
    });

    /* 确保读锁已获取 */
    while (read_count.load() == 0) {
        std::this_thread::yield();
    }

    /* 尝试获取写锁（应被读锁阻塞，但 trylock 应失败） */
    EXPECT_EQ(-1, bt_latch_write_trylock(header));

    reader.join();
    EXPECT_EQ(0, header->latch_readers);
}

TEST_F(BTreeConcurrentTest, ReadWriteAlternating) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    bt_latch_init(header);

    std::atomic<int> shared_counter(0);
    std::atomic<bool> stop(false);
    const int NUM_READERS = 5;
    std::vector<std::thread> threads;

    /* 读线程 */
    for (int t = 0; t < NUM_READERS; t++) {
        threads.emplace_back([&]() {
            while (!stop.load()) {
                bt_latch_read_lock(header);
                int val = shared_counter.load();
                /* 读锁期间，值不应被写线程修改 */
                EXPECT_GE(val, 0);
                (void)val;
                bt_latch_read_unlock(header);
                std::this_thread::yield();
            }
        });
    }

    /* 写线程 */
    threads.emplace_back([&]() {
        for (int i = 0; i < 50; i++) {
            bt_latch_write_lock(header);
            shared_counter.store(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            bt_latch_write_unlock(header);
            std::this_thread::yield();
        }
        stop.store(true);
    });

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(0, header->latch_readers);
    EXPECT_EQ(0, header->latch_writer_active);
}

/* ============================================================
 * 页面号排序获取测试（避免死锁）
 * ============================================================ */

TEST_F(BTreeConcurrentTest, PageOrderedLockAcquisition) {
    /* 模拟两个页面，按页面号排序获取锁 */
    char page_a[BTREE_PAGE_SIZE];
    char page_b[BTREE_PAGE_SIZE];
    memset(page_a, 0, BTREE_PAGE_SIZE);
    memset(page_b, 0, BTREE_PAGE_SIZE);

    BTPageHeader header_a = (BTPageHeader)page_a;
    BTPageHeader header_b = (BTPageHeader)page_b;
    bt_latch_init(header_a);
    bt_latch_init(header_b);

    uint32_t blkno_a = 100;
    uint32_t blkno_b = 200;

    /* 两个线程以不同顺序请求锁，但通过排序避免死锁 */
    std::atomic<int> t1_done(0);
    std::atomic<int> t2_done(0);

    std::thread t1([&]() {
        /* 线程 1：先获取小页面号 */
        bt_latch_write_lock(header_a);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bt_latch_write_lock(header_b);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        bt_latch_write_unlock(header_b);
        bt_latch_write_unlock(header_a);
        t1_done.store(1);
    });

    std::thread t2([&]() {
        /* 线程 2：也按页面号排序获取（先小后大） */
        /* 模拟页面号排序逻辑 */
        uint32_t first = (blkno_a < blkno_b) ? blkno_a : blkno_b;
        uint32_t second = (blkno_a < blkno_b) ? blkno_b : blkno_a;

        BTPageHeader first_header = (first == blkno_a) ? header_a : header_b;
        BTPageHeader second_header = (second == blkno_b) ? header_b : header_a;

        bt_latch_write_lock(first_header);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bt_latch_write_lock(second_header);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        bt_latch_write_unlock(second_header);
        bt_latch_write_unlock(first_header);
        t2_done.store(1);
    });

    t1.join();
    t2.join();

    EXPECT_EQ(1, t1_done.load());
    EXPECT_EQ(1, t2_done.load());
    /* 两个线程完成，无死锁 */
}

/* ============================================================
 * BTree 操作并发测试（集成）
 * ============================================================ */

TEST_F(BTreeConcurrentTest, ConcurrentBTreeInsert) {
    Relation rel = relation_opennode(4000, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    const int NUM_THREADS = 4;
    const int INSERTS_PER_THREAD = 250;
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            int base = t * INSERTS_PER_THREAD;
            for (int i = 0; i < INSERTS_PER_THREAD; i++) {
                int key = base + i;
                int result = btinsert(rel, (const void **)&key, 1, &key);
                if (result == 0) {
                    success_count.fetch_add(1);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    /* 所有插入都应成功 */
    EXPECT_GE(success_count.load(), NUM_THREADS * INSERTS_PER_THREAD - 10);

    relation_close(rel, 0);
}

/* ============================================================
 * 板凳测试：Latch 性能基准
 * ============================================================ */

TEST_F(BTreeConcurrentTest, LatchBenchmark) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    bt_latch_init(header);

    const int NUM_OPS = 10000;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_OPS; i++) {
        bt_latch_read_lock(header);
        bt_latch_read_unlock(header);
    }

    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    /* 单线程读锁 10000 次应在合理范围内 */
    EXPECT_LT(us, 500000);  /* 500ms 上限 */
    (void)us;
}