/**
 * @file test_memctx.cpp
 * @brief MemoryContext 子系统单元测试
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <chrono>

/* 强制启用 MMDB_MEMORY_DEBUG 以测试线程归属校验路径：
 * memctx.c 已在内部默认启用该宏，测试端同步声明以便 GTEST_SKIP 分支
 * 与实现行为保持一致。 */
#ifndef MMDB_MEMORY_DEBUG
#define MMDB_MEMORY_DEBUG
#endif

/* 强制启用 MMDB_MEMCTX_STRICT_FREE 以测试严格释放校验路径 */
#ifndef MMDB_MEMCTX_STRICT_FREE
#define MMDB_MEMCTX_STRICT_FREE 1
#endif

extern "C" {
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief 统计上下文当前块链表长度（测试辅助）
 */
static int CountBlocks(MemoryContext ctx) {
    AllocSetContext *set = reinterpret_cast<AllocSetContext *>(ctx);
    int n = 0;
    for (AllocSetBlock *b = set->blocks; b != nullptr; b = b->next) {
        ++n;
    }
    return n;
}

/**
 * @brief 测试基本的内存分配与释放
 */
TEST(MemoryContextTest, BasicAlloc) {
    MemoryContext ctx = AllocSetContextCreate(NULL, "test", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    void *p = palloc(ctx, 100);
    ASSERT_NE(p, nullptr);

    /* 验证分配大小 */
    EXPECT_EQ(ctx->current_bytes, 100);

    pfree(ctx, p);
    delete_memory(ctx);
}

/**
 * @brief 测试 palloc0 分配零初始化内存
 */
TEST(MemoryContextTest, PallocZero) {
    MemoryContext ctx = AllocSetContextCreate(NULL, "test_zero", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    /* 分配一块内存并写入非零数据 */
    const Size size = 256;
    char *p = (char *)palloc0(ctx, size);
    ASSERT_NE(p, nullptr);

    /* 验证所有字节都是 0 */
    for (Size i = 0; i < size; ++i) {
        EXPECT_EQ(p[i], 0) << "Byte " << i << " should be 0";
    }

    delete_memory(ctx);
}

/**
 * @brief 测试父子层级: 重置子上下文释放子块（含多块场景）
 *
 * 先在子上下文分配到超过一个块，确认块链表长度 > 1；
 * 再 reset，验证只剩下保留的首块（长度 == 1），且首块可继续分配。
 */
TEST(MemoryContextTest, ResetFreesChildBlocks) {
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
    /* 子上下文用小块（1024），便于用少量分配触发新块 */
    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 1024, 1024, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(child, nullptr);

    /* 验证父子关系 */
    EXPECT_EQ(child->parent, parent);
    EXPECT_EQ(parent->firstchild, child);

    /* 初始只有预分配的首块 */
    EXPECT_EQ(CountBlocks(child), 1);

    /* 连续分配多次 512 字节，1024 的块放不下两个 512+对齐，必然触发额外块 */
    std::vector<void *> ptrs;
    for (int i = 0; i < 8; ++i) {
        void *p = palloc(child, 512);
        ASSERT_NE(p, nullptr);
        memset(p, 0xAA, 512);
        ptrs.push_back(p);
    }

    /* 此时应已产生多个块 */
    EXPECT_GT(CountBlocks(child), 1);

    Size child_alloc_before = child->current_bytes;
    EXPECT_GT(child_alloc_before, 0u);

    /* 重置子上下文 */
    reset_memory(child);

    /* 子上下文的已分配计数应被清零 */
    EXPECT_EQ(child->current_bytes, 0u);
    /* reset 后应只保留首块，其余块被释放 */
    EXPECT_EQ(CountBlocks(child), 1);

    /* 重新分配应成功 */
    void *p2 = palloc(child, 100);
    ASSERT_NE(p2, nullptr);

    delete_memory(parent);
}

/**
 * @brief 测试多块分配: 触发新块分配
 */
TEST(MemoryContextTest, AllocSetMultipleBlocks) {
    MemoryContext ctx = AllocSetContextCreate(NULL, "test_multi", 0, 1024, 8192, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    /* 分配多个块，触发新块分配 */
    std::vector<void *> ptrs;
    for (int i = 0; i < 100; i++) {
        ptrs.push_back(palloc(ctx, 1000));
    }

    /* 所有指针都应该非空 */
    for (void *p : ptrs) {
        ASSERT_NE(p, nullptr);
    }

    /* 验证分配计数 */
    EXPECT_EQ(ctx->current_bytes, 100u * 1000u);

    delete_memory(ctx);
}

/**
 * @brief 测试兄弟上下文关系
 */
TEST(MemoryContextTest, SiblingContexts) {
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
    MemoryContext child1 = AllocSetContextCreate(parent, "child1", 0, 1024, 1024, ALLOCSET_PRESET_DEFAULT);
    MemoryContext child2 = AllocSetContextCreate(parent, "child2", 0, 1024, 1024, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(child1, nullptr);
    ASSERT_NE(child2, nullptr);

    /* 新创建的子上下文插入到 firstchild 链表首部 */
    EXPECT_EQ(parent->firstchild, child2);  /* 最后创建的在前 */
    EXPECT_EQ(child2->nextchild, child1);
    EXPECT_EQ(child1->prevchild, child2);

    delete_memory(parent);
}

/**
 * @brief 测试 8 字节对齐
 */
TEST(MemoryContextTest, Alignment) {
    MemoryContext ctx = AllocSetContextCreate(NULL, "align_test", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    /* 分配若干小块，验证地址对齐 */
    void *p1 = palloc(ctx, 1);
    void *p2 = palloc(ctx, 5);
    void *p3 = palloc(ctx, 13);

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);

    /* 所有指针都应按 8 字节对齐 */
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p1) % 8, 0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p2) % 8, 0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p3) % 8, 0u);

    delete_memory(ctx);
}

/**
 * @brief 测试上下文名称
 */
TEST(MemoryContextTest, ContextName) {
    MemoryContext ctx = AllocSetContextCreate(NULL, "my_context", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);
    ASSERT_NE(ctx->name, nullptr);
    EXPECT_STREQ(ctx->name, "my_context");

    delete_memory(ctx);
}

/**
 * @brief 测试超大请求触发溢出保护返回 NULL（不得回绕成小缓冲区）
 */
TEST(MemoryContextTest, AllocOverflowReturnsNull) {
    MemoryContext ctx = AllocSetContextCreate(NULL, "overflow", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    /* 接近 Size 上限的请求：对齐加法会回绕，必须被拦截返回 NULL */
    EXPECT_EQ(palloc(ctx, ALLOCSET_MAX_SIZE), nullptr);
    EXPECT_EQ(palloc(ctx, ALLOCSET_MAX_SIZE - 3), nullptr);
    /* 减去块头后仍接近上限，块头加法会回绕，同样拦截 */
    EXPECT_EQ(palloc(ctx, ALLOCSET_MAX_SIZE - ALLOCSET_ALIGNMENT), nullptr);

    /* 计数不应被污染 */
    EXPECT_EQ(ctx->current_bytes, 0u);

    delete_memory(ctx);
}

/**
 * @brief 测试 minContextSize 提升首块数据区容量
 */
TEST(MemoryContextTest, MinContextSizeHonored) {
    /* initBlockSize 给一个小值，用 minContextSize 抬高首块容量 */
    const Size min_size = 4096;
    MemoryContext ctx = AllocSetContextCreate(NULL, "min_ctx", min_size, 1024, 8192, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    AllocSetContext *set = reinterpret_cast<AllocSetContext *>(ctx);
    ASSERT_NE(set->blocks, nullptr);
    /* 首块可分配空间应不小于 minContextSize */
    EXPECT_GE(set->blocks->free, min_size);

    /* 应能在首块内一次性分配 minContextSize 而不新增块 */
    int blocks_before = CountBlocks(ctx);
    void *p = palloc(ctx, min_size);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(CountBlocks(ctx), blocks_before);

    delete_memory(ctx);
}

/**
 * @brief 测试 SwitchTo 基本上下文切换与恢复
 */
TEST(MemoryContextTest, SwitchToAndRestore) {
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(parent, nullptr);

    /* 切换前，当前上下文应为 NULL（初始状态）或由其他测试设置 */
    MemoryContext prev = MemoryContextSwitchTo(parent);
    EXPECT_EQ(MemoryContextCurrent(), parent);

    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(child, nullptr);

    MemoryContext old = MemoryContextSwitchTo(child);
    EXPECT_EQ(MemoryContextCurrent(), child);
    EXPECT_EQ(old, parent);

    /* 恢复到旧上下文 */
    MemoryContextSwitchTo(old);
    EXPECT_EQ(MemoryContextCurrent(), parent);

    delete_memory(parent);
}

/**
 * @brief 测试嵌套 SwitchTo 场景
 */
TEST(MemoryContextTest, SwitchToNested) {
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(parent, nullptr);

    MemoryContext child1 = AllocSetContextCreate(parent, "child1", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    MemoryContext child2 = AllocSetContextCreate(parent, "child2", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(child1, nullptr);
    ASSERT_NE(child2, nullptr);

    /* 嵌套切换 */
    MemoryContextSwitchTo(parent);
    MemoryContext old1 = MemoryContextSwitchTo(child1);
    MemoryContext old2 = MemoryContextSwitchTo(child2);

    EXPECT_EQ(MemoryContextCurrent(), child2);
    EXPECT_EQ(old1, parent);

    /* 逐层恢复 */
    MemoryContextSwitchTo(old2);
    EXPECT_EQ(MemoryContextCurrent(), child1);

    MemoryContextSwitchTo(old1);
    EXPECT_EQ(MemoryContextCurrent(), parent);

    delete_memory(parent);
}

/**
 * @brief 测试 SwitchTo 后在当前上下文分配内存
 */
TEST(MemoryContextTest, SwitchToAlloc) {
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(parent, nullptr);

    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 1024, 1024, ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(child, nullptr);

    /* 切换到子上下文 */
    MemoryContextSwitchTo(child);

    /* 在子上下文分配（通过 CurrentMemoryContext 间接验证） */
    void *ptr = palloc(MemoryContextCurrent(), 100);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(child->current_bytes, 100u);

    /* 恢复父上下文 */
    MemoryContextSwitchTo(parent);

    delete_memory(parent);
}

/* ========================================================================
 * 资源析构机制测试
 * ======================================================================== */

/**
 * @brief 资源析构测试夹具
 *
 * 提供 parent 上下文和全局析构计数器，用于验证资源注册、取消注册和析构行为。
 */
class ResourceDestructionTest : public ::testing::Test {
public:
    void SetUp() override {
        parent = AllocSetContextCreate(NULL, "res_parent", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
        ASSERT_NE(parent, nullptr);
        destructor_call_count = 0;
        last_destroyed_resource = nullptr;
        destruction_order_idx = 0;
        memset(destruction_order, 0, sizeof(destruction_order));
    }

    void TearDown() override {
        delete_memory(parent);
        parent = nullptr;
    }

    MemoryContext parent;

    /* 全局析构统计 */
    int destructor_call_count;
    void *last_destroyed_resource;

    /* LIFO 顺序验证 */
    int destruction_order_idx;
    int destruction_order[10];
};

/**
 * @brief 测试析构回调：记录调用次数和资源指针
 */
static void test_res_destructor(void *resource, void *arg)
{
    ResourceDestructionTest *fixture =
        static_cast<ResourceDestructionTest *>(arg);
    fixture->destructor_call_count++;
    fixture->last_destroyed_resource = resource;
}

/**
 * @brief 测试析构回调：记录资源 ID 到 order 数组（用于 LIFO 验证）
 */
static void test_res_destructor_order(void *resource, void *arg)
{
    ResourceDestructionTest *fixture =
        static_cast<ResourceDestructionTest *>(arg);
    int res_id = *static_cast<int *>(resource);
    fixture->destruction_order[fixture->destruction_order_idx++] = res_id;
}

/**
 * @brief 测试注册和取消注册资源
 */
TEST_F(ResourceDestructionTest, RegisterResource) {
    int resource1 = 42;
    int resource2 = 99;

    EXPECT_EQ(0, mmdb_mem_register_resource(parent, &resource1,
                                            test_res_destructor, this, "res1"));
    EXPECT_EQ(parent->resource_count, 1u);

    EXPECT_EQ(0, mmdb_mem_register_resource(parent, &resource2,
                                            test_res_destructor, this, "res2"));
    EXPECT_EQ(parent->resource_count, 2u);

    /* 取消注册 res1 */
    EXPECT_EQ(0, mmdb_mem_unregister_resource(parent, &resource1));
    EXPECT_EQ(parent->resource_count, 1u);

    /* 再次取消注册应返回 -1（已不在链表中） */
    EXPECT_EQ(-1, mmdb_mem_unregister_resource(parent, &resource1));

    /* 取消注册 res2 */
    EXPECT_EQ(0, mmdb_mem_unregister_resource(parent, &resource2));
    EXPECT_EQ(parent->resource_count, 0u);

    /* 资源不在链表中时取消注册返回 -1 */
    int dummy = 0;
    EXPECT_EQ(-1, mmdb_mem_unregister_resource(parent, &dummy));
}

/**
 * @brief 测试 Reset 时自动执行资源析构
 */
TEST_F(ResourceDestructionTest, ResourceDestructionOnReset) {
    void *resource1 = malloc(100);
    void *resource2 = malloc(200);
    ASSERT_NE(resource1, nullptr);
    ASSERT_NE(resource2, nullptr);

    EXPECT_EQ(0, mmdb_mem_register_resource(parent, resource1,
                                            test_res_destructor, this, "heap1"));
    EXPECT_EQ(0, mmdb_mem_register_resource(parent, resource2,
                                            test_res_destructor, this, "heap2"));
    EXPECT_EQ(parent->resource_count, 2u);

    /* Reset 应触发析构 */
    reset_memory(parent);

    EXPECT_EQ(destructor_call_count, 2);
    EXPECT_EQ(parent->resource_count, 0u);

    free(resource1);
    free(resource2);
}

/**
 * @brief 测试 Delete 时自动执行资源析构
 */
TEST_F(ResourceDestructionTest, ResourceDestructionOnDelete) {
    void *resource = malloc(100);
    ASSERT_NE(resource, nullptr);

    EXPECT_EQ(0, mmdb_mem_register_resource(parent, resource,
                                            test_res_destructor, this, "heap"));
    EXPECT_EQ(parent->resource_count, 1u);

    /* Delete 应触发析构 */
    delete_memory(parent);
    parent = nullptr;

    EXPECT_EQ(destructor_call_count, 1);
    EXPECT_EQ(last_destroyed_resource, resource);

    free(resource);
}

/**
 * @brief 测试 LIFO 析构顺序
 *
 * 注册 3 个资源，验证 Reset 时按 LIFO（后进先出）顺序析构。
 */
TEST_F(ResourceDestructionTest, ResourceDestructionLIFO) {
    int res1 = 1, res2 = 2, res3 = 3;

    /* 按 1 -> 2 -> 3 顺序注册 */
    EXPECT_EQ(0, mmdb_mem_register_resource(parent, &res1,
                                            test_res_destructor_order, this, "r1"));
    EXPECT_EQ(0, mmdb_mem_register_resource(parent, &res2,
                                            test_res_destructor_order, this, "r2"));
    EXPECT_EQ(0, mmdb_mem_register_resource(parent, &res3,
                                            test_res_destructor_order, this, "r3"));
    EXPECT_EQ(parent->resource_count, 3u);

    /* Reset 应按 LIFO 顺序析构：3 -> 2 -> 1 */
    reset_memory(parent);

    EXPECT_EQ(destruction_order_idx, 3);
    EXPECT_EQ(destruction_order[0], 3);  /* 最后注册，最先析构 */
    EXPECT_EQ(destruction_order[1], 2);
    EXPECT_EQ(destruction_order[2], 1);  /* 最先注册，最后析构 */
    EXPECT_EQ(parent->resource_count, 0u);
}

/* ========================================================================
 * Task 5: Reset/Delete 生命周期保护与统计测试
 * ======================================================================== */

/**
 * @brief 生命周期测试夹具
 *
 * 提供 parent 上下文，用于 Reset/Delete 生命周期保护测试。
 */
class LifecycleTest : public ::testing::Test {
public:
    void SetUp() override {
        parent = AllocSetContextCreate(NULL, "lifecycle_parent", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
        ASSERT_NE(parent, nullptr);
    }

    void TearDown() override {
        /* 安全清理：仅当父上下文未被测试删除时才清理 */
        if (parent) {
            delete_memory(parent);
            parent = nullptr;
        }
    }

    MemoryContext parent;
};

/**
 * @brief 测试 Reset 保留首块、重置分配指针、更新统计
 */
TEST_F(LifecycleTest, ResetPreservesFirstBlock) {
    palloc(parent, 100);
    palloc(parent, 200);

    AllocSetContext *set = (AllocSetContext *)parent;
    int block_count_before = CountBlocks(parent);
    EXPECT_GE(block_count_before, 1);

    reset_memory(parent);

    /* 重置后应保留首块 */
    EXPECT_NE(set->blocks, nullptr);
    /* 首块 free 应恢复为完整数据区容量（未分配状态） */
    EXPECT_EQ(set->blocks->free, set->blocks->size - ALLOCSET_ALIGN(sizeof(AllocSetBlock)));
    /* 所有扩展块应已释放，只剩首块 */
    EXPECT_EQ(CountBlocks(parent), 1);
    EXPECT_EQ(parent->current_bytes, 0u);
    EXPECT_EQ(parent->generation, 1u);
}

/**
 * @brief 测试 Delete 释放全部块并从父链表移除
 */
TEST_F(LifecycleTest, DeleteReleasesAllBlocks) {
    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    palloc(child, 100);

    AllocSetContext *child_set = (AllocSetContext *)child;
    EXPECT_NE(child_set->blocks, nullptr);

    /* 记录父上下文子链表状态 */
    EXPECT_EQ(parent->firstchild, child);

    delete_memory(child);

    /* 父上下文子链表应已更新：child 不再是 firstchild */
    EXPECT_NE(parent->firstchild, child);
    /* parent 无其他子上下文时应为 NULL */
    EXPECT_EQ(parent->firstchild, nullptr);
}

/**
 * @brief 测试递归 Reset：父上下文 Reset 会重置所有子上下文的 current_bytes
 */
TEST_F(LifecycleTest, RecursiveReset) {
    MemoryContext child1 = AllocSetContextCreate(parent, "child1", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    MemoryContext child2 = AllocSetContextCreate(parent, "child2", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);

    MemoryContextSwitchTo(child1);
    palloc(child1, 100);
    MemoryContextSwitchTo(child2);
    palloc(child2, 200);
    MemoryContextSwitchTo(parent);

    EXPECT_GT(child1->current_bytes, 0u);
    EXPECT_GT(child2->current_bytes, 0u);

    MemoryContextReset(parent);

    EXPECT_EQ(child1->current_bytes, 0u);
    EXPECT_EQ(child2->current_bytes, 0u);
}

/**
 * @brief 测试统计累积：allocation_count、total_allocated、peak_bytes
 */
TEST_F(LifecycleTest, StatsAccumulation) {
    palloc(parent, 100);
    palloc(parent, 200);
    palloc(parent, 300);

    EXPECT_EQ(parent->allocation_count, 3u);
    /* total_allocated 累计用户请求的原始大小 */
    EXPECT_EQ(parent->total_allocated, 100u + 200u + 300u);
    /* current_bytes 为当前存活的分配总和 */
    EXPECT_EQ(parent->current_bytes, 100u + 200u + 300u);
    /* peak_bytes 应不小于 current_bytes */
    EXPECT_GE(parent->peak_bytes, parent->current_bytes);
}

/**
 * @brief 测试 Reset 后 peak_bytes 保持历史峰值
 */
TEST_F(LifecycleTest, ResetPreservesPeakBytes) {
    palloc(parent, 500);
    Size peak_before = parent->peak_bytes;
    EXPECT_EQ(peak_before, 500u);

    reset_memory(parent);
    EXPECT_EQ(parent->current_bytes, 0u);
    /* peak_bytes 应保留历史峰值 */
    EXPECT_EQ(parent->peak_bytes, peak_before);

    /* 再次分配较小内存，peak 不应下降 */
    palloc(parent, 100);
    EXPECT_EQ(parent->peak_bytes, peak_before);
}

/**
 * @brief 测试 Reset 计数器递增
 */
TEST_F(LifecycleTest, ResetCountIncrements) {
    EXPECT_EQ(parent->reset_count, 0u);

    reset_memory(parent);
    EXPECT_EQ(parent->reset_count, 1u);

    reset_memory(parent);
    EXPECT_EQ(parent->reset_count, 2u);
}

/**
 * @brief 测试已删除上下文的 Reset 为空操作
 */
TEST_F(LifecycleTest, ResetAfterDeleteIsNoop) {
    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    palloc(child, 100);

    /* 删除子上下文 */
    delete_memory(child);

    /* 父上下文应无子上下文 */
    EXPECT_EQ(parent->firstchild, nullptr);
}

/**
 * @brief 测试 MemoryContextResetChildren 只重置子上下文
 */
TEST_F(LifecycleTest, ResetChildrenOnly) {
    MemoryContext child1 = AllocSetContextCreate(parent, "child1", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    MemoryContext child2 = AllocSetContextCreate(parent, "child2", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);

    palloc(parent, 100);
    palloc(child1, 200);
    palloc(child2, 300);

    MemoryContextResetChildren(parent);

    /* 子上下文应被重置 */
    EXPECT_EQ(child1->current_bytes, 0u);
    EXPECT_EQ(child2->current_bytes, 0u);

    /* 父上下文不受影响 */
    EXPECT_EQ(parent->current_bytes, 100u);
}

/**
 * @brief 测试 MemoryContextReset 标准 API 名称
 */
TEST_F(LifecycleTest, StandardAPIReset) {
    palloc(parent, 100);
    AllocSetContext *set = (AllocSetContext *)parent;

    MemoryContextReset(parent);

    EXPECT_EQ(parent->current_bytes, 0u);
    EXPECT_NE(set->blocks, nullptr);
    EXPECT_EQ(CountBlocks(parent), 1);
}

/**
 * @brief 测试 MemoryContextDelete 标准 API 名称
 */
TEST_F(LifecycleTest, StandardAPIDelete) {
    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    EXPECT_EQ(parent->firstchild, child);

    MemoryContextDelete(child);

    EXPECT_EQ(parent->firstchild, nullptr);
}

/**
 * @brief 测试多层嵌套父子关系的递归 Delete
 */
TEST_F(LifecycleTest, RecursiveDelete) {
    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 8192, 8192 * 1024, ALLOCSET_PRESET_DEFAULT);
    MemoryContext grandchild = AllocSetContextCreate(child, "grandchild", 0, 1024, 1024, ALLOCSET_PRESET_DEFAULT);

    palloc(grandchild, 50);
    EXPECT_EQ(grandchild->current_bytes, 50u);

    /* 删除 child 应递归删除 grandchild */
    delete_memory(child);

    /* parent 无子上下文 */
    EXPECT_EQ(parent->firstchild, nullptr);
}

/* ========================================================================
 * Task 6: 线程归属校验与 Generation 追踪
 * ======================================================================== */

/**
 * @brief 线程归属测试夹具
 */
class ThreadOwnershipTest : public ::testing::Test {
public:
    void SetUp() override {
        parent = AllocSetContextCreate(NULL, "thread_parent", 0, 8192, 8192, ALLOCSET_PRESET_DEFAULT);
        ASSERT_NE(parent, nullptr);
    }

    void TearDown() override {
        if (parent) {
            delete_memory(parent);
            parent = nullptr;
        }
    }

    MemoryContext parent;
};

/**
 * @brief 测试线程归属设置与校验
 *
 * 1. 默认未启用归属检查，CheckThread 应返回 true
 * 2. 设置当前线程为所有者后，CheckThread 应返回 true
 * 3. 设置为其他线程 ID 后，CheckThread 应返回 false
 */
TEST_F(ThreadOwnershipTest, ThreadOwnership) {
    /* 1. 默认未启用归属检查 */
    EXPECT_FALSE(parent->is_thread_owner);
    EXPECT_TRUE(MemoryContextCheckThread(parent));

    /* 2. 设置当前线程为所有者 */
    MemoryContextSetThreadOwner(parent, mmdb_current_thread_id());
    EXPECT_TRUE(parent->is_thread_owner);
    EXPECT_TRUE(MemoryContextCheckThread(parent));

    /* 3. 模拟错误线程 */
    MemoryContextSetThreadOwner(parent, 999999);
    EXPECT_TRUE(parent->is_thread_owner);
    EXPECT_FALSE(MemoryContextCheckThread(parent));
}

/**
 * @brief 测试 NULL 上下文对线程 API 的响应
 */
TEST_F(ThreadOwnershipTest, NullContextSafe) {
    /* SetThreadOwner 对 NULL 应为空操作 */
    MemoryContextSetThreadOwner(nullptr, 12345);

    /* CheckThread 对 NULL 应返回 true（不阻塞调用方） */
    EXPECT_TRUE(MemoryContextCheckThread(nullptr));

    /* GetGeneration 对 NULL 应返回 0 */
    EXPECT_EQ(MemoryContextGetGeneration(nullptr), 0u);
}

/**
 * @brief 测试跨线程访问检测
 *
 * 在 Debug 模式下，设置错误的线程 ID 后调用 palloc 应返回 NULL。
 * 在 Release 模式下（未定义 MMDB_MEMORY_DEBUG），跳过此测试。
 */
TEST_F(ThreadOwnershipTest, CrossThreadAccessDetected) {
#ifndef MMDB_MEMORY_DEBUG
    GTEST_SKIP() << "MMDB_MEMORY_DEBUG not enabled; cross-thread detection "
                    "requires debug build";
#else
    /* 设置错误的线程 ID */
    MemoryContextSetThreadOwner(parent, 999999);

    /* Debug 模式下 palloc 应返回 NULL */
    void *result = palloc(parent, 100);
    EXPECT_EQ(result, nullptr);

    /* 同样，pfree 也应静默返回（void） */
    pfree(parent, nullptr);
#endif
}

/**
 * @brief 测试 Generation 追踪
 *
 * 1. 初始 generation 为 0
 * 2. 分配后 header.generation 等于当时的 context.generation
 * 3. Reset 后 generation 递增
 * 4. 重新分配后 header.generation 反映递增后的值
 */
TEST_F(ThreadOwnershipTest, GenerationTracking) {
    EXPECT_EQ(MemoryContextGetGeneration(parent), 0u);

    /* 第一次分配，generation 应为 0 */
    void *ptr = palloc(parent, 100);
    ASSERT_NE(ptr, nullptr);
    MemoryAllocationHeader *header = GET_ALLOCATION_HEADER(ptr);
    EXPECT_EQ(header->generation, 0u);
    EXPECT_EQ(MemoryContextGetGeneration(parent), 0u);

    /* Reset 后 generation 递增 */
    reset_memory(parent);
    EXPECT_EQ(MemoryContextGetGeneration(parent), 1u);

    /* 第二次分配，header.generation 应为 1 */
    void *ptr2 = palloc(parent, 100);
    ASSERT_NE(ptr2, nullptr);
    MemoryAllocationHeader *header2 = GET_ALLOCATION_HEADER(ptr2);
    EXPECT_EQ(header2->generation, 1u);

    /* 再次 Reset，generation 递增到 2 */
    reset_memory(parent);
    EXPECT_EQ(MemoryContextGetGeneration(parent), 2u);
}

/**
 * @brief 测试未启用归属检查时 palloc 正常工作
 */
TEST_F(ThreadOwnershipTest, AllocWithoutThreadOwner) {
    /* is_thread_owner=false 时 CHECK_THREAD 退化为空操作 */
    EXPECT_FALSE(parent->is_thread_owner);

    void *ptr = palloc(parent, 256);
    EXPECT_NE(ptr, nullptr);

    /* 校验 header 被正确写入 */
    MemoryAllocationHeader *header = GET_ALLOCATION_HEADER(ptr);
    EXPECT_EQ(header->magic, MEMORY_ALLOCATION_HEADER_MAGIC);
    EXPECT_EQ(header->requested_size, 256u);
    EXPECT_EQ(header->owner, parent);
    EXPECT_EQ(header->generation, parent->generation);
}

/**
 * @brief 测试设置当前线程后 palloc 仍能正常工作
 */
TEST_F(ThreadOwnershipTest, AllocWithCorrectThreadOwner) {
    MemoryContextSetThreadOwner(parent, mmdb_current_thread_id());

    /* 当前线程匹配，palloc 应成功 */
    void *ptr = palloc(parent, 256);
    EXPECT_NE(ptr, nullptr);
}

}  // namespace

/* ========================================================================
 * 严格释放模式（MMDB_MEMCTX_STRICT_FREE）测试
 * ======================================================================== */

#if MMDB_MEMCTX_STRICT_FREE

/**
 * @brief 测试双重释放检测
 *
 * 验证 pfree() 在严格模式下能检测到双重释放，
 * 并将 double_free_count 计数器递增。
 */
TEST(StrictFreeTest, DoubleFreeDetected) {
    MemoryContext ctx = AllocSetContextCreate(
        NULL, "StrictDoubleFree", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    /* 分配一块内存 */
    void *ptr = palloc(ctx, 128);
    ASSERT_NE(ptr, nullptr);

    /* 第一次释放 — 应成功 */
    pfree(ctx, ptr);
    EXPECT_EQ(ctx->double_free_count, (Size)0);

    /* 第二次释放 — 应被检测到 */
    pfree(ctx, ptr);
    EXPECT_GE(ctx->double_free_count, (Size)1)
        << "双重释放应被检测到并递增 double_free_count";

    /* 清理 */
    MemoryContextDelete(ctx);
}

/**
 * @brief 测试跨上下文释放检测
 *
 * 验证 pfree() 在严格模式下能检测到跨上下文释放，
 * 并将 invalid_free_count 计数器递增。
 */
TEST(StrictFreeTest, CrossContextFreeDetected) {
    /* 创建两个上下文 */
    MemoryContext ctx1 = AllocSetContextCreate(
        NULL, "StrictCtx1", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx1, nullptr);

    MemoryContext ctx2 = AllocSetContextCreate(
        NULL, "StrictCtx2", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx2, nullptr);

    /* 在 ctx1 中分配内存 */
    void *ptr = palloc(ctx1, 128);
    ASSERT_NE(ptr, nullptr);

    /* 尝试用 ctx2 释放 ctx1 的内存 — 应被检测到 */
    pfree(ctx2, ptr);
    EXPECT_GE(ctx2->invalid_free_count, (Size)1)
        << "跨上下文释放应被检测到并递增 invalid_free_count";

    /* 原上下文不受影响 */
    EXPECT_EQ(ctx1->invalid_free_count, (Size)0)
        << "原上下文的 invalid_free_count 不应受影响";

    /* 清理 */
    MemoryContextDelete(ctx1);
    MemoryContextDelete(ctx2);
}

/**
 * @brief 测试魔数校验失败检测
 *
 * 验证 pfree() 在严格模式下能检测到魔数校验失败
 * （例如对已释放内存或野指针调用 pfree）。
 */
TEST(StrictFreeTest, MagicCheckFailed) {
    MemoryContext ctx = AllocSetContextCreate(
        NULL, "StrictMagic", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    /* 模拟已释放内存（魔数被覆盖） */
    void *ptr = palloc(ctx, 128);
    ASSERT_NE(ptr, nullptr);

    /* 覆盖魔数 */
    MemoryAllocationHeader *hdr = GET_ALLOCATION_HEADER(ptr);
    hdr->magic = 0xDEADBEEF;  /* 无效魔数 */

    /* 尝试释放 — 应检测到魔数失败 */
    pfree(ctx, ptr);
    EXPECT_GE(ctx->invalid_free_count, (Size)1)
        << "魔数校验失败应被检测到并递增 invalid_free_count";

    /* 清理 */
    MemoryContextDelete(ctx);
}

/**
 * @brief 测试 Reset 时的未释放检测
 *
 * 验证 MemoryContextReset() 在严格模式下能检测到未释放的分配。
 */
TEST(StrictFreeTest, ResetDetectsUnfreed) {
    MemoryContext ctx = AllocSetContextCreate(
        NULL, "StrictReset", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx, nullptr);

    /* 分配内存但不释放 */
    void *ptr1 = palloc(ctx, 64);
    void *ptr2 = palloc(ctx, 128);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    /* Reset — 应检测到未释放的分配并输出警告 */
    MemoryContextReset(ctx);

    /* Reset 后 current_bytes 应清零 */
    EXPECT_EQ(ctx->current_bytes, (Size)0);

    /* 清理 */
    MemoryContextDelete(ctx);
}

#endif /* MMDB_MEMCTX_STRICT_FREE */

/* ========================================================================
 * 性能对比测试：AllocSet 预设配置
 * ======================================================================== */

/**
 * @brief 验证 ALLOCSET_PRESET_SMALL高频 预设的块大小参数正确
 *
 * 验证预设配置正确设置了 initBlockSize 和 maxBlockSize。
 * 预设 1（小对象高频）：1KB init, 64KB max
 * 预设 0（默认）：8KB init, 8KB max
 *
 * 块大小差异体现在 malloc 级别：1KB init 比 8KB init 节省 87.5% 初始内存。
 */
TEST(AllocSetPresetPerf, PresetBlockSizes) {
    /* 创建 DEFAULT 预设上下文 */
    MemoryContext ctx_def = AllocSetContextCreate(
        NULL, "DefaultSize", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx_def, nullptr);

    /* 创建 SMALL高频 预设上下文 */
    MemoryContext ctx_small = AllocSetContextCreate(
        NULL, "SmallSize", 0,
        ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_SMALL高频);
    ASSERT_NE(ctx_small, nullptr);

    /* 验证 block size 设置正确 */
    AllocSetContext *set_def = (AllocSetContext *)ctx_def;
    AllocSetContext *set_small = (AllocSetContext *)ctx_small;

    /* DEFAULT 预设：initBlockSize = 8KB, maxBlockSize = 8KB */
    EXPECT_EQ(set_def->initBlockSize, (Size)ALLOCSET_DEFAULT_BLOCK_SIZE);
    EXPECT_EQ(set_def->maxBlockSize, (Size)ALLOCSET_DEFAULT_BLOCK_SIZE);

    /* SMALL高频 预设：initBlockSize = 1KB, maxBlockSize = 64KB */
    EXPECT_EQ(set_small->initBlockSize, (Size)ALLOCSET_PRESET1_INIT);
    EXPECT_EQ(set_small->maxBlockSize, (Size)ALLOCSET_PRESET1_MAX);

    printf("  DEFAULT: init=%llu max=%llu, SMALL高频: init=%llu max=%llu\n",
           (unsigned long long)set_def->initBlockSize,
           (unsigned long long)set_def->maxBlockSize,
           (unsigned long long)set_small->initBlockSize,
           (unsigned long long)set_small->maxBlockSize);

    /* 验证向后兼容：DEFAULT 预设的 initBlockSize 应等于传入的 initBlockSize */
    MemoryContext ctx_compat = AllocSetContextCreate(
        NULL, "CompatSize", 0, 4096, 16384,
        ALLOCSET_PRESET_DEFAULT);
    ASSERT_NE(ctx_compat, nullptr);
    AllocSetContext *set_compat = (AllocSetContext *)ctx_compat;
    EXPECT_EQ(set_compat->initBlockSize, (Size)4096);
    EXPECT_EQ(set_compat->maxBlockSize, (Size)16384);

    MemoryContextDelete(ctx_def);
    MemoryContextDelete(ctx_small);
    MemoryContextDelete(ctx_compat);
}
