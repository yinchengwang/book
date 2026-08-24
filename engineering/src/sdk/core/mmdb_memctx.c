/**
 * @file mmdb_memctx.c
 * @brief SDK 兼容层：内存上下文（Memory Context）实现
 *
 * 在 SDK 层以 `mmdb_mem_*` 前缀对外暴露 AllocSet 内存上下文能力。
 * 实现遵循以下原则：
 * 1. 参数校验前置：空指针 / 0 字节 / 跨线程调用一律安全返回
 * 2. 行为对齐核心 API：所有分配行为通过核心 `MemoryContext` 完成
 * 3. realloc 通过 alloc + memcpy + 释放旧块 模拟
 * 4. 限额由 SDK 层兜底：核心 AllocSet 不内置限额，本层做预检
 */

#include "sdk/impl/mmdb_memctx.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* SDK 层默认块大小（8KB，与核心默认值一致） */
#define MMDB_MEMCTX_INIT_BLOCK_SIZE (8 * 1024)

/* SDK 层最大块大小（1MB，避免单次分配超大块） */
#define MMDB_MEMCTX_MAX_BLOCK_SIZE  (1024 * 1024)

/**
 * @brief 内部辅助：限额预检
 *
 * @param ctx  内存上下文
 * @param size 请求字节数
 *
 * @return true 表示通过预检，可继续分配；false 表示超过限额
 */
static bool mmdb_mem_check_limit(mmdb_memctx_t ctx, Size size) {
    /* max_bytes == 0 表示不限制，直接通过 */
    if (ctx->max_bytes == 0) {
        return true;
    }
    /* 单次分配即超过限额时直接拒绝（同时防止后续减法下溢） */
    if (size > ctx->max_bytes) {
        return false;
    }
    /* 使用减法避免 current_bytes + size 溢出 */
    if (ctx->current_bytes > ctx->max_bytes - size) {
        return false;
    }
    return true;
}

mmdb_memctx_t mmdb_memctx_create(mmdb_memctx_t parent,
                                 const char *name,
                                 size_t max_bytes) {
    /* 参数校验：name 必须非空（与核心 AllocSetContextCreate 一致） */
    if (!name) {
        return NULL;
    }

    /* 推导 initBlockSize：若 max_bytes 小于默认值，则按 max_bytes 收敛 */
    Size init_size = MMDB_MEMCTX_INIT_BLOCK_SIZE;
    if (max_bytes > 0 && max_bytes < init_size) {
        init_size = (Size)max_bytes;
    }

    /*
     * 委托给核心 AllocSet 分配器创建上下文（5 个参数）：
     *   parent, name, minContextSize, initBlockSize, maxBlockSize
     * 核心 AllocSetContextCreate 不支持直接传入 max_bytes，
     * 创建后通过设置 header.max_bytes 字段启用限额，由 SDK 层兜底预检。
     */
    MemoryContext ctx = AllocSetContextCreate(parent, name, 0, init_size,
                                              MMDB_MEMCTX_MAX_BLOCK_SIZE);
    if (!ctx) {
        return NULL;
    }
    /* 设置内存限额（0 表示不限制） */
    ctx->max_bytes = (Size)max_bytes;
    return ctx;
}

void *mmdb_mem_alloc(mmdb_memctx_t ctx, size_t size) {
    /* 参数校验：空上下文 / 0 字节 / 跨线程一律返回 NULL */
    if (!ctx || size == 0 || !MemoryContextCheckThread(ctx)) {
        return NULL;
    }
    /* 限额预检（核心 AllocSet 不内置限额，由 SDK 层负责） */
    if (!mmdb_mem_check_limit(ctx, (Size)size)) {
        return NULL;
    }
    /* 委托给核心 palloc（自动 8 字节对齐） */
    return palloc(ctx, (Size)size);
}

void *mmdb_mem_calloc(mmdb_memctx_t ctx, size_t count, size_t size) {
    /* 参数校验：空上下文 / 0 元素 / 0 字节 / 溢出均返回 NULL */
    if (!ctx || count == 0 || size == 0 || count > SIZE_MAX / size) {
        return NULL;
    }
    /* 通过 mmdb_mem_alloc 分配，再清零，保证走核心 ctx 校验与限额 */
    size_t total = count * size;
    void *ptr = mmdb_mem_alloc(ctx, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *mmdb_mem_realloc(mmdb_memctx_t ctx, void *ptr, size_t size) {
    /* 参数校验：空上下文 / 跨线程一律返回 NULL */
    if (!ctx || !MemoryContextCheckThread(ctx)) {
        return NULL;
    }
    /* ptr == NULL 等价于 alloc */
    if (!ptr) {
        return mmdb_mem_alloc(ctx, size);
    }
    /* size == 0 等价于 free，返回 NULL */
    if (size == 0) {
        mmdb_mem_free(ctx, ptr);
        return NULL;
    }

    /*
     * AllocSet 是 arena 分配器，palloc 返回的指针前隐藏了
     * MemoryAllocationHeader，无法原地 realloc。
     * 通过 GET_ALLOCATION_HEADER 回溯读取 requested_size，
     * 然后 alloc + memcpy + 旧块交由 reset/delete 统一回收。
     */
    MemoryAllocationHeader *hdr = GET_ALLOCATION_HEADER(ptr);
    size_t old_size = hdr->requested_size;

    void *new_ptr = mmdb_mem_alloc(ctx, size);
    if (!new_ptr) {
        return NULL;
    }
    /* 取新旧较小者做拷贝，避免越界读 */
    memcpy(new_ptr, ptr, old_size < size ? old_size : size);
    return new_ptr;
}

char *mmdb_mem_strdup(mmdb_memctx_t ctx, const char *value) {
    /* 参数校验：空上下文 / 空字符串指针一律返回 NULL */
    if (!ctx || !value) {
        return NULL;
    }
    size_t len = strlen(value);
    /* SIZE_MAX 表示字符串长度溢出，理论上不可能但做防御 */
    if (len == SIZE_MAX) {
        return NULL;
    }
    char *copy = (char *)mmdb_mem_alloc(ctx, len + 1);
    if (copy) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

void mmdb_mem_free(mmdb_memctx_t ctx, void *ptr) {
    /* 参数校验：空上下文 / 空指针 / 跨线程一律安全返回 */
    if (!ctx || !ptr || !MemoryContextCheckThread(ctx)) {
        return;
    }
    /* AllocSet 实现下为空操作，保留接口以兼容 PostgreSQL 风格 */
    pfree(ctx, ptr);
}

void mmdb_memctx_reset(mmdb_memctx_t ctx) {
    /* 空上下文安全返回（与核心 MemoryContextReset 一致） */
    if (ctx) {
        MemoryContextReset(ctx);
    }
}

void mmdb_memctx_delete(mmdb_memctx_t ctx) {
    /* 空上下文安全返回 */
    if (ctx) {
        MemoryContextDelete(ctx);
    }
}
