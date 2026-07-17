/**
 * @file memctx.c
 * @brief AllocSet 内存分配器实现
 *
 * 设计要点：
 * - 按块预分配：每个块一次性向系统申请 initBlockSize~maxBlockSize 字节
 * - 块内线性分配：维护首块，在首块剩余空间足够时直接推进；不足时申请新块
 * - 新块大小按 2 倍指数增长，直到 maxBlockSize 上限
 * - 8 字节对齐：所有应用层分配地址按 8 字节对齐
 * - 父子层级：
 *   - create：插入到 parent 的子链表首部
 *   - reset：释放所有子上下文；保留当前上下文的首块（重置 free 指针）
 *   - delete：递归删除子上下文 + 释放当前上下文所有块
 */

#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * 块头占用尺寸（对齐后）
 *
 * 数据区必须从 8 字节对齐地址开始。malloc 返回的地址满足最大对齐，
 * 因此只要块头占用是 8 的倍数，紧随其后的数据区即天然对齐。
 * 直接使用 sizeof(AllocSetBlock) 隐含假设其为 8 的倍数（在 32 位平台
 * 上可能不成立），故统一用 ALLOCSET_ALIGN(sizeof(AllocSetBlock)) 计算。
 * ======================================================================== */
#define ALLOCSET_BLOCK_HDR_SIZE (ALLOCSET_ALIGN(sizeof(AllocSetBlock)))

/* ========================================================================
 * AllocSet 方法表（静态实例）
 * ======================================================================== */

static void *allocset_alloc(MemoryContext ctx, Size size);
static void  allocset_free_p(MemoryContext ctx, void *ptr);
static void  allocset_reset(MemoryContext ctx);
static void  allocset_delete(MemoryContext ctx);

static const MemoryContextMethods g_allocset_methods = {
    .alloc     = allocset_alloc,
    .free_p    = allocset_free_p,
    .reset     = allocset_reset,
    .delete_ctx = allocset_delete,
};

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 计算下一个块大小（指数增长，至 maxBlockSize 上限）
 *
 * 采用饱和逻辑：current * 2 若发生无符号回绕则钳制到 maxBlockSize；
 * 最终结果保证不小于 needed（needed 由调用方保证已做过溢出检查）。
 */
static Size next_block_size(AllocSetContext *set, Size needed)
{
    Size current = set->blocks ? set->blocks->size : set->initBlockSize;

    /* current * 2 的饱和计算：若翻倍会溢出，则直接取 maxBlockSize */
    Size next;
    if (current > set->maxBlockSize / 2 || current > ALLOCSET_MAX_SIZE / 2) {
        next = set->maxBlockSize;
    } else {
        next = current * 2;
        if (next > set->maxBlockSize) {
            next = set->maxBlockSize;
        }
    }

    if (next < needed) {
        next = needed;
    }
    return next;
}

/**
 * @brief 分配新块并链接到块链表首部
 */
static AllocSetBlock *allocset_new_block(AllocSetContext *set, Size needed)
{
    Size block_size = next_block_size(set, needed);
    if (block_size < ALLOCSET_MIN_BLOCK_SIZE) {
        block_size = ALLOCSET_MIN_BLOCK_SIZE;
    }

    /* 分配块：块头 + 数据区 */
    AllocSetBlock *block = (AllocSetBlock *)malloc(block_size);
    if (!block) {
        return NULL;
    }

    block->size = block_size;
    block->free = block_size - ALLOCSET_BLOCK_HDR_SIZE;
    block->start = (char *)block + ALLOCSET_BLOCK_HDR_SIZE;
    block->end = (char *)block + block_size;
    block->next = set->blocks;

    set->blocks = block;
    return block;
}

/**
 * @brief 在当前块分配内存；若当前块剩余空间不足则申请新块
 */
static void *allocset_alloc(MemoryContext ctx, Size size)
{
    AllocSetContext *set = (AllocSetContext *)ctx;
    if (!ctx || size == 0) {
        return NULL;
    }

    /* 溢出检查 1：对齐加法。ALLOCSET_ALIGN(size) = size + 7 后按位与，
     * 若 size 过大会回绕成很小甚至为 0 的值，导致返回小于请求的缓冲区。
     * 上界为 ALLOCSET_MAX_SIZE - (ALLOCSET_ALIGNMENT - 1)。 */
    if (size > ALLOCSET_MAX_SIZE - (ALLOCSET_ALIGNMENT - 1)) {
        return NULL;
    }
    Size aligned = ALLOCSET_ALIGN(size);

    /* 溢出检查 2：块头 + 数据区。若 aligned 接近上限，加上块头会回绕。 */
    if (aligned > ALLOCSET_MAX_SIZE - ALLOCSET_BLOCK_HDR_SIZE) {
        return NULL;
    }
    Size needed = ALLOCSET_BLOCK_HDR_SIZE + aligned;

    /* 在首块尝试分配；不足则申请新块（首块替换为新块） */
    AllocSetBlock *block = set->blocks;
    if (!block || block->free < aligned) {
        block = allocset_new_block(set, needed);
        if (!block) {
            return NULL;
        }
    }

    /* 在首块末尾分配（线性推进） */
    void *ptr = block->end - block->free;
    block->free -= aligned;
    set->header.mem_allocated += size;

    return ptr;
}

/**
 * @brief pfree 在 AllocSet 中为空操作
 */
static void allocset_free_p(MemoryContext ctx, void *ptr)
{
    /* noop：AllocSet 不支持单独释放，依靠 reset/delete 统一回收 */
    (void)ctx;
    (void)ptr;
}

/**
 * @brief 重置 AllocSet：保留首块，释放其余块
 */
static void allocset_reset(MemoryContext ctx)
{
    AllocSetContext *set = (AllocSetContext *)ctx;
    if (!ctx) {
        return;
    }

    /* 先递归重置所有子上下文 */
    MemoryContext child = set->header.firstchild;
    while (child) {
        MemoryContext next = child->nextchild;
        if (child->methods && child->methods->reset) {
            child->methods->reset(child);
        }
        child = next;
    }

    /* 保留首块，重置其 free 指针；释放其余块 */
    AllocSetBlock *first = set->blocks;
    if (first) {
        AllocSetBlock *cur = first->next;
        first->next = NULL;
        first->free = first->size - ALLOCSET_BLOCK_HDR_SIZE;
        while (cur) {
            AllocSetBlock *next = cur->next;
            free(cur);
            cur = next;
        }
    }

    set->header.mem_allocated = 0;
    set->header.isReset = true;
}

/**
 * @brief 删除 AllocSet：释放全部块，递归删除子上下文
 */
static void allocset_delete(MemoryContext ctx)
{
    AllocSetContext *set = (AllocSetContext *)ctx;
    if (!ctx) {
        return;
    }

    /* 先递归删除所有子上下文 */
    MemoryContext child = set->header.firstchild;
    while (child) {
        MemoryContext next = child->nextchild;
        if (child->methods && child->methods->delete_ctx) {
            child->methods->delete_ctx(child);
        }
        child = next;
    }

    /* 释放全部块 */
    AllocSetBlock *block = set->blocks;
    while (block) {
        AllocSetBlock *next = block->next;
        free(block);
        block = next;
    }
    set->blocks = NULL;

    /* 从父上下文的子链表中移除 */
    if (set->header.parent) {
        MemoryContext p = set->header.parent;
        if (p->firstchild == ctx) {
            p->firstchild = ctx->nextchild;
        }
        if (ctx->prevchild) {
            ctx->prevchild->nextchild = ctx->nextchild;
        }
        if (ctx->nextchild) {
            ctx->nextchild->prevchild = ctx->prevchild;
        }
    }

    free(set);
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 创建 AllocSet 内存上下文
 */
MemoryContext AllocSetContextCreate(
    MemoryContext parent,
    const char *name,
    Size minContextSize,
    Size initBlockSize,
    Size maxBlockSize)
{
    AllocSetContext *set = (AllocSetContext *)calloc(1, sizeof(AllocSetContext));
    if (!set) {
        return NULL;
    }

    set->header.type = T_AllocSetContext;
    set->header.parent = parent;
    set->header.firstchild = NULL;
    set->header.prevchild = NULL;
    set->header.nextchild = NULL;
    set->header.methods = &g_allocset_methods;
    set->header.name = name;
    set->header.mem_allocated = 0;
    set->header.isReset = false;

    set->blocks = NULL;
    set->initBlockSize = (initBlockSize == 0) ? ALLOCSET_DEFAULT_BLOCK_SIZE : initBlockSize;
    set->maxBlockSize = (maxBlockSize == 0) ? ALLOCSET_DEFAULT_BLOCK_SIZE : maxBlockSize;

    /* 参数校验 */
    if (set->initBlockSize < ALLOCSET_MIN_BLOCK_SIZE) {
        set->initBlockSize = ALLOCSET_MIN_BLOCK_SIZE;
    }
    if (set->maxBlockSize < set->initBlockSize) {
        set->maxBlockSize = set->initBlockSize;
    }

    /* 链接到父上下文的子链表首部 */
    if (parent) {
        set->header.prevchild = NULL;
        set->header.nextchild = parent->firstchild;
        if (parent->firstchild) {
            parent->firstchild->prevchild = (MemoryContext)set;
        }
        parent->firstchild = (MemoryContext)set;
    }

    /* 预分配首块。
     * minContextSize 语义：保证首块数据区（可分配容量）不小于该值，
     * 因此首块总尺寸下限 = 块头 + minContextSize（做溢出保护）。 */
    Size first_block = set->initBlockSize;
    if (first_block < ALLOCSET_MIN_BLOCK_SIZE) {
        first_block = ALLOCSET_MIN_BLOCK_SIZE;
    }
    if (minContextSize > 0 &&
        minContextSize <= ALLOCSET_MAX_SIZE - ALLOCSET_BLOCK_HDR_SIZE) {
        Size min_total = ALLOCSET_BLOCK_HDR_SIZE + minContextSize;
        if (first_block < min_total) {
            first_block = min_total;
        }
    }
    AllocSetBlock *block = (AllocSetBlock *)malloc(first_block);
    if (!block) {
        if (parent) {
            parent->firstchild = set->header.nextchild;
            if (set->header.nextchild) {
                set->header.nextchild->prevchild = NULL;
            }
        }
        free(set);
        return NULL;
    }
    block->size = first_block;
    block->free = first_block - ALLOCSET_BLOCK_HDR_SIZE;
    block->start = (char *)block + ALLOCSET_BLOCK_HDR_SIZE;
    block->end = (char *)block + first_block;
    block->next = NULL;
    set->blocks = block;

    return (MemoryContext)set;
}

/**
 * @brief 从上下文中分配内存
 */
void *palloc(MemoryContext ctx, Size size)
{
    if (!ctx || !ctx->methods || !ctx->methods->alloc) {
        return NULL;
    }
    ctx->isReset = false;
    return ctx->methods->alloc(ctx, size);
}

/**
 * @brief 从上下文中分配零初始化内存
 */
void *palloc0(MemoryContext ctx, Size size)
{
    void *ptr = palloc(ctx, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/**
 * @brief 释放单个指针（AllocSet 中为空操作）
 */
void pfree(MemoryContext ctx, void *ptr)
{
    if (!ctx || !ctx->methods || !ctx->methods->free_p) {
        return;
    }
    ctx->methods->free_p(ctx, ptr);
}

/**
 * @brief 重置上下文
 */
void reset_memory(MemoryContext ctx)
{
    if (!ctx || !ctx->methods || !ctx->methods->reset) {
        return;
    }
    ctx->methods->reset(ctx);
}

/**
 * @brief 删除上下文
 */
void delete_memory(MemoryContext ctx)
{
    if (!ctx || !ctx->methods || !ctx->methods->delete_ctx) {
        return;
    }
    ctx->methods->delete_ctx(ctx);
}