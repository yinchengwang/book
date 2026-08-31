/**
 * @file blob_gc.c
 * @brief Blob GC 与引用计数管理实现（Task 7）
 *
 * 实现延迟垃圾回收和引用计数保护机制。
 *
 * 引用计数语义：
 * - 在 Blob COMMIT 时，每个 Chunk 的引用计数 +1
 * - 在 Blob DELETE 时，每个 Chunk 的引用计数 -1
 * - 引用计数为 0 时设置 gc_after 时间戳
 *
 * 活动读取者保护：
 * - blob_get/range_get 在打开 Chunk 前调用 reader_enter
 * - 读取完成后调用 reader_exit
 * - GC 只有在 refcount==0 且无活动读取者时删除 Chunk
 *
 * GC 流程：
 * 1. 扫描 catalog 中 refcount=0 的 Chunk
 * 2. 检查 gc_after 是否已到期
 * 3. 检查是否有活动读取者
 * 4. 确认后原子删除 Chunk 文件
 */
#include "db/blob_gc.h"
#include "db/blob_engine.h"
#include "db/blob_catalog.h"
#include "db/blob_manifest.h"
#include "db/blob_reader_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define mkdir_path(path) _mkdir(path)
#define unlink_func(path) _unlink(path)
#define fsync_func(fd) _commit(fd)
#else
#include <unistd.h>
#include <sys/time.h>
#define mkdir_path(path) mkdir(path, 0755)
#define unlink_func(path) unlink(path)
#define fsync_func(fd) fsync(fd)
#endif

/* ========================================================================
 * 日志宏
 * ======================================================================== */

#ifdef BLOB_GC_DEBUG
#include <stdio.h>
#define GC_DEBUG(...) fprintf(stderr, "[blob_gc] " __VA_ARGS__)
#else
#define GC_DEBUG(...) ((void)0)
#endif

/* ========================================================================
 * 活动时间获取
 * ======================================================================== */

#ifdef _WIN32
static int64_t get_time_ms(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    int64_t t = ((int64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000LL;
    return t / 10000;
}
#else
static int64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif

/* ========================================================================
 * 路径构造
 * ======================================================================== */

/**
 * @brief 获取 Chunk 文件路径
 */
static int get_chunk_path(const char *chunks_dir,
                          const uint8_t chunk_id[32],
                          char *path_buf, size_t buf_size) {
    static const char hexc[] = "0123456789abcdef";
    char hex[65];

    for (int i = 0; i < 32; i++) {
        hex[2 * i] = hexc[chunk_id[i] >> 4];
        hex[2 * i + 1] = hexc[chunk_id[i] & 0x0F];
    }
    hex[64] = '\0';

    int n = snprintf(path_buf, buf_size, "%s/%s.chunk", chunks_dir, hex);
    if (n < 0 || (size_t)n >= buf_size) {
        return -1;
    }
    return 0;
}

/* ========================================================================
 * 读者保护接口实现
 * ======================================================================== */

/* blob_engine_get_reader_table 在 blob_engine.h 中声明 */

/* ========================================================================
 * 活动读取者保护
 * ======================================================================== */

void blob_gc_reader_enter(blob_engine_t *engine, const uint8_t chunk_id[32]) {
    if (!engine || !chunk_id) return;

    reader_table_t *table = (reader_table_t *)blob_engine_get_reader_table(engine);
    if (table) {
        uint32_t count = reader_table_inc(table, chunk_id);
        GC_DEBUG("reader_enter: chunk=%02x%02x... count=%u\n",
                 chunk_id[0], chunk_id[1], count);
        (void)count;
    }
}

void blob_gc_reader_exit(blob_engine_t *engine, const uint8_t chunk_id[32]) {
    if (!engine || !chunk_id) return;

    reader_table_t *table = (reader_table_t *)blob_engine_get_reader_table(engine);
    if (table) {
        uint32_t count = reader_table_dec(table, chunk_id);
        GC_DEBUG("reader_exit: chunk=%02x%02x... count=%u\n",
                 chunk_id[0], chunk_id[1], count);
        (void)count;
    }
}

bool blob_gc_has_active_readers(blob_engine_t *engine,
                                const uint8_t chunk_id[32]) {
    if (!engine || !chunk_id) return true;  /* 安全起见 */

    reader_table_t *table = (reader_table_t *)blob_engine_get_reader_table(engine);
    if (!table) return false;

    return reader_table_get(table, chunk_id) > 0;
}

/* ========================================================================
 * GC 运行
 * ======================================================================== */

/**
 * @brief 删除单个 Chunk 文件
 */
static int delete_chunk_file(const char *chunks_dir, const uint8_t chunk_id[32]) {
    char path[1024];
    if (get_chunk_path(chunks_dir, chunk_id, path, sizeof(path)) != 0) {
        return -1;
    }

    /* 检查文件是否存在 */
    struct stat st;
    if (stat(path, &st) != 0) {
        /* 文件不存在，可能已经被删除 */
        return 0;
    }

    /* 删除文件 */
    if (unlink_func(path) != 0) {
        GC_DEBUG("delete_chunk_file: failed to delete %s\n", path);
        return -1;
    }

    GC_DEBUG("delete_chunk_file: deleted %s\n", path);
    return 0;
}

int blob_gc_run(blob_engine_t *engine) {
    if (!engine) return -1;

    /* 获取 catalog */
    blob_catalog_t *catalog = blob_engine_get_catalog(engine);
    if (!catalog) {
        GC_DEBUG("gc_run: no catalog available\n");
        return -1;
    }

    const char *chunks_dir = blob_engine_get_chunks_dir(engine);
    if (!chunks_dir) {
        return -1;
    }

    reader_table_t *readers = (reader_table_t *)blob_engine_get_reader_table(engine);

    /* 获取当前时间 */
    int64_t now = get_time_ms();

    GC_DEBUG("gc_run: started at %lld\n", (long long)now);

    /* 创建 Chunk 迭代器 */
    blob_catalog_chunk_iter_t *iter = blob_catalog_chunk_iter_create(catalog);
    if (!iter) {
        return -1;
    }

    int deleted_count = 0;

    /* 遍历所有 Chunk 引用 */
    blob_chunk_ref_t ref;
    while (blob_catalog_chunk_iter_next(iter, &ref) == BLOB_CATALOG_OK) {
        /* 检查是否满足 GC 条件：
         * 1. ref_count == 0
         * 2. gc_after 已到期
         * 3. 无活动读取者
         */
        if (ref.ref_count != 0) {
            continue;
        }

        if (ref.gc_after_ms == 0) {
            continue;
        }

        if (ref.gc_after_ms > now) {
            /* 宽限期未到 */
            GC_DEBUG("gc_run: chunk %02x%02x... gc_after not reached (%lld > %lld)\n",
                     ref.chunk_id[0], ref.chunk_id[1],
                     (long long)ref.gc_after_ms, (long long)now);
            continue;
        }

        /* 检查活动读取者 */
        if (readers && reader_table_get(readers, ref.chunk_id) > 0) {
            GC_DEBUG("gc_run: chunk %02x%02x... has active readers\n",
                     ref.chunk_id[0], ref.chunk_id[1]);
            continue;
        }

        /* 确认 ref_count 仍为 0（双重检查） */
        blob_chunk_ref_t current_ref;
        if (blob_catalog_find_chunk(catalog, ref.chunk_id, &current_ref) == BLOB_CATALOG_OK) {
            if (current_ref.ref_count != 0) {
                GC_DEBUG("gc_run: chunk %02x%02x... ref_count changed\n",
                         ref.chunk_id[0], ref.chunk_id[1]);
                continue;
            }
        }

        /* 删除 Chunk 文件 */
        GC_DEBUG("gc_run: deleting chunk %02x%02x...\n",
                 ref.chunk_id[0], ref.chunk_id[1]);

        int rc = delete_chunk_file(chunks_dir, ref.chunk_id);
        if (rc == 0) {
            deleted_count++;
        }
    }

    blob_catalog_chunk_iter_destroy(iter);

    GC_DEBUG("gc_run: completed, deleted %d chunks\n", deleted_count);

    (void)deleted_count;  /* 暂时不使用返回值 */
    return 0;
}

int blob_gc_stats(blob_engine_t *engine, uint64_t *out_pending_gc) {
    if (!engine) return -1;

    /* 获取 catalog */
    blob_catalog_t *catalog = blob_engine_get_catalog(engine);
    if (!catalog) return -1;

    int64_t now = get_time_ms();
    uint64_t pending = 0;

    /* 遍历所有 Chunk 引用，统计可 GC 的数量 */
    blob_catalog_chunk_iter_t *iter = blob_catalog_chunk_iter_create(catalog);
    if (!iter) return -1;

    blob_chunk_ref_t ref;
    while (blob_catalog_chunk_iter_next(iter, &ref) == BLOB_CATALOG_OK) {
        if (ref.ref_count == 0 && ref.gc_after_ms > 0 && ref.gc_after_ms <= now) {
            pending++;
        }
    }

    blob_catalog_chunk_iter_destroy(iter);

    if (out_pending_gc) {
        *out_pending_gc = pending;
    }

    return 0;
}
