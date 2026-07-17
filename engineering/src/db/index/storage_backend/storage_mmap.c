/**
 * @file storage_mmap.c
 * @brief 内存映射存储后端实现
 *
 * 基于 mmap（POSIX）/ MapViewOfFile（Windows）的页面级存储后端。
 * 文件被映射到进程的虚拟地址空间，读写页面通过内存拷贝完成，
 * 由 OS 负责将脏页回写到磁盘。适合大索引和需要随机访问的场景。
 *
 * 跨平台设计：
 * - POSIX 平台使用 mmap/munmap/msync
 * - Windows 平台使用 CreateFileMapping/MapViewOfFile/FlushViewOfFile
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "storage_backend.h"

/* ============================================================
 * 平台相关头文件与类型
 * ============================================================ */

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <direct.h>

/* Windows mkdir */
#define MMAP_MKDIR(path) _mkdir(path)
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* POSIX mkdir */
#define MMAP_MKDIR(path) mkdir(path, 0755)
#endif

/* ============================================================
 * 常量
 * ============================================================ */

/** 默认页面大小（与 db/page.h 的 DEFAULT_PAGE_SIZE 一致） */
#define MMAP_DEFAULT_PAGE_SIZE 8192u

/** 默认初始映射大小（4 个页面） */
#define MMAP_INITIAL_MAPPED_PAGES 4u

/* ============================================================
 * 平台抽象层
 * ============================================================ */

#ifdef _WIN32

/** Windows 句柄封装，用于替代 POSIX fd */
typedef struct {
    HANDLE file_handle;     /**< CreateFile 返回的句柄 */
    HANDLE mapping_handle;  /**< CreateFileMapping 返回的句柄 */
    void  *map_addr;        /**< MapViewOfFile 返回的基址 */
} mmap_platform_handle_t;

/**
 * @brief 打开或创建文件
 * @return 0 成功，非 0 失败
 */
static int mmap_platform_open(const char *path, mmap_platform_handle_t *ph)
{
    ph->file_handle = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (ph->file_handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    ph->mapping_handle = NULL;
    ph->map_addr = NULL;
    return 0;
}

/**
 * @brief 关闭并清理
 */
static void mmap_platform_close(mmap_platform_handle_t *ph)
{
    if (ph->map_addr != NULL) {
        UnmapViewOfFile(ph->map_addr);
        ph->map_addr = NULL;
    }
    if (ph->mapping_handle != NULL) {
        CloseHandle(ph->mapping_handle);
        ph->mapping_handle = NULL;
    }
    if (ph->file_handle != NULL && ph->file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(ph->file_handle);
        ph->file_handle = INVALID_HANDLE_VALUE;
    }
}

/**
 * @brief 截断/扩展文件到指定大小
 */
static int mmap_platform_truncate(mmap_platform_handle_t *ph, uint64_t new_size)
{
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)new_size;
    if (!SetFilePointerEx(ph->file_handle, li, NULL, FILE_BEGIN)) {
        return -1;
    }
    if (!SetEndOfFile(ph->file_handle)) {
        return -1;
    }
    return 0;
}

/**
 * @brief 获取文件大小
 */
static int64_t mmap_platform_file_size(mmap_platform_handle_t *ph)
{
    LARGE_INTEGER li;
    if (!GetFileSizeEx(ph->file_handle, &li)) {
        return -1;
    }
    return (int64_t)li.QuadPart;
}

/**
 * @brief 建立或重建内存映射
 * @param new_size 新的映射大小
 * @return 映射基址，失败返回 NULL
 */
static void *mmap_platform_map(mmap_platform_handle_t *ph, size_t new_size)
{
    /* 若已存在映射，先解除 */
    if (ph->map_addr != NULL) {
        UnmapViewOfFile(ph->map_addr);
        ph->map_addr = NULL;
    }
    if (ph->mapping_handle != NULL) {
        CloseHandle(ph->mapping_handle);
        ph->mapping_handle = NULL;
    }

    /* 创建文件映射对象 */
    ph->mapping_handle = CreateFileMappingA(
        ph->file_handle,
        NULL,
        PAGE_READWRITE,
        (DWORD)((uint64_t)new_size >> 32),
        (DWORD)((uint64_t)new_size & 0xFFFFFFFFu),
        NULL);
    if (ph->mapping_handle == NULL) {
        return NULL;
    }

    /* 映射整个文件 */
    ph->map_addr = MapViewOfFile(
        ph->mapping_handle,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        new_size);
    return ph->map_addr;
}

/**
 * @brief 同步指定范围到磁盘
 */
static int mmap_platform_sync_range(mmap_platform_handle_t *ph,
                                    void *addr, size_t len)
{
    (void)ph;
    if (addr == NULL || len == 0) {
        return 0;
    }
    return FlushViewOfFile(addr, (SIZE_T)len) ? 0 : -1;
}

/**
 * @brief 同步全部映射到磁盘
 */
static int mmap_platform_sync_all(mmap_platform_handle_t *ph)
{
    if (ph->map_addr == NULL) {
        return 0;
    }
    return FlushViewOfFile(ph->map_addr, 0) ? 0 : -1;
}

#else /* POSIX */

/** POSIX 句柄封装 */
typedef struct {
    int fd;                 /**< 文件描述符 */
} mmap_platform_handle_t;

static int mmap_platform_open(const char *path, mmap_platform_handle_t *ph)
{
    ph->fd = open(path, O_RDWR | O_CREAT, 0644);
    if (ph->fd < 0) {
        return -1;
    }
    return 0;
}

static void mmap_platform_close(mmap_platform_handle_t *ph)
{
    if (ph->fd >= 0) {
        close(ph->fd);
        ph->fd = -1;
    }
}

static int mmap_platform_truncate(mmap_platform_handle_t *ph, uint64_t new_size)
{
    if (ftruncate(ph->fd, (off_t)new_size) != 0) {
        return -1;
    }
    return 0;
}

static int64_t mmap_platform_file_size(mmap_platform_handle_t *ph)
{
    struct stat st;
    if (fstat(ph->fd, &st) != 0) {
        return -1;
    }
    return (int64_t)st.st_size;
}

static void *mmap_platform_map(mmap_platform_handle_t *ph, size_t new_size)
{
    void *addr = mmap(NULL, new_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, ph->fd, 0);
    if (addr == MAP_FAILED) {
        return NULL;
    }
    return addr;
}

static int mmap_platform_sync_range(mmap_platform_handle_t *ph,
                                    void *addr, size_t len)
{
    (void)ph;
    if (addr == NULL || len == 0) {
        return 0;
    }
    if (msync(addr, len, MS_SYNC) != 0) {
        return -1;
    }
    return 0;
}

static int mmap_platform_sync_all(mmap_platform_handle_t *ph)
{
    (void)ph;
    /* 由 sync_all 通过 mapped_addr + mapped_size 走 sync_range */
    return 0;
}

#endif /* _WIN32 */

/* ============================================================
 * mmap 后端上下文
 * ============================================================ */

/** mmap 后端上下文 */
typedef struct {
    mmap_platform_handle_t plat;  /**< 平台相关句柄 */
    char *path;                   /**< 文件路径副本 */
    char *map_addr;               /**< 映射起始地址 */
    size_t mapped_size;           /**< 当前映射大小（字节） */
    size_t page_size;             /**< 页面大小（字节） */
    int32_t page_count;           /**< 已分配页面数 */
} mmap_backend_ctx_t;

/* ============================================================
 * 路径辅助
 * ============================================================ */

/**
 * @brief 提取目录路径并创建
 *
 * 支持 POSIX '/' 和 Windows '\\' 两种分隔符。若无目录部分则空操作。
 */
static void mmap_ensure_parent_dir(const char *path)
{
    if (path == NULL) {
        return;
    }

    char dir[512];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(dir)) {
        return;
    }
    memcpy(dir, path, len + 1);

    /* 查找最后一个分隔符 */
    char *last_sep = NULL;
    for (char *p = dir; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            last_sep = p;
        }
    }

    if (last_sep != NULL) {
        *last_sep = '\0';
        /* 单层目录创建，错误忽略（已存在也属正常） */
        MMAP_MKDIR(dir);
    }
}

/* ============================================================
 * 存储后端操作实现
 * ============================================================ */

/**
 * @brief 分配新页面
 *
 * 按需扩展文件大小与映射区域，返回新页面的 ID。
 * 失败时回退 page_count 并返回 INVALID_PAGE_ID。
 */
static page_id_t mmap_alloc_page(void *ctx)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx == NULL) {
        return INVALID_PAGE_ID;
    }

    page_id_t page_id = mctx->page_count++;
    size_t required = (size_t)mctx->page_count * mctx->page_size;

    /* 若当前映射不足，则扩展文件并重建映射 */
    if (required > mctx->mapped_size) {
        /* 计算新大小，至少为初始映射大小 */
        size_t new_size = required;
        if (new_size < (size_t)MMAP_INITIAL_MAPPED_PAGES * mctx->page_size) {
            new_size = (size_t)MMAP_INITIAL_MAPPED_PAGES * mctx->page_size;
        }

        if (mmap_platform_truncate(&mctx->plat, (uint64_t)new_size) != 0) {
            mctx->page_count--;
            return INVALID_PAGE_ID;
        }

        char *new_addr = (char *)mmap_platform_map(&mctx->plat, new_size);
        if (new_addr == NULL) {
            /* 映射失败时尝试回滚扩展 */
            mctx->page_count--;
            return INVALID_PAGE_ID;
        }
        mctx->map_addr = new_addr;
        mctx->mapped_size = new_size;
    }

    /* 新页面内容清零 */
    memset(mctx->map_addr + (size_t)page_id * mctx->page_size,
           0, mctx->page_size);

    return page_id;
}

/**
 * @brief 读取页面
 */
static int mmap_read_page(void *ctx, page_id_t page_id, page_t *page)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx == NULL || page == NULL) {
        return -1;
    }
    if (page_id < 0 || page_id >= mctx->page_count) {
        return -1;
    }
    if (mctx->map_addr == NULL) {
        return -1;
    }

    memcpy(page, mctx->map_addr + (size_t)page_id * mctx->page_size,
           mctx->page_size);
    return 0;
}

/**
 * @brief 写入页面
 */
static int mmap_write_page(void *ctx, page_id_t page_id, const page_t *page)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx == NULL || page == NULL) {
        return -1;
    }
    if (page_id < 0 || page_id >= mctx->page_count) {
        return -1;
    }
    if (mctx->map_addr == NULL) {
        return -1;
    }

    memcpy(mctx->map_addr + (size_t)page_id * mctx->page_size,
           page, mctx->page_size);
    return 0;
}

/**
 * @brief 刷盘指定页面
 */
static int mmap_flush_page(void *ctx, page_id_t page_id)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx == NULL) {
        return -1;
    }
    if (page_id < 0 || page_id >= mctx->page_count) {
        return -1;
    }
    if (mctx->map_addr == NULL) {
        return -1;
    }

    return mmap_platform_sync_range(&mctx->plat,
                                    mctx->map_addr +
                                    (size_t)page_id * mctx->page_size,
                                    mctx->page_size);
}

/**
 * @brief 批量写入多个页面
 */
static int mmap_batch_write(void *ctx, const page_id_t *page_ids,
                            const page_t **pages, int count)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx == NULL || page_ids == NULL || pages == NULL || count <= 0) {
        return -1;
    }
    if (mctx->map_addr == NULL) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        page_id_t pid = page_ids[i];
        if (pid < 0 || pid >= mctx->page_count || pages[i] == NULL) {
            return -1;
        }
        memcpy(mctx->map_addr + (size_t)pid * mctx->page_size,
               pages[i], mctx->page_size);
    }
    return 0;
}

/**
 * @brief 同步所有脏页
 */
static int mmap_sync(void *ctx)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx == NULL) {
        return -1;
    }
    if (mctx->map_addr == NULL || mctx->mapped_size == 0) {
        return 0;
    }

#ifdef _WIN32
    return mmap_platform_sync_all(&mctx->plat);
#else
    return mmap_platform_sync_range(&mctx->plat,
                                    mctx->map_addr,
                                    mctx->mapped_size);
#endif
}

/**
 * @brief 关闭后端并释放资源
 */
static int mmap_close(void *ctx)
{
    mmap_backend_ctx_t *mctx = (mmap_backend_ctx_t *)ctx;
    if (mctx == NULL) {
        return 0;
    }

    mmap_platform_close(&mctx->plat);
    free(mctx->path);
    mctx->path = NULL;
    mctx->map_addr = NULL;
    mctx->mapped_size = 0;
    free(mctx);
    return 0;
}

/* ============================================================
 * 操作接口表
 * ============================================================ */

static const storage_backend_ops_t mmap_ops = {
    .alloc_page  = mmap_alloc_page,
    .read_page   = mmap_read_page,
    .write_page  = mmap_write_page,
    .flush_page  = mmap_flush_page,
    .batch_write = mmap_batch_write,
    .sync        = mmap_sync,
    .close       = mmap_close,
};

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * @brief 创建 mmap 存储后端
 *
 * @param path 文件路径（必须非 NULL）
 * @param page_size 页面大小（字节），传 0 使用默认值 8192
 * @return 新创建的后端指针；失败返回 NULL
 *
 * @note 调用方负责通过 storage_backend_destroy 释放返回的指针。
 */
storage_backend_t *storage_mmap_create(const char *path, size_t page_size)
{
    if (path == NULL) {
        return NULL;
    }

    /* page_size == 0 时使用默认值 */
    if (page_size == 0) {
        page_size = MMAP_DEFAULT_PAGE_SIZE;
    }

    if (page_size < STORAGE_BACKEND_MIN_PAGE_SIZE ||
        page_size > STORAGE_BACKEND_MAX_PAGE_SIZE) {
        return NULL;
    }

    /* 确保父目录存在 */
    mmap_ensure_parent_dir(path);

    /* 分配上下文 */
    mmap_backend_ctx_t *ctx = (mmap_backend_ctx_t *)calloc(
        1, sizeof(mmap_backend_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->page_size = page_size;
#ifdef _WIN32
    ctx->plat.file_handle = INVALID_HANDLE_VALUE;
#else
    ctx->plat.fd = -1;
#endif

    /* 复制路径 */
    ctx->path = strdup(path);
    if (ctx->path == NULL) {
        free(ctx);
        return NULL;
    }

    /* 打开文件 */
    if (mmap_platform_open(path, &ctx->plat) != 0) {
        free(ctx->path);
        free(ctx);
        return NULL;
    }

    /* 获取已有文件大小并按页面对齐映射 */
    int64_t file_size = mmap_platform_file_size(&ctx->plat);
    if (file_size < 0) {
        mmap_platform_close(&ctx->plat);
        free(ctx->path);
        free(ctx);
        return NULL;
    }

    if (file_size > 0) {
        /* 向下对齐到 page_size 边界 */
        size_t aligned_size = (size_t)file_size;
        size_t rem = aligned_size % page_size;
        if (rem != 0) {
            aligned_size -= rem;
        }
        if (aligned_size == 0) {
            aligned_size = (size_t)MMAP_INITIAL_MAPPED_PAGES * page_size;
            if (mmap_platform_truncate(&ctx->plat,
                                       (uint64_t)aligned_size) != 0) {
                mmap_platform_close(&ctx->plat);
                free(ctx->path);
                free(ctx);
                return NULL;
            }
        }
        ctx->page_count = (int32_t)(aligned_size / page_size);

        char *addr = (char *)mmap_platform_map(&ctx->plat, aligned_size);
        if (addr == NULL) {
            mmap_platform_close(&ctx->plat);
            free(ctx->path);
            free(ctx);
            return NULL;
        }
        ctx->map_addr = addr;
        ctx->mapped_size = aligned_size;
    } else {
        /* 新文件：保持零页面，延迟到首次 alloc_page 时分配 */
        ctx->page_count = 0;
        ctx->map_addr = NULL;
        ctx->mapped_size = 0;
    }

    /* 分配后端结构 */
    storage_backend_t *backend = (storage_backend_t *)malloc(
        sizeof(storage_backend_t));
    if (backend == NULL) {
        mmap_platform_close(&ctx->plat);
        free(ctx->path);
        free(ctx);
        return NULL;
    }

    backend->type      = STORAGE_BACKEND_MMAP;
    backend->ctx       = ctx;
    backend->page_size = page_size;
    backend->ops       = &mmap_ops;

    return backend;
}
