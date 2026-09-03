/**
 * @file storage_memory.c
 * @brief 纯内存存储后端实现（不持久化）
 *
 * 本实现使用动态数组管理页面，所有数据存储在内存中。
 * 适用于临时索引、缓存层或无需持久化的场景。
 */

#include <stdlib.h>
#include <string.h>
#include "storage_backend.h"

/** 内存后端上下文 */
typedef struct {
    page_t **pages;          /**< 页面指针数组 */
    int32_t page_count;      /**< 已分配页面数 */
    int32_t page_capacity;   /**< 页面容量 */
    size_t page_size;        /**< 页面大小（字节） */
} memory_backend_ctx_t;

/* ============================================================
 * 存储后端操作实现
 * ============================================================ */

/**
 * @brief 分配新页面
 *
 * 动态扩展页面数组，分配并初始化新页面。
 * 页面 ID 从 0 开始递增。
 */
static page_id_t memory_alloc_page(void *ctx)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;

    /* 检查是否需要扩容 */
    if (mctx->page_count >= mctx->page_capacity) {
        int32_t new_capacity = mctx->page_capacity * 2;
        if (new_capacity <= mctx->page_capacity) {
            /* 容量溢出 */
            return INVALID_PAGE_ID;
        }

        page_t **new_pages = (page_t **)realloc(mctx->pages,
                                                  new_capacity * sizeof(page_t *));
        if (new_pages == NULL) {
            return INVALID_PAGE_ID;
        }

        mctx->pages = new_pages;
        mctx->page_capacity = new_capacity;
    }

    /* 分配新页面 */
    page_id_t page_id = mctx->page_count++;
    mctx->pages[page_id] = (page_t *)malloc(mctx->page_size);
    if (mctx->pages[page_id] == NULL) {
        mctx->page_count--;
        return INVALID_PAGE_ID;
    }

    /* 初始化为零 */
    memset(mctx->pages[page_id], 0, mctx->page_size);
    return page_id;
}

/**
 * @brief 读取页面
 *
 * 将指定页面的内容复制到输出缓冲区。
 */
static int memory_read_page(void *ctx, page_id_t page_id, page_t *page)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;

    if (page_id < 0 || page_id >= mctx->page_count || page == NULL) {
        return -1;
    }

    memcpy(page, mctx->pages[page_id], mctx->page_size);
    return 0;
}

/**
 * @brief 写入页面
 *
 * 将页面内容复制到指定位置。
 */
static int memory_write_page(void *ctx, page_id_t page_id, const page_t *page)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;

    if (page_id < 0 || page_id >= mctx->page_count || page == NULL) {
        return -1;
    }

    memcpy(mctx->pages[page_id], page, mctx->page_size);
    return 0;
}

/**
 * @brief 刷盘指定页面
 *
 * 纯内存后端无需刷盘，直接返回成功。
 */
static int memory_flush_page(void *ctx, page_id_t page_id)
{
    (void)ctx;
    (void)page_id;
    return 0;
}

/**
 * @brief 批量写入页面
 *
 * 顺序写入多个页面，适用于批量导入场景。
 */
static int memory_batch_write(void *ctx, const page_id_t *page_ids,
                                const page_t **pages, int count)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;

    if (page_ids == NULL || pages == NULL || count <= 0) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        page_id_t pid = page_ids[i];
        if (pid < 0 || pid >= mctx->page_count) {
            return -1;
        }
        memcpy(mctx->pages[pid], pages[i], mctx->page_size);
    }

    return 0;
}

/**
 * @brief 同步所有脏页
 *
 * 纯内存后端无需同步，直接返回成功。
 */
static int memory_sync(void *ctx)
{
    (void)ctx;
    return 0;
}

/**
 * @brief 关闭后端并释放资源
 *
 * 释放所有页面和上下文结构。
 */
static int memory_close(void *ctx)
{
    memory_backend_ctx_t *mctx = (memory_backend_ctx_t *)ctx;

    if (mctx == NULL) {
        return 0;
    }

    /* 释放所有页面 */
    for (int32_t i = 0; i < mctx->page_count; i++) {
        free(mctx->pages[i]);
    }

    /* 释放页面数组和上下文 */
    free(mctx->pages);
    free(mctx);

    return 0;
}

/* ============================================================
 * 操作接口表
 * ============================================================ */

static const storage_backend_ops_t memory_ops = {
    .alloc_page = memory_alloc_page,
    .read_page = memory_read_page,
    .write_page = memory_write_page,
    .flush_page = memory_flush_page,
    .batch_write = memory_batch_write,
    .sync = memory_sync,
    .close = memory_close,
};

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * @brief 创建纯内存存储后端
 *
 * @param page_size 页面大小（字节），必须满足 STORAGE_BACKEND_MIN_PAGE_SIZE
 *                  和 STORAGE_BACKEND_MAX_PAGE_SIZE 的范围要求
 * @return 新创建的后端指针；失败返回 NULL
 */
storage_backend_t *storage_memory_create(size_t page_size)
{
    /* 参数校验：传 0 使用默认页面大小 */
    if (page_size == 0) {
        page_size = 8192;
    }

    if (page_size < STORAGE_BACKEND_MIN_PAGE_SIZE ||
        page_size > STORAGE_BACKEND_MAX_PAGE_SIZE) {
        return NULL;
    }

    /* 分配上下文 */
    memory_backend_ctx_t *ctx = (memory_backend_ctx_t *)calloc(1,
                                                                sizeof(memory_backend_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    /* 初始化上下文 */
    ctx->page_size = page_size;
    ctx->page_capacity = 256;  /* 初始容量 */
    ctx->pages = (page_t **)malloc(ctx->page_capacity * sizeof(page_t *));
    if (ctx->pages == NULL) {
        free(ctx);
        return NULL;
    }

    /* 分配后端结构 */
    storage_backend_t *backend = (storage_backend_t *)malloc(sizeof(storage_backend_t));
    if (backend == NULL) {
        free(ctx->pages);
        free(ctx);
        return NULL;
    }

    /* 设置后端属性 */
    backend->type = STORAGE_BACKEND_MEMORY;
    backend->ctx = ctx;
    backend->page_size = page_size;
    backend->ops = &memory_ops;

    return backend;
}
