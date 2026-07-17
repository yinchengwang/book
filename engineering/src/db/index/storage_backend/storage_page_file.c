/**
 * @file storage_page_file.c
 * @brief 页面文件存储后端实现
 *
 * 基于标准 C 文件 I/O 的页面级存储后端。每个页面在文件中按 page_size
 * 对齐顺序存放，通过 fseek 定位读写。适用于中小规模索引的持久化场景。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "storage_backend.h"

/* Windows 平台兼容 */
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

/** 默认页面大小（与 db/page.h 的 DEFAULT_PAGE_SIZE 一致） */
#define PAGE_FILE_DEFAULT_PAGE_SIZE 8192u

/** 页面文件后端上下文 */
typedef struct {
    FILE *file;             /**< 文件句柄 */
    char *path;             /**< 文件路径 */
    size_t page_size;       /**< 页面大小 */
    int32_t page_count;     /**< 总页面数 */
} page_file_ctx_t;

static page_id_t pagefile_alloc_page(void *ctx)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx == NULL) {
        return INVALID_PAGE_ID;
    }
    page_id_t page_id = pctx->page_count++;
    return page_id;
}

static int pagefile_read_page(void *ctx, page_id_t page_id, page_t *page)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx == NULL || pctx->file == NULL || page == NULL) {
        return -1;
    }
    if (page_id < 0 || page_id >= pctx->page_count) {
        return -1;
    }
    if (fseek(pctx->file, (long)(page_id * pctx->page_size), SEEK_SET) != 0) {
        return -1;
    }
    size_t read = fread(page, 1, pctx->page_size, pctx->file);
    return (read == pctx->page_size) ? 0 : -1;
}

static int pagefile_write_page(void *ctx, page_id_t page_id, const page_t *page)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx == NULL || pctx->file == NULL || page == NULL) {
        return -1;
    }
    if (page_id < 0) {
        return -1;
    }
    /* 扩展页面数 */
    if (page_id >= pctx->page_count) {
        pctx->page_count = page_id + 1;
    }
    if (fseek(pctx->file, (long)(page_id * pctx->page_size), SEEK_SET) != 0) {
        return -1;
    }
    size_t written = fwrite(page, 1, pctx->page_size, pctx->file);
    return (written == pctx->page_size) ? 0 : -1;
}

static int pagefile_flush_page(void *ctx, page_id_t page_id)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx == NULL || pctx->file == NULL) {
        return -1;
    }
    (void)page_id;
    return fflush(pctx->file);
}

static int pagefile_batch_write(void *ctx, const page_id_t *page_ids,
                                const page_t **pages, int count)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx == NULL || pctx->file == NULL || page_ids == NULL ||
        pages == NULL || count <= 0) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (pagefile_write_page(ctx, page_ids[i], pages[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int pagefile_sync(void *ctx)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx == NULL || pctx->file == NULL) {
        return -1;
    }
    fflush(pctx->file);
    return 0;
}

static int pagefile_close(void *ctx)
{
    page_file_ctx_t *pctx = (page_file_ctx_t *)ctx;
    if (pctx == NULL) {
        return -1;
    }
    if (pctx->file) {
        fclose(pctx->file);
        pctx->file = NULL;
    }
    free(pctx->path);
    pctx->path = NULL;
    free(pctx);
    return 0;
}

static const storage_backend_ops_t pagefile_ops = {
    .alloc_page  = pagefile_alloc_page,
    .read_page   = pagefile_read_page,
    .write_page  = pagefile_write_page,
    .flush_page  = pagefile_flush_page,
    .batch_write = pagefile_batch_write,
    .sync        = pagefile_sync,
    .close       = pagefile_close,
};

/**
 * @brief 创建页面文件存储后端
 *
 * @param path 文件路径（必须非 NULL）
 * @param page_size 页面大小（字节），传 0 使用默认值 8192
 * @return 新创建的后端指针；失败返回 NULL
 *
 * @note 调用方负责通过 storage_backend_destroy 释放返回的指针。
 */
storage_backend_t *storage_page_file_create(const char *path, size_t page_size)
{
    if (path == NULL) {
        return NULL;
    }

    /* page_size == 0 时使用默认值 */
    if (page_size == 0) {
        page_size = PAGE_FILE_DEFAULT_PAGE_SIZE;
    }

    /* 确保目录存在 */
    char dir[512];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *last_slash = strrchr(dir, '/');
    /* Windows 路径也检查反斜杠 */
    char *last_backslash = strrchr(dir, '\\');
    char *last_sep = last_slash;
    if (last_backslash != NULL &&
        (last_slash == NULL || last_backslash > last_slash)) {
        last_sep = last_backslash;
    }
    if (last_sep) {
        *last_sep = '\0';
        /* 递归创建父目录（忽略已存在的错误） */
        mkdir(dir, 0755);
    }

    /* 分配上下文 */
    page_file_ctx_t *ctx = (page_file_ctx_t *)calloc(1, sizeof(page_file_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->path = strdup(path);
    if (ctx->path == NULL) {
        free(ctx);
        return NULL;
    }
    ctx->page_size = page_size;

    /* 尝试打开已有文件 */
    ctx->file = fopen(path, "r+b");
    if (ctx->file == NULL) {
        /* 文件不存在，创建新文件 */
        ctx->file = fopen(path, "w+b");
        if (ctx->file == NULL) {
            free(ctx->path);
            free(ctx);
            return NULL;
        }
    }

    /* 获取文件大小以确定页面数 */
    fseek(ctx->file, 0, SEEK_END);
    long file_size = ftell(ctx->file);
    if (file_size > 0 && page_size > 0) {
        ctx->page_count = (int32_t)(file_size / (long)page_size);
    } else {
        ctx->page_count = 0;
    }

    /* 分配后端结构 */
    storage_backend_t *backend = (storage_backend_t *)malloc(sizeof(storage_backend_t));
    if (backend == NULL) {
        fclose(ctx->file);
        free(ctx->path);
        free(ctx);
        return NULL;
    }

    backend->type      = STORAGE_BACKEND_PAGE_FILE;
    backend->ctx       = ctx;
    backend->page_size = page_size;
    backend->ops       = &pagefile_ops;

    return backend;
}
