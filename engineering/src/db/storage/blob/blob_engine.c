/**
 * @file blob_engine.c
 * @brief Blob 存储引擎实现（C3-1）
 *
 * 分块存储 + SHA-256 内容寻址 + 独立 Catalog
 *
 * 目录布局：
 * - chunks/：Chunk 文件 {chunk_sha256}.chunk
 * - manifests/：Manifest 文件 {blob_sha256}.manifest
 * - uploads/：上传临时文件
 * - catalog/：Catalog WAL 和 checkpoint
 *
 * Task 7 增强：引用计数、延迟 GC 与启动恢复
 */
#include "db/blob_engine.h"
#include "db/blob_upload.h"
#include "db/blob_manifest.h"
#include "db/blob_catalog.h"
#include "db/blob_gc.h"
#include "db/sha256.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define mkdir_path(path) _mkdir(path)
#define fsync_func(fd) _commit(fd)
#else
#include <unistd.h>
#include <sys/time.h>
#define mkdir_path(path) mkdir(path, 0755)
#define fsync_func(fd) fsync(fd)
#endif

/* ========================================================================
 * 读者计数表（来自 blob_gc.c）
 * ======================================================================== */

/**
 * @brief 读者计数条目
 */
typedef struct reader_entry_s {
    uint8_t  chunk_id[32];      /**< Chunk ID */
    uint32_t reader_count;      /**< 当前读者数量 */
    bool     occupied;          /**< 是否被占用 */
} reader_entry_t;

/**
 * @brief 读者计数表
 */
typedef struct reader_table_s {
    reader_entry_t *entries;
    size_t          size;
} reader_table_t;

#define READER_TABLE_SIZE 1024

/**
 * @brief 创建读者计数表
 */
static reader_table_t *reader_table_create(void) {
    reader_table_t *table = (reader_table_t *)calloc(1, sizeof(reader_table_t));
    if (!table) return NULL;

    table->size = READER_TABLE_SIZE;
    table->entries = (reader_entry_t *)calloc(table->size, sizeof(reader_entry_t));
    if (!table->entries) {
        free(table);
        return NULL;
    }

    return table;
}

/**
 * @brief 销毁读者计数表
 */
static void reader_table_destroy(reader_table_t *table) {
    if (!table) return;
    free(table->entries);
    free(table);
}

/**
 * @brief DJB2 哈希函数
 */
static size_t hash_chunk_id(const uint8_t chunk_id[32]) {
    uint32_t h = 5381;
    for (int i = 0; i < 32; i++) {
        h = ((h << 5) + h) ^ chunk_id[i];
    }
    return h;
}

/**
 * @brief 增加读者计数
 */
static uint32_t reader_table_inc(reader_table_t *table, const uint8_t chunk_id[32]) {
    size_t idx = hash_chunk_id(chunk_id) % table->size;

    for (size_t i = 0; i < table->size; i++) {
        size_t pos = (idx + i) % table->size;

        if (!table->entries[pos].occupied) {
            memcpy(table->entries[pos].chunk_id, chunk_id, 32);
            table->entries[pos].reader_count = 1;
            table->entries[pos].occupied = true;
            return 1;
        }

        if (memcmp(table->entries[pos].chunk_id, chunk_id, 32) == 0) {
            table->entries[pos].reader_count++;
            return table->entries[pos].reader_count;
        }
    }

    return 0;
}

/**
 * @brief 减少读者计数
 */
static uint32_t reader_table_dec(reader_table_t *table, const uint8_t chunk_id[32]) {
    size_t idx = hash_chunk_id(chunk_id) % table->size;

    for (size_t i = 0; i < table->size; i++) {
        size_t pos = (idx + i) % table->size;

        if (!table->entries[pos].occupied) {
            continue;
        }

        if (memcmp(table->entries[pos].chunk_id, chunk_id, 32) == 0) {
            if (table->entries[pos].reader_count > 0) {
                table->entries[pos].reader_count--;
            }
            return table->entries[pos].reader_count;
        }
    }

    return 0;
}

/**
 * @brief 获取读者计数
 */
static uint32_t reader_table_get(reader_table_t *table, const uint8_t chunk_id[32]) {
    size_t idx = hash_chunk_id(chunk_id) % table->size;

    for (size_t i = 0; i < table->size; i++) {
        size_t pos = (idx + i) % table->size;

        if (!table->entries[pos].occupied) {
            continue;
        }

        if (memcmp(table->entries[pos].chunk_id, chunk_id, 32) == 0) {
            return table->entries[pos].reader_count;
        }
    }

    return 0;
}

/* ========================================================================
 * 引擎内部结构
 * ======================================================================== */

struct blob_engine_s {
    char data_dir[512];              /**< 数据根目录 */
    char chunks_dir[600];            /**< chunks 目录 */
    char manifests_dir[600];         /**< manifests 目录 */
    char uploads_dir[600];           /**< uploads 目录 */
    blob_catalog_t *catalog;         /**< Catalog 句柄 */
    reader_table_t *readers;         /**< 读者计数表 */
};

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

static void ensure_dir(const char *path) {
    mkdir_path(path);
}

/* ========================================================================
 * 引擎生命周期
 * ======================================================================== */

blob_engine_t *blob_engine_create(const char *data_dir) {
    if (!data_dir) return NULL;

    blob_engine_t *e = (blob_engine_t *)calloc(1, sizeof(blob_engine_t));
    if (!e) return NULL;

    strncpy(e->data_dir, data_dir, sizeof(e->data_dir) - 1);

    /* 创建目录结构 */
    ensure_dir(data_dir);
    snprintf(e->chunks_dir, sizeof(e->chunks_dir), "%s/chunks", data_dir);
    snprintf(e->manifests_dir, sizeof(e->manifests_dir), "%s/manifests", data_dir);
    snprintf(e->uploads_dir, sizeof(e->uploads_dir), "%s/uploads", data_dir);

    ensure_dir(e->chunks_dir);
    ensure_dir(e->manifests_dir);
    ensure_dir(e->uploads_dir);

    /* 打开 Catalog */
    e->catalog = blob_catalog_open(data_dir);
    if (!e->catalog) {
        LOG_WARN("Catalog 打开失败，将使用简化模式");
    }

    /* 初始化读者计数表 */
    e->readers = reader_table_create();

    LOG_INFO("Blob 引擎创建: %s", data_dir);
    return e;
}

blob_engine_t *blob_engine_open(const char *data_dir) {
    return blob_engine_create(data_dir);
}

void blob_engine_close(blob_engine_t *engine) {
    if (!engine) return;

    /* 关闭 Catalog */
    if (engine->catalog) {
        blob_catalog_close(engine->catalog);
        engine->catalog = NULL;
    }

    /* 销毁读者计数表 */
    if (engine->readers) {
        reader_table_destroy(engine->readers);
        engine->readers = NULL;
    }

    free(engine);
}

/* ========================================================================
 * 内部访问器
 * ======================================================================== */

const char *blob_engine_get_data_dir(const blob_engine_t *engine) {
    return engine ? engine->data_dir : NULL;
}

const char *blob_engine_get_chunks_dir(const blob_engine_t *engine) {
    return engine ? engine->chunks_dir : NULL;
}

const char *blob_engine_get_manifests_dir(const blob_engine_t *engine) {
    return engine ? engine->manifests_dir : NULL;
}

blob_catalog_t *blob_engine_get_catalog(blob_engine_t *engine) {
    return engine ? engine->catalog : NULL;
}

void *blob_engine_get_reader_table(blob_engine_t *engine) {
    return engine ? (void *)engine->readers : NULL;
}

/* ========================================================================
 * 活动读取者保护
 * ======================================================================== */

void blob_engine_reader_enter(blob_engine_t *engine,
                              const uint8_t chunk_id[BLOB_SHA256_SIZE]) {
    if (!engine || !chunk_id) return;
    if (engine->readers) {
        reader_table_inc(engine->readers, chunk_id);
    }
}

void blob_engine_reader_exit(blob_engine_t *engine,
                             const uint8_t chunk_id[BLOB_SHA256_SIZE]) {
    if (!engine || !chunk_id) return;
    if (engine->readers) {
        reader_table_dec(engine->readers, chunk_id);
    }
}

/* ========================================================================
 * 一次性 API
 * ======================================================================== */

int blob_put(blob_engine_t *engine, const void *data, size_t len,
             uint8_t out_blob_id[BLOB_SHA256_SIZE]) {
    if (!engine || !data || len == 0 || !out_blob_id) return -1;

    /* 使用流式 Upload API */
    blob_upload_t *upload = blob_upload_begin(engine, NULL);
    if (!upload) return -1;

    int rc = blob_upload_write(upload, data, len);
    if (rc != BLOB_UPLOAD_OK) {
        blob_upload_abort(upload);
        return -1;
    }

    rc = blob_upload_finish(upload, out_blob_id);
    if (rc != BLOB_UPLOAD_OK) {
        return -1;
    }

    /* upload 内部已释放，无需手动释放 */

    return 0;
}

int blob_get(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE],
             void *out_buf, size_t buf_len, size_t *out_read) {
    if (!engine || !blob_id || !out_buf || !out_read) return -1;
    *out_read = 0;

    /* 从 Manifest 读取 Blob */
    blob_manifest_t *manifest = NULL;
    int rc = blob_manifest_load_checked(engine->manifests_dir, blob_id, &manifest);
    if (rc != BLOB_OK) {
        return -1;
    }

    /* 检查缓冲区大小 */
    if (buf_len < manifest->header.blob_size) {
        blob_manifest_free(manifest);
        return -1;
    }

    /* 逐 Chunk 读取，登记活动读取者 */
    size_t total_read = 0;
    for (uint32_t i = 0; i < manifest->chunk_count; i++) {
        const blob_manifest_chunk_t *chunk = &manifest->chunks[i];
        uint8_t *dest = (uint8_t *)out_buf + chunk->logical_offset;

        /* 登记读者 */
        blob_engine_reader_enter(engine, chunk->chunk_sha256);

        size_t chunk_read = 0;
        rc = blob_chunk_read_checked(engine->chunks_dir,
                                    chunk->chunk_sha256,
                                    dest,
                                    chunk->chunk_size,
                                    &chunk_read);

        /* 注销读者 */
        blob_engine_reader_exit(engine, chunk->chunk_sha256);

        if (rc != BLOB_OK) {
            blob_manifest_free(manifest);
            return -1;
        }

        total_read += chunk_read;
    }

    *out_read = total_read;
    blob_manifest_free(manifest);
    return 0;
}

/* ========================================================================
 * blob_delete：引用计数与延迟 GC
 * ======================================================================== */

int blob_delete(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE]) {
    if (!engine || !blob_id) return -1;

    /* 如果没有 Catalog，无法执行删除 */
    if (!engine->catalog) {
        LOG_WARN("blob_delete: no catalog available");
        return -1;
    }

    /* 1. 查找 Blob 条目 */
    blob_entry_t entry;
    int rc = blob_catalog_find_blob(engine->catalog, blob_id, &entry);
    if (rc != BLOB_CATALOG_OK) {
        return -1;
    }

    /* 只能删除已提交的 Blob */
    if (entry.state != BLOB_STATE_COMMITTED) {
        LOG_WARN("blob_delete: blob not committed (state=%d)", entry.state);
        return -1;
    }

    /* 2. 加载 Manifest 获取 Chunk 列表 */
    blob_manifest_t *manifest = NULL;
    rc = blob_manifest_load_checked(engine->manifests_dir, blob_id, &manifest);
    if (rc != BLOB_OK) {
        return -1;
    }

    /* 3. 开始事务 */
    rc = blob_catalog_begin(engine->catalog);
    if (rc != BLOB_CATALOG_OK) {
        blob_manifest_free(manifest);
        return -1;
    }

    /* 4. 对每个 Chunk 执行 REF_DEC */
    for (uint32_t i = 0; i < manifest->chunk_count; i++) {
        const blob_manifest_chunk_t *chunk = &manifest->chunks[i];
        rc = blob_catalog_ref_dec(engine->catalog, chunk->chunk_sha256);
        if (rc != BLOB_CATALOG_OK) {
            /* REF_DEC 失败，继续处理其他 Chunk */
            LOG_WARN("blob_delete: ref_dec failed for chunk %u", i);
        }
    }

    /* 5. 写 BLOB_DELETE WAL */
    rc = blob_catalog_delete(engine->catalog, blob_id);
    if (rc != BLOB_CATALOG_OK) {
        blob_manifest_free(manifest);
        blob_catalog_end(engine->catalog);
        return -1;
    }

    /* 6. 提交事务（fsync WAL） */
    rc = blob_catalog_end(engine->catalog);
    if (rc != BLOB_CATALOG_OK) {
        blob_manifest_free(manifest);
        return -1;
    }

    /* 7. 删除 Manifest 文件（原子操作） */
    char manifest_path[1024];
    char hex[65];
    static const char hexc[] = "0123456789abcdef";
    for (int j = 0; j < 32; j++) {
        hex[2 * j] = hexc[blob_id[j] >> 4];
        hex[2 * j + 1] = hexc[blob_id[j] & 0x0F];
    }
    hex[64] = '\0';

    uint32_t deleted_chunk_count = manifest->chunk_count;

    snprintf(manifest_path, sizeof(manifest_path), "%s/%s.manifest",
             engine->manifests_dir, hex);
    remove(manifest_path);

    blob_manifest_free(manifest);

    LOG_INFO("blob_delete: deleted blob_id=%s, chunks=%u",
             hex, deleted_chunk_count);

    return 0;
}

int blob_stat(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE],
              size_t *out_len) {
    if (!engine || !blob_id || !out_len) return -1;

    /* 从 Manifest 读取头部 */
    blob_manifest_t *manifest = NULL;
    int rc = blob_manifest_load_checked(engine->manifests_dir, blob_id, &manifest);
    if (rc != BLOB_OK) {
        return -1;
    }

    *out_len = manifest->header.blob_size;
    blob_manifest_free(manifest);
    return 0;
}

int blob_range_get(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE],
                   size_t offset, size_t len,
                   void *out_buf, size_t buf_len, size_t *out_read) {
    if (!engine || !blob_id || !out_buf || !out_read) return -1;
    *out_read = 0;

    /* 从 Manifest 读取头部 */
    blob_manifest_t *manifest = NULL;
    int rc = blob_manifest_load_checked(engine->manifests_dir, blob_id, &manifest);
    if (rc != BLOB_OK) {
        return -1;
    }

    /* 检查偏移量 */
    if (offset >= manifest->header.blob_size) {
        blob_manifest_free(manifest);
        return -1;
    }

    /* 计算实际读取长度 */
    size_t actual_len = len;
    if (offset + actual_len > manifest->header.blob_size) {
        actual_len = manifest->header.blob_size - offset;
    }
    if (actual_len > buf_len) {
        actual_len = buf_len;
    }

    /* 二分查找第一个覆盖 offset 的 Chunk */
    uint32_t start_chunk = 0;
    for (uint32_t i = 0; i < manifest->chunk_count; i++) {
        const blob_manifest_chunk_t *chunk = &manifest->chunks[i];
        if (chunk->logical_offset + chunk->chunk_size > offset) {
            start_chunk = i;
            break;
        }
    }

    /* 读取覆盖范围的 Chunk */
    size_t bytes_read = 0;
    size_t remaining = actual_len;

    for (uint32_t i = start_chunk; i < manifest->chunk_count && remaining > 0; i++) {
        const blob_manifest_chunk_t *chunk = &manifest->chunks[i];

        /* 计算本 Chunk 需要读取的字节数 */
        size_t chunk_start = (offset > chunk->logical_offset) ? offset : chunk->logical_offset;
        size_t chunk_end = chunk->logical_offset + chunk->chunk_size;
        size_t read_start = chunk_start - chunk->logical_offset;
        size_t read_len = chunk_end - chunk_start;
        if (read_len > remaining) {
            read_len = remaining;
        }

        /* 登记读者 */
        blob_engine_reader_enter(engine, chunk->chunk_sha256);

        /* 读取整个 Chunk 到临时缓冲区 */
        uint8_t *temp_buf = (uint8_t *)malloc(chunk->chunk_size);
        if (!temp_buf) {
            blob_engine_reader_exit(engine, chunk->chunk_sha256);
            blob_manifest_free(manifest);
            return -1;
        }

        size_t chunk_read = 0;
        rc = blob_chunk_read_checked(engine->chunks_dir,
                                    chunk->chunk_sha256,
                                    temp_buf,
                                    chunk->chunk_size,
                                    &chunk_read);

        /* 注销读者 */
        blob_engine_reader_exit(engine, chunk->chunk_sha256);

        if (rc != BLOB_OK) {
            free(temp_buf);
            blob_manifest_free(manifest);
            return -1;
        }

        /* 复制需要的部分 */
        memcpy((uint8_t *)out_buf + bytes_read, temp_buf + read_start, read_len);
        free(temp_buf);

        bytes_read += read_len;
        remaining -= read_len;
    }

    *out_read = bytes_read;
    blob_manifest_free(manifest);
    return 0;
}

/* ========================================================================
 * GC 运行（使用 blob_gc.c 中的实现）
 * ======================================================================== */

/* blob_gc_run 和 blob_gc_stats 的实现位于 blob_gc.c */
