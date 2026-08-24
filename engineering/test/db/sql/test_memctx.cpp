/**
 * @file test_memctx.cpp
 * @brief MemoryContext 子系统单元测试
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>

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
    MemoryContext ctx = AllocSetContextCreate(NULL, "test", 0, 8192, 8192);
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
    MemoryContext ctx = AllocSetContextCreate(NULL, "test_zero", 0, 8192, 8192);
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
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192);
    /* 子上下文用小块（1024），便于用少量分配触发新块 */
    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 1024, 1024);
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
    MemoryContext ctx = AllocSetContextCreate(NULL, "test_multi", 0, 1024, 8192);
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
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192);
    MemoryContext child1 = AllocSetContextCreate(parent, "child1", 0, 1024, 1024);
    MemoryContext child2 = AllocSetContextCreate(parent, "child2", 0, 1024, 1024);
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
    MemoryContext ctx = AllocSetContextCreate(NULL, "align_test", 0, 8192, 8192);
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
    MemoryContext ctx = AllocSetContextCreate(NULL, "my_context", 0, 8192, 8192);
    ASSERT_NE(ctx, nullptr);
    ASSERT_NE(ctx->name, nullptr);
    EXPECT_STREQ(ctx->name, "my_context");

    delete_memory(ctx);
}

/**
 * @brief 测试超大请求触发溢出保护返回 NULL（不得回绕成小缓冲区）
 */
TEST(MemoryContextTest, AllocOverflowReturnsNull) {
    MemoryContext ctx = AllocSetContextCreate(NULL, "overflow", 0, 8192, 8192);
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
    MemoryContext ctx = AllocSetContextCreate(NULL, "min_ctx", min_size, 1024, 8192);
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
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192 * 1024);
    ASSERT_NE(parent, nullptr);

    /* 切换前，当前上下文应为 NULL（初始状态）或由其他测试设置 */
    MemoryContext prev = MemoryContextSwitchTo(parent);
    EXPECT_EQ(MemoryContextCurrent(), parent);

    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 8192, 8192 * 1024);
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
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192 * 1024);
    ASSERT_NE(parent, nullptr);

    MemoryContext child1 = AllocSetContextCreate(parent, "child1", 0, 8192, 8192 * 1024);
    MemoryContext child2 = AllocSetContextCreate(parent, "child2", 0, 8192, 8192 * 1024);
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
    MemoryContext parent = AllocSetContextCreate(NULL, "parent", 0, 8192, 8192 * 1024);
    ASSERT_NE(parent, nullptr);

    MemoryContext child = AllocSetContextCreate(parent, "child", 0, 1024, 1024);
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
        parent = AllocSetContextCreate(NULL, "res_parent", 0, 8192, 8192);
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

}  // namespace