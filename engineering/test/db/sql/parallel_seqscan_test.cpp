/**
 * @file parallel_seqscan_test.cpp
 * @brief W4.2: 并行 SeqScan 测试
 *
 * 测试 ParallelScanState 共享页面范围计数器、多 worker 并行扫描。
 * 不需要实际存储引擎，使用模拟的页面范围。
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/sql/parallel.h"
}

#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>

namespace {

/**
 * @brief 测试 ParallelScanState 创建
 */
TEST(ParallelSeqScanTest, CreateDestroy) {
    ParallelScanState *pstate = CreateParallelScanState((void*)0x1, 100, 4);
    ASSERT_NE(pstate, nullptr);
    EXPECT_EQ(pstate->total_pages, 100);
    EXPECT_EQ(pstate->nworkers, 4);
    EXPECT_EQ(pstate->next_page, 0);
    EXPECT_EQ(pstate->pages_scanned, 0);

    DestroyParallelScanState(pstate);
}

/**
 * @brief 测试 NULL 销毁安全
 */
TEST(ParallelSeqScanTest, NullDestroy) {
    DestroyParallelScanState(nullptr);
}

/**
 * @brief 测试无效参数
 */
TEST(ParallelSeqScanTest, InvalidParams) {
    ParallelScanState *pstate;

    pstate = CreateParallelScanState((void*)0x1, 0, 4);
    EXPECT_EQ(pstate, nullptr);

    pstate = CreateParallelScanState((void*)0x1, 100, 0);
    EXPECT_EQ(pstate, nullptr);

    pstate = CreateParallelScanState((void*)0x1, -1, 4);
    EXPECT_EQ(pstate, nullptr);
}

/**
 * @brief 测试单线程页面分配
 */
TEST(ParallelSeqScanTest, SingleThreadPageAllocation) {
    ParallelScanState *pstate = CreateParallelScanState((void*)0x1, 10, 1);
    ASSERT_NE(pstate, nullptr);

    /* 单线程获取所有页面号 */
    std::vector<int> pages;
    int page;
    while ((page = ParallelScanNextPage(pstate)) >= 0) {
        pages.push_back(page);
    }

    EXPECT_EQ(pages.size(), 10);
    EXPECT_EQ(pstate->pages_scanned, 10);

    /* 验证页面号连续 */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(pages[i], i);
    }

    /* 超出范围后返回 -1 */
    EXPECT_EQ(ParallelScanNextPage(pstate), -1);

    DestroyParallelScanState(pstate);
}

/**
 * @brief 测试多线程并行页面分配
 *
 * 4 个 worker 共享 100 页，每个 worker 通过 ParallelScanNextPage
 * 原子获取页面号，验证总扫描页数 = 100。
 */
TEST(ParallelSeqScanTest, MultiThreadPageAllocation) {
    const int N_WORKERS = 4;
    const int N_PAGES = 100;

    ParallelScanState *pstate = CreateParallelScanState(
        (void*)0x1, N_PAGES, N_WORKERS);
    ASSERT_NE(pstate, nullptr);

    std::atomic<int> total_scanned{0};
    std::vector<std::thread> workers;

    for (int w = 0; w < N_WORKERS; w++) {
        workers.emplace_back([pstate, &total_scanned]() {
            int scanned = 0;
            int page;
            while ((page = ParallelScanNextPage(pstate)) >= 0) {
                scanned++;
                /* 模拟页面扫描 */
            }
            total_scanned += scanned;
            ParallelScanWorkerFinished(pstate);
        });
    }

    for (auto &t : workers) {
        t.join();
    }

    /* 验证总扫描页数 */
    EXPECT_EQ(total_scanned, N_PAGES);
    EXPECT_EQ(pstate->pages_scanned, N_PAGES);
    EXPECT_TRUE(ParallelScanAllFinished(pstate));

    DestroyParallelScanState(pstate);
}

/**
 * @brief 测试多线程大页面分配
 *
 * 验证 1000 页，4 个 worker 的并行分配正确性。
 */
TEST(ParallelSeqScanTest, MultiThreadLargeAllocation) {
    const int N_WORKERS = 4;
    const int N_PAGES = 1000;

    ParallelScanState *pstate = CreateParallelScanState(
        (void*)0x1, N_PAGES, N_WORKERS);
    ASSERT_NE(pstate, nullptr);

    std::atomic<int> total_scanned{0};
    std::vector<std::thread> workers;

    for (int w = 0; w < N_WORKERS; w++) {
        workers.emplace_back([pstate, &total_scanned]() {
            int scanned = 0;
            while (ParallelScanNextPage(pstate) >= 0) {
                scanned++;
            }
            total_scanned += scanned;
            ParallelScanWorkerFinished(pstate);
        });
    }

    for (auto &t : workers) {
        t.join();
    }

    EXPECT_EQ(total_scanned, N_PAGES);
    EXPECT_TRUE(ParallelScanAllFinished(pstate));

    DestroyParallelScanState(pstate);
}

/**
 * @brief 测试页面分配不重叠
 *
 * 验证每个页面号只被分配一次。
 */
TEST(ParallelSeqScanTest, NoOverlapAllocation) {
    const int N_WORKERS = 4;
    const int N_PAGES = 50;

    ParallelScanState *pstate = CreateParallelScanState(
        (void*)0x1, N_PAGES, N_WORKERS);
    ASSERT_NE(pstate, nullptr);

    /* 使用 mutex 保护的集合跟踪所有页面号 */
    std::mutex mtx;
    std::vector<int> all_pages;

    std::vector<std::thread> workers;
    for (int w = 0; w < N_WORKERS; w++) {
        workers.emplace_back([pstate, &mtx, &all_pages]() {
            std::vector<int> local_pages;
            int page;
            while ((page = ParallelScanNextPage(pstate)) >= 0) {
                local_pages.push_back(page);
            }
            {
                std::lock_guard<std::mutex> lock(mtx);
                all_pages.insert(all_pages.end(),
                    local_pages.begin(), local_pages.end());
            }
            ParallelScanWorkerFinished(pstate);
        });
    }

    for (auto &t : workers) {
        t.join();
    }

    /* 验证无重复页面号 */
    EXPECT_EQ(all_pages.size(), N_PAGES);
    std::sort(all_pages.begin(), all_pages.end());
    for (int i = 0; i < N_PAGES; i++) {
        EXPECT_EQ(all_pages[i], i);
    }

    DestroyParallelScanState(pstate);
}

/**
 * @brief 测试 worker 数量多于页面数
 */
TEST(ParallelSeqScanTest, MoreWorkersThanPages) {
    const int N_WORKERS = 8;
    const int N_PAGES = 3;

    ParallelScanState *pstate = CreateParallelScanState(
        (void*)0x1, N_PAGES, N_WORKERS);
    ASSERT_NE(pstate, nullptr);

    std::atomic<int> total_scanned{0};
    std::vector<std::thread> workers;

    for (int w = 0; w < N_WORKERS; w++) {
        workers.emplace_back([pstate, &total_scanned]() {
            int scanned = 0;
            while (ParallelScanNextPage(pstate) >= 0) {
                scanned++;
            }
            total_scanned += scanned;
            ParallelScanWorkerFinished(pstate);
        });
    }

    for (auto &t : workers) {
        t.join();
    }

    /* 只有 3 页可扫描 */
    EXPECT_EQ(total_scanned, N_PAGES);
    EXPECT_TRUE(ParallelScanAllFinished(pstate));

    DestroyParallelScanState(pstate);
}

/**
 * @brief 测试并行扫描状态完成检测
 */
TEST(ParallelSeqScanTest, AllFinishedDetection) {
    ParallelScanState *pstate = CreateParallelScanState(
        (void*)0x1, 10, 2);
    ASSERT_NE(pstate, nullptr);

    /* 初始状态：未完成 */
    EXPECT_FALSE(ParallelScanAllFinished(pstate));

    /* 一个 worker 完成 */
    ParallelScanWorkerFinished(pstate);
    EXPECT_FALSE(ParallelScanAllFinished(pstate));

    /* 所有 worker 完成 */
    ParallelScanWorkerFinished(pstate);
    EXPECT_TRUE(ParallelScanAllFinished(pstate));

    DestroyParallelScanState(pstate);
}

}  // namespace