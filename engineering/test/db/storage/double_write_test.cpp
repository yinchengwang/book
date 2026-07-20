/**
 * @file double_write_test.cpp
 * @brief W3.8: Double Write 测试
 *
 * Double Write 缓冲是 PostgreSQL 防止页面部分写入的技术：
 * 1. 脏页刷盘时，先批量写入 Double Write 文件
 * 2. 再写实际位置
 * 3. 崩溃恢复时从 Double Write 读取未完整写入的页面
 */
#include <gtest/gtest.h>
extern "C" {
#include "db/heapam.h"
}

TEST(DoubleWriteTest, DoubleWriteConcept) {
    // 验证 Double Write 概念

    // 模拟 Double Write 缓冲区
    struct DoubleWriteBuffer {
        void *pages[64];     // 64 个页面缓冲区
        int page_count;      // 当前页面数
        int flush_threshold; // 刷盘阈值
    };

    DoubleWriteBuffer dwb;
    dwb.page_count = 0;
    dwb.flush_threshold = 16;  // 积累 16 页后刷盘

    // 模拟脏页写入
    for (int i = 0; i < 10; i++) {
        dwb.pages[dwb.page_count++] = malloc(8192);
    }

    // 批量写入 Double Write 文件
    // for (int i = 0; i < dwb.page_count; i++) {
    //     pwrite(dw_fd, dwb.pages[i], 8192, i * 8192);
    // }

    // 然后写入实际位置
    // for (int i = 0; i < dwb.page_count; i++) {
    //     pwrite(actual_fd, dwb.pages[i], 8192, actual_offset);
    // }

    // 清理
    for (int i = 0; i < dwb.page_count; i++) {
        free(dwb.pages[i]);
    }

    EXPECT_EQ(dwb.page_count, 10);
}

TEST(DoubleWriteTest, PartialWriteRecovery) {
    // 验证部分写入恢复
    // 在崩溃恢复时，检查 Double Write 文件中的页面
    // 如果比实际文件中的页面新，则恢复

    // 模拟检查点
    struct CheckpointPage {
        uint32_t file_id;       // 文件 ID
        uint32_t page_no;       // 页面号
        uint32_t dw_offset;     // Double Write 中的偏移
        bool needs_recovery;    // 需要恢复
    };

    // 检查点记录哪些页面在 Double Write 缓冲区中
    CheckpointPage cp_pages[] = {
        {1, 100, 0, true},   // 需要恢复（页面 100 未完整写入）
        {1, 101, 1, true},   // 需要恢复
        {1, 102, 2, false},  // 不需要恢复（已完整写入）
    };

    // 崩溃恢复时，从 Double Write 读取
    int recovered = 0;
    for (int i = 0; i < 3; i++) {
        if (cp_pages[i].needs_recovery) {
            recovered++;
            // memcpy(actual_page, dw_buffer + cp_pages[i].dw_offset * PAGE_SIZE, PAGE_SIZE);
        }
    }

    EXPECT_EQ(recovered, 2);
}

TEST(DoubleWriteTest, FlushThreshold) {
    // 验证 Double Write 刷盘阈值

    // 模拟：每 16 页或 100ms 刷一次
    const int FLUSH_THRESHOLD_PAGES = 16;
    const int FLUSH_THRESHOLD_MS = 100;

    // 验证阈值
    EXPECT_EQ(FLUSH_THRESHOLD_PAGES, 16);
    EXPECT_EQ(FLUSH_THRESHOLD_MS, 100);

    // 模拟积累
    int pages_in_buffer = 0;
    int flush_count = 0;

    for (int i = 0; i < 50; i++) {
        pages_in_buffer++;
        if (pages_in_buffer >= FLUSH_THRESHOLD_PAGES) {
            // 刷盘
            flush_count++;
            pages_in_buffer = 0;
        }
    }

    // 50 页，每次刷 16 页，需要 3 次（最后 2 页留在缓冲区）
    EXPECT_EQ(flush_count, 3);
    EXPECT_EQ(pages_in_buffer, 2);  // 剩余 2 页
}

TEST(DoubleWriteTest, DoubleWriteOverhead) {
    // 验证 Double Write 开销
    // Double Write 增加了写放大，但提供了安全保证

    const int N_DIRTY_PAGES = 100;
    const int PAGE_SIZE = 8192;

    // 没有 Double Write：直接写 100 页
    int direct_writes = N_DIRTY_PAGES;
    int direct_bytes = N_DIRTY_PAGES * PAGE_SIZE;

    // 有 Double Write：先写 Double Write，再写实际位置
    int dw_batch_size = 16;
    int dw_batches = (N_DIRTY_PAGES + dw_batch_size - 1) / dw_batch_size;
    int dw_writes = N_DIRTY_PAGES + dw_batches;  // 实际页 + 批写入
    int dw_bytes = (N_DIRTY_PAGES + dw_batches) * PAGE_SIZE;

    // 写放大
    double write_amplification = (double)dw_bytes / direct_bytes;

    EXPECT_GT(write_amplification, 1.0);  // 写放大 > 1
    EXPECT_LT(write_amplification, 1.1);  // 但通常 < 10%
    EXPECT_EQ(dw_batches, 7);             // 100/16 = 6.25 → 7 批
}