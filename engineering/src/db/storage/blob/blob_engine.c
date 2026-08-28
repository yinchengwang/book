/**
 * @file blob_engine.c
 * @brief Blob 存储引擎实现（C3-1）
 *
 * 分块存储 + SHA-256 内容寻址 + KV catalog 元数据
 * chunk 文件：{data_dir}/chunks/{chunk_id_hex}.bin
 * metadata：{data_dir}/blob_meta/
 */
#include "db/blob_engine.h"
#include "db/blob_upload.h"
#include "db/blob_manifest.h"
#include "db/blob_catalog.h"
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
#define mkdir_path(path) mkdir(path, 0755)
#define fsync_func(fd) fsync(fd)
#endif

struct blob_engine_s {
    char data_dir[512];
    char chunks_dir[600];
    char manifests_dir[600];
    blob_catalog_t *catalog;  /**< Catalog 句柄 */
};

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

static void ensure_dir(const char *path) {
    mkdir_path(path);
}

/**
 * @brief SHA-256 摘要转十六进制字符串
 */
static void hex_str(const uint8_t *bin, char *hex, size_t bin_len) {
    static const char hexc[] = "0123456789abcdef";
    for (size_t i = 0; i < bin_len; ++i) {
        hex[2*i]   = hexc[bin[i] >> 4];
        hex[2*i+1] = hexc[bin[i] & 0x0f];
    }
    hex[2*bin_len] = '\0';
}

/* ========================================================================
 * 引擎生命周期
 * ======================================================================== */

blob_engine_t *blob_engine_create(const char *data_dir) {
    if (!data_dir) return NULL;
    blob_engine_t *e = (blob_engine_t *)calloc(1, sizeof(blob_engine_t));
    if (!e) return NULL;
    strncpy(e->data_dir, data_dir, sizeof(e->data_dir) - 1);

    /* 创建必要目录 */
    ensure_dir(data_dir);
    snprintf(e->chunks_dir, sizeof(e->chunks_dir), "%s/chunks", data_dir);
    snprintf(e->manifests_dir, sizeof(e->manifests_dir), "%s/manifests", data_dir);
    ensure_dir(e->chunks_dir);
    ensure_dir(e->manifests_dir);
    char upload_dir[600];
    snprintf(upload_dir, sizeof(upload_dir), "%s/uploads", data_dir);
    ensure_dir(upload_dir);

    /* 打开 Catalog */
    e->catalog = blob_catalog_open(data_dir);
    if (!e->catalog) {
        LOG_WARN("Catalog 打开失败，将使用简化模式");
        /* 不阻止引擎启动，但功能受限 */
    }

    LOG_INFO("Blob 引擎创建: %s", data_dir);
    return e;
}

blob_engine_t *blob_engine_open(const char *data_dir) {
    return blob_engine_create(data_dir);
}

void blob_engine_close(blob_engine_t *engine) {
    if (!engine) return;
    if (engine->catalog) {
        blob_catalog_close(engine->catalog);
    }
    free(engine);
}

const char *blob_engine_get_data_dir(const blob_engine_t *engine) {
    return engine ? engine->data_dir : NULL;
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

    /* 逐 Chunk 读取 */
    size_t total_read = 0;
    for (uint32_t i = 0; i < manifest->chunk_count; i++) {
        const blob_manifest_chunk_t *chunk = &manifest->chunks[i];
        uint8_t *dest = (uint8_t *)out_buf + chunk->logical_offset;

        size_t chunk_read = 0;
        rc = blob_chunk_read_checked(engine->chunks_dir,
                                    chunk->chunk_sha256,
                                    dest,
                                    chunk->chunk_size,
                                    &chunk_read);
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

int blob_delete(blob_engine_t *engine, const uint8_t blob_id[BLOB_SHA256_SIZE]) {
    if (!engine || !blob_id) return -1;

    /* TODO: 实现引用计数和延迟 GC */
    /* 目前暂时不实现删除，因为需要复杂的引用计数逻辑 */

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

        /* 读取整个 Chunk 到临时缓冲区 */
        uint8_t *temp_buf = (uint8_t *)malloc(chunk->chunk_size);
        if (!temp_buf) {
            blob_manifest_free(manifest);
            return -1;
        }

        size_t chunk_read = 0;
        rc = blob_chunk_read_checked(engine->chunks_dir,
                                    chunk->chunk_sha256,
                                    temp_buf,
                                    chunk->chunk_size,
                                    &chunk_read);
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
