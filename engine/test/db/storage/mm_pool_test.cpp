/**
 * @file mm_pool_test.cpp
 * @brief 统一内存池测试
 *
 * 测试 Slab 分配器和 Arena 分配器的正确性和性能。
 */
#include "db/mm_pool.h"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <thread>
#include <atomic>

// 测试数据目录
static const char *TEST_DATA_DIR = "./test_mm_pool_data";

class MMPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保测试目录存在
#ifdef _WIN32
        _mkdir(TEST_DATA_DIR);
#else
        mkdir(TEST_DATA_DIR, 0755);
#endif
    }

    void TearDown() override {
        // 清理测试目录
    }
};

/* ========================================================================
 * Slab 分配器基本测试
 * ======================================================================== */

TEST_F(MMPoolTest, SlabCreateDestroy) {
    mm_pool_config_t config = {
        .type = MM_POOL_SLAB,
        .block_size = 256,
        .max_size = 0,
        .initial_size = 0,
        .name = "test_slab",
        .thread_safe = false
    };

    mm_pool_t *pool = mm_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    mm_pool_type_t type = mm_pool_get_type(pool);
    EXPECT_EQ(type, MM_POOL_SLAB);

    size_t block_size = mm_pool_get_block_size(pool);
    EXPECT_EQ(block_size, 256);

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, SlabDefaultCreate) {
    mm_pool_t *pool = mm_pool_create(NULL);
    ASSERT_NE(pool, nullptr);

    mm_pool_type_t type = mm_pool_get_type(pool);
    EXPECT_EQ(type, MM_POOL_SLAB);

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, SlabAllocFree) {
    mm_pool_t *pool = mm_pool_slab_create(256, 16, "test");
    ASSERT_NE(pool, nullptr);

    // 分配内存
    void *ptr = mm_pool_alloc(pool, 100);
    ASSERT_NE(ptr, nullptr);

    // 写入数据
    memset(ptr, 0xAB, 100);

    // 释放内存
    mm_pool_free(pool, ptr, 100);

    // 再次分配应该能成功
    void *ptr2 = mm_pool_alloc(pool, 100);
    ASSERT_NE(ptr2, nullptr);

    mm_pool_free(pool, ptr2, 100);
    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, SlabBlockAlignment) {
    mm_pool_t *pool = mm_pool_slab_create(512, 16, "test");
    ASSERT_NE(pool, nullptr);

    // 测试不同大小的分配
    std::vector<void*> ptrs;
    for (size_t size = 1; size <= 512; size += 127) {
        void *ptr = mm_pool_alloc(pool, size);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);

        // 验证内存可写
        memset(ptr, 0xCD, size);
    }

    // 释放所有内存
    for (void *ptr : ptrs) {
        mm_pool_free(pool, ptr, 1);
    }

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, SlabRealloc) {
    mm_pool_t *pool = mm_pool_slab_create(256, 16, "test");
    ASSERT_NE(pool, nullptr);

    // 初始分配
    void *ptr = mm_pool_alloc(pool, 100);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0x11, 100);

    // 重新分配
    void *ptr2 = mm_pool_realloc(pool, ptr, 100, 200);
    ASSERT_NE(ptr2, nullptr);

    // 验证旧数据被复制（部分）
    unsigned char *bytes = (unsigned char *)ptr2;
    EXPECT_EQ(bytes[0], 0x11);
    EXPECT_EQ(bytes[99], 0x11);

    // 新空间可写
    memset(ptr2, 0x22, 200);

    mm_pool_free(pool, ptr2, 200);
    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, SlabCalloc) {
    mm_pool_t *pool = mm_pool_slab_create(256, 16, "test");
    ASSERT_NE(pool, nullptr);

    // 分配零初始化内存
    int *arr = (int *)mm_pool_calloc(pool, 10, sizeof(int));
    ASSERT_NE(arr, nullptr);

    // 验证数据为零
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(arr[i], 0);
    }

    mm_pool_free(pool, arr, 10 * sizeof(int));
    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, SlabStats) {
    mm_pool_t *pool = mm_pool_slab_create(256, 16, "test");
    ASSERT_NE(pool, nullptr);

    // 分配一些内存
    void *ptr1 = mm_pool_alloc(pool, 100);
    void *ptr2 = mm_pool_alloc(pool, 200);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    // 获取统计
    mm_pool_stats_t stats;
    int ret = mm_pool_get_stats(pool, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_allocated, 256 * 16);  // 至少 16 个块（实际按 256 对齐）
    EXPECT_GE(stats.total_used, 512);  // 至少使用了 2 个块
    EXPECT_EQ(stats.num_allocations, 2);

    mm_pool_free(pool, ptr1, 100);
    mm_pool_free(pool, ptr2, 200);

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, SlabReset) {
    mm_pool_t *pool = mm_pool_slab_create(256, 16, "test");
    ASSERT_NE(pool, nullptr);

    // 分配一些内存
    for (int i = 0; i < 32; i++) {
        void *ptr = mm_pool_alloc(pool, 128);
        ASSERT_NE(ptr, nullptr);
    }

    // 重置
    mm_pool_reset(pool);

    // 再次分配应该成功
    void *ptr = mm_pool_alloc(pool, 256);
    ASSERT_NE(ptr, nullptr);

    mm_pool_destroy(pool);
}

/* ========================================================================
 * Arena 分配器基本测试
 * ======================================================================== */

TEST_F(MMPoolTest, ArenaCreateDestroy) {
    mm_pool_config_t config = {
        .type = MM_POOL_ARENA,
        .block_size = 1024 * 1024,  // 1MB chunk
        .max_size = 0,
        .initial_size = 0,
        .name = "test_arena",
        .thread_safe = false
    };

    mm_pool_t *pool = mm_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    mm_pool_type_t type = mm_pool_get_type(pool);
    EXPECT_EQ(type, MM_POOL_ARENA);

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaAllocFree) {
    mm_pool_t *pool = mm_pool_arena_create(1024 * 1024, 1024 * 1024, "test");
    ASSERT_NE(pool, nullptr);

    // 分配内存
    void *ptr = mm_pool_alloc(pool, 100);
    ASSERT_NE(ptr, nullptr);

    // 写入数据
    memset(ptr, 0xAB, 100);

    // 释放内存（Arena 延迟释放）
    mm_pool_free(pool, ptr, 100);

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaLargeAlloc) {
    mm_pool_t *pool = mm_pool_arena_create(1024 * 1024, 1024 * 1024, "test");
    ASSERT_NE(pool, nullptr);

    // 分配大块内存（超过 chunk 大小）
    void *ptr = mm_pool_alloc(pool, 2 * 1024 * 1024);
    ASSERT_NE(ptr, nullptr);

    // 写入数据
    memset(ptr, 0xCD, 2 * 1024 * 1024);

    mm_pool_free(pool, ptr, 2 * 1024 * 1024);
    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaRealloc) {
    mm_pool_t *pool = mm_pool_arena_create(1024 * 1024, 1024 * 1024, "test");
    ASSERT_NE(pool, nullptr);

    // 初始分配
    void *ptr = mm_pool_alloc(pool, 100);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0x11, 100);

    // 重新分配
    void *ptr2 = mm_pool_realloc(pool, ptr, 100, 200);
    ASSERT_NE(ptr2, nullptr);

    // 验证旧数据被复制
    unsigned char *bytes = (unsigned char *)ptr2;
    EXPECT_EQ(bytes[0], 0x11);
    EXPECT_EQ(bytes[99], 0x11);

    mm_pool_free(pool, ptr2, 200);
    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaStats) {
    mm_pool_t *pool = mm_pool_arena_create(1024 * 1024, 1024 * 1024, "test");
    ASSERT_NE(pool, nullptr);

    // 分配一些内存
    mm_pool_alloc(pool, 100);
    mm_pool_alloc(pool, 200);

    // 获取统计
    mm_pool_stats_t stats;
    int ret = mm_pool_get_stats(pool, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_allocated, 1024 * 1024);  // 至少 1 个 chunk
    EXPECT_GE(stats.total_used, 300);
    EXPECT_EQ(stats.num_allocations, 2);
    EXPECT_EQ(stats.fragmentation, 0.0);  // Arena 无碎片

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaReset) {
    mm_pool_t *pool = mm_pool_arena_create(1024 * 1024, 1024 * 1024, "test");
    ASSERT_NE(pool, nullptr);

    // 分配一些内存
    for (int i = 0; i < 100; i++) {
        mm_pool_alloc(pool, 1024);
    }

    // 重置
    mm_pool_reset(pool);

    // 再次分配应该成功
    void *ptr = mm_pool_alloc(pool, 1024 * 1024);
    ASSERT_NE(ptr, nullptr);

    mm_pool_destroy(pool);
}

/* ========================================================================
 * 便捷函数测试
 * ======================================================================== */

TEST_F(MMPoolTest, SlabCreateConvenience) {
    mm_pool_t *pool = mm_pool_slab_create(512, 32, "convenience_test");
    ASSERT_NE(pool, nullptr);

    EXPECT_EQ(mm_pool_get_type(pool), MM_POOL_SLAB);
    EXPECT_EQ(mm_pool_get_block_size(pool), 512);

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaCreateConvenience) {
    mm_pool_t *pool = mm_pool_arena_create(1024 * 1024, 2 * 1024 * 1024, "convenience_test");
    ASSERT_NE(pool, nullptr);

    EXPECT_EQ(mm_pool_get_type(pool), MM_POOL_ARENA);
    EXPECT_EQ(mm_pool_get_block_size(pool), 1024 * 1024);

    mm_pool_destroy(pool);
}

/* ========================================================================
 * 边界测试
 * ======================================================================== */

TEST_F(MMPoolTest, SlabZeroAlloc) {
    mm_pool_t *pool = mm_pool_slab_create(256, 16, "test");
    ASSERT_NE(pool, nullptr);

    // 分配 0 字节应该返回 NULL
    void *ptr = mm_pool_alloc(pool, 0);
    EXPECT_EQ(ptr, nullptr);

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaZeroAlloc) {
    mm_pool_t *pool = mm_pool_arena_create(1024 * 1024, 1024 * 1024, "test");
    ASSERT_NE(pool, nullptr);

    // 分配 0 字节应该返回 NULL
    void *ptr = mm_pool_alloc(pool, 0);
    EXPECT_EQ(ptr, nullptr);

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, NullPoolAlloc) {
    // 对 NULL 池分配应该返回 NULL
    void *ptr = mm_pool_alloc(NULL, 100);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(MMPoolTest, NullFree) {
    // 释放 NULL 指针应该安全
    mm_pool_free(NULL, NULL, 0);
}

TEST_F(MMPoolTest, SlabContains) {
    mm_pool_t *pool = mm_pool_slab_create(256, 16, "test");
    ASSERT_NE(pool, nullptr);

    void *ptr = mm_pool_alloc(pool, 100);
    ASSERT_NE(ptr, nullptr);

    // 指针属于池
    EXPECT_TRUE(mm_pool_contains(pool, ptr));

    // 不属于池的指针
    char dummy;
    EXPECT_FALSE(mm_pool_contains(pool, &dummy));

    mm_pool_free(pool, ptr, 100);
    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaContains) {
    mm_pool_t *pool = mm_pool_arena_create(1024 * 1024, 1024 * 1024, "test");
    ASSERT_NE(pool, nullptr);

    void *ptr = mm_pool_alloc(pool, 100);
    ASSERT_NE(ptr, nullptr);

    // 指针属于池
    EXPECT_TRUE(mm_pool_contains(pool, ptr));

    // 不属于池的指针
    char dummy;
    EXPECT_FALSE(mm_pool_contains(pool, &dummy));

    mm_pool_free(pool, ptr, 100);
    mm_pool_destroy(pool);
}

/* ========================================================================
 * 并发测试
 * ======================================================================== */

TEST_F(MMPoolTest, SlabConcurrentAlloc) {
    mm_pool_config_t config = {
        .type = MM_POOL_SLAB,
        .block_size = 256,
        .max_size = 0,
        .initial_size = 0,
        .name = "concurrent_test",
        .thread_safe = true  // 启用线程安全
    };

    mm_pool_t *pool = mm_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    const int num_threads = 4;
    const int allocs_per_thread = 100;
    std::vector<std::vector<void*>> thread_ptrs(num_threads);
    std::atomic<int> success_count(0);

    auto alloc_func = [&](int thread_id) {
        for (int i = 0; i < allocs_per_thread; i++) {
            void *ptr = mm_pool_alloc(pool, 128);
            if (ptr != nullptr) {
                memset(ptr, thread_id, 128);
                thread_ptrs[thread_id].push_back(ptr);
                success_count.fetch_add(1);
            }
        }
    };

    // 启动线程
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(alloc_func, i);
    }

    // 等待线程完成
    for (auto &t : threads) {
        t.join();
    }

    // 验证分配成功
    EXPECT_GT(success_count.load(), 0);

    // 释放所有内存
    for (auto &ptrs : thread_ptrs) {
        for (void *ptr : ptrs) {
            mm_pool_free(pool, ptr, 128);
        }
    }

    mm_pool_destroy(pool);
}

TEST_F(MMPoolTest, ArenaConcurrentAlloc) {
    mm_pool_config_t config = {
        .type = MM_POOL_ARENA,
        .block_size = 1024 * 1024,
        .max_size = 0,
        .initial_size = 1024 * 1024,
        .name = "concurrent_arena_test",
        .thread_safe = true  // 启用线程安全
    };

    mm_pool_t *pool = mm_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    const int num_threads = 4;
    const int allocs_per_thread = 50;
    std::vector<std::vector<void*>> thread_ptrs(num_threads);
    std::atomic<int> success_count(0);

    auto alloc_func = [&](int thread_id) {
        for (int i = 0; i < allocs_per_thread; i++) {
            void *ptr = mm_pool_alloc(pool, 1024);
            if (ptr != nullptr) {
                memset(ptr, thread_id, 1024);
                thread_ptrs[thread_id].push_back(ptr);
                success_count.fetch_add(1);
            }
        }
    };

    // 启动线程
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(alloc_func, i);
    }

    // 等待线程完成
    for (auto &t : threads) {
        t.join();
    }

    // 验证分配成功
    EXPECT_GT(success_count.load(), 0);

    // 释放所有内存
    for (auto &ptrs : thread_ptrs) {
        for (void *ptr : ptrs) {
            mm_pool_free(pool, ptr, 1024);
        }
    }

    mm_pool_destroy(pool);
}

/* ========================================================================
 * 内存碎片测试
 * ======================================================================== */

TEST_F(MMPoolTest, SlabFragmentation) {
    mm_pool_t *pool = mm_pool_slab_create(256, 16, "test");
    ASSERT_NE(pool, nullptr);

    // 分配并释放一些内存
    std::vector<void*> ptrs;
    for (int i = 0; i < 8; i++) {
        void *ptr = mm_pool_alloc(pool, 100);
        if (ptr != nullptr) ptrs.push_back(ptr);
    }

    // 释放奇数位置的内存
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        mm_pool_free(pool, ptrs[i], 100);
    }

    // 继续分配（应该能复用释放的块）
    for (int i = 0; i < 4; i++) {
        void *ptr = mm_pool_alloc(pool, 100);
        if (ptr != nullptr) ptrs.push_back(ptr);
    }

    // 获取统计
    mm_pool_stats_t stats;
    mm_pool_get_stats(pool, &stats);
    // 碎片率应该小于 50%
    EXPECT_LE(stats.fragmentation, 0.5);

    // 清理
    for (void *ptr : ptrs) {
        mm_pool_free(pool, ptr, 100);
    }

    mm_pool_destroy(pool);
}

/* ========================================================================
 * 全局内存池测试
 * ======================================================================== */

TEST_F(MMPoolTest, GlobalPoolInitShutdown) {
    // 初始化全局内存池
    int ret = mm_pool_init_global(TEST_DATA_DIR);
    EXPECT_EQ(ret, 0);

    // 验证全局池存在
    EXPECT_NE(g_vec_pool, nullptr);
    EXPECT_NE(g_graph_pool, nullptr);
    EXPECT_NE(g_ts_pool, nullptr);
    EXPECT_NE(g_doc_pool, nullptr);

    // 关闭全局内存池
    mm_pool_shutdown_global();

    // 验证池已销毁
    EXPECT_EQ(g_vec_pool, nullptr);
    EXPECT_EQ(g_graph_pool, nullptr);
    EXPECT_EQ(g_ts_pool, nullptr);
    EXPECT_EQ(g_doc_pool, nullptr);
}

TEST_F(MMPoolTest, GlobalPoolUsage) {
    // 初始化全局内存池
    mm_pool_init_global(TEST_DATA_DIR);

    // 使用向量池分配内存
    void *vec_ptr = mm_pool_alloc(g_vec_pool, 512);
    ASSERT_NE(vec_ptr, nullptr);
    memset(vec_ptr, 0xAB, 512);

    // 使用图池分配内存
    void *graph_ptr = mm_pool_alloc(g_graph_pool, 64);
    ASSERT_NE(graph_ptr, nullptr);
    memset(graph_ptr, 0xCD, 64);

    // 使用时序池分配内存
    void *ts_ptr = mm_pool_alloc(g_ts_pool, 1024);
    ASSERT_NE(ts_ptr, nullptr);
    memset(ts_ptr, 0xEF, 1024);

    // 使用文档池分配内存
    void *doc_ptr = mm_pool_alloc(g_doc_pool, 256);
    ASSERT_NE(doc_ptr, nullptr);
    memset(doc_ptr, 0x12, 256);

    // 释放内存
    mm_pool_free(g_vec_pool, vec_ptr, 512);
    mm_pool_free(g_graph_pool, graph_ptr, 64);
    mm_pool_free(g_ts_pool, ts_ptr, 1024);
    mm_pool_free(g_doc_pool, doc_ptr, 256);

    // 关闭全局内存池
    mm_pool_shutdown_global();
}
