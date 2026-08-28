/**
 * @file blob_upload.c
 * @brief 统一流式 Upload Writer 实现（Task 5）
 *
 * 实现流式上传接口，支持任意大小数据的分块写入，
 * 自动管理 Chunk 分块、SHA-256 计算和两阶段发布。
 */
#include "db/blob_upload.h"
#include "db/blob_engine.h"
#include "db/blob_manifest.h"
#include "db/blob_catalog.h"
#include "db/sha256.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define mkdir_path(path) _mkdir(path)
#define fsync_func(fd) _commit(fd)
#else
#include <unistd.h>
#include <sys/time.h>
#define mkdir_path(path) mkdir(path, 0755)
#define fsync_func(fd) fsync(fd)
#endif

/* ========================================================================
 * Upload 会话状态
 * ======================================================================== */

typedef enum {
    BLOB_UPLOAD_ACTIVE   = 0,  /**< 上传中 */
    BLOB_UPLOAD_FINISHING = 1, /**< 正在完成 */
    BLOB_UPLOAD_COMMITTED = 2, /**< 已提交 */
    BLOB_UPLOAD_ABORTED  = 3   /**< 已中止 */
} blob_upload_state_t;

/* ========================================================================
 * Upload 内部结构
 * ======================================================================== */

/** Chunk 清单条目 */
typedef struct blob_upload_chunk_entry_s {
    uint8_t  chunk_sha256[BLOB_SHA256_SIZE]; /**< Chunk ID */
    uint64_t logical_offset;                   /**< 逻辑偏移 */
    uint32_t chunk_size;                       /**< Chunk 大小 */
} blob_upload_chunk_entry_t;

/** Upload 句柄 */
struct blob_upload_s {
    blob_engine_t *engine;                  /**< Blob 引擎句柄 */

    /* 上传会话信息 */
    char upload_id[64];                     /**< 上传会话 ID */
    char chunks_dir[600];                   /**< chunks 目录 */
    char manifests_dir[600];                /**< manifests 目录 */
    char session_dir[600];                  /**< session 临时目录 */

    /* 选项 */
    char content_type[256];                 /**< 内容类型 */
    void *metadata;                         /**< 元数据 */
    size_t metadata_len;                    /**< 元数据长度 */

    /* 流式写入状态 */
    sha256_ctx_t blob_sha_ctx;              /**< 整体 Blob SHA-256 上下文 */
    uint8_t *chunk_buffer;                  /**< 当前 Chunk 缓冲区 */
    size_t chunk_buffer_used;               /**< 缓冲区已用字节数 */
    uint64_t blob_size;                     /**< Blob 总大小 */

    /* Chunk 清单 */
    blob_upload_chunk_entry_t *chunk_entries; /**< Chunk 条目数组 */
    uint32_t chunk_count;                   /**< Chunk 数量 */
    uint32_t chunk_entries_capacity;        /**< 条目数组容量 */

    /* 状态 */
    blob_upload_state_t state;              /**< 当前状态 */
};

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 确保目录存在
 */
static int ensure_dir(const char *dir) {
    if (mkdir_path(dir) != 0 && errno != EEXIST) {
        return BLOB_UPLOAD_ERR_IO;
    }
    return BLOB_UPLOAD_OK;
}

/**
 * @brief 生成唯一 upload_id
 */
static void generate_upload_id(char *buf, size_t buf_size) {
    static int counter = 0;
    #ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    #else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t t = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    #endif
    snprintf(buf, buf_size, "upload_%llu_%d", (unsigned long long)t, counter++);
}

/**
 * @brief 释放上传会话的临时目录
 */
static void cleanup_session_dir(const char *session_dir) {
    /* 删除 session.meta */
    char meta_path[700];
    snprintf(meta_path, sizeof(meta_path), "%s/session.meta", session_dir);
    remove(meta_path);

    /* 删除 parts 目录（如果存在） */
    char parts_dir[700];
    snprintf(parts_dir, sizeof(parts_dir), "%s/parts", session_dir);
    /* 注意：这里不递归删除 parts 目录，因为可能有其他进程在使用 */

    /* 删除 session 目录本身 */
    #ifdef _WIN32
    _rmdir(session_dir);
    #else
    rmdir(session_dir);
    #endif
}

/* ========================================================================
 * Upload 生命周期 API 实现
 * ======================================================================== */

blob_upload_t *blob_upload_begin(blob_engine_t *engine,
                                 const blob_upload_options_t *options) {
    if (!engine) {
        return NULL;
    }

    /* 分配 Upload 结构 */
    blob_upload_t *upload = (blob_upload_t *)calloc(1, sizeof(blob_upload_t));
    if (!upload) {
        return NULL;
    }

    upload->engine = engine;
    upload->state = BLOB_UPLOAD_ACTIVE;

    /* 生成或使用提供的 upload_id */
    if (options && options->upload_id && options->upload_id[0] != '\0') {
        strncpy(upload->upload_id, options->upload_id, sizeof(upload->upload_id) - 1);
    } else {
        generate_upload_id(upload->upload_id, sizeof(upload->upload_id));
    }

    /* 获取 data_dir */
    const char *data_dir = blob_engine_get_data_dir(engine);
    if (!data_dir) {
        free(upload);
        return NULL;
    }

    /* 构造目录路径 */
    snprintf(upload->chunks_dir, sizeof(upload->chunks_dir),
             "%s/chunks", data_dir);
    snprintf(upload->manifests_dir, sizeof(upload->manifests_dir),
             "%s/manifests", data_dir);
    snprintf(upload->session_dir, sizeof(upload->session_dir),
             "%s/uploads/%s", data_dir, upload->upload_id);

    /* 创建目录 */
    if (ensure_dir(upload->chunks_dir) != BLOB_UPLOAD_OK ||
        ensure_dir(upload->manifests_dir) != BLOB_UPLOAD_OK ||
        ensure_dir(upload->session_dir) != BLOB_UPLOAD_OK) {
        free(upload);
        return NULL;
    }

    /* 初始化 SHA-256 上下文 */
    sha256_init(&upload->blob_sha_ctx);

    /* 分配 Chunk 缓冲区 */
    upload->chunk_buffer = (uint8_t *)malloc(BLOB_MAX_CHUNK_SIZE);
    if (!upload->chunk_buffer) {
        free(upload);
        return NULL;
    }
    upload->chunk_buffer_used = 0;

    /* 初始化 Chunk 清单 */
    upload->chunk_entries_capacity = 16;
    upload->chunk_entries = (blob_upload_chunk_entry_t *)calloc(
        upload->chunk_entries_capacity, sizeof(blob_upload_chunk_entry_t));
    if (!upload->chunk_entries) {
        free(upload->chunk_buffer);
        free(upload);
        return NULL;
    }
    upload->chunk_count = 0;

    /* 复制选项 */
    if (options) {
        if (options->content_type) {
            strncpy(upload->content_type, options->content_type, sizeof(upload->content_type) - 1);
        }
        if (options->metadata && options->metadata_len > 0) {
            upload->metadata = malloc(options->metadata_len);
            if (upload->metadata) {
                memcpy(upload->metadata, options->metadata, options->metadata_len);
                upload->metadata_len = options->metadata_len;
            }
        }
    }

    return upload;
}

int blob_upload_write(blob_upload_t *upload, const void *data, size_t len) {
    if (!upload || !data || len == 0) {
        return BLOB_UPLOAD_ERR_INVAL;
    }

    if (upload->state != BLOB_UPLOAD_ACTIVE) {
        return BLOB_UPLOAD_ERR_STATE;
    }

    /* 更新整体 SHA-256 */
    sha256_update(&upload->blob_sha_ctx, data, len);
    upload->blob_size += len;

    /* 将数据追加到当前 Chunk 缓冲区 */
    const uint8_t *src = (const uint8_t *)data;
    size_t remaining = len;

    while (remaining > 0) {
        /* 计算可写入缓冲区的字节数 */
        size_t space = BLOB_MAX_CHUNK_SIZE - upload->chunk_buffer_used;
        size_t to_copy = (remaining < space) ? remaining : space;

        /* 复制数据到缓冲区 */
        memcpy(upload->chunk_buffer + upload->chunk_buffer_used, src, to_copy);
        upload->chunk_buffer_used += to_copy;
        src += to_copy;
        remaining -= to_copy;

        /* 如果缓冲区满，发布 Chunk */
        if (upload->chunk_buffer_used == BLOB_MAX_CHUNK_SIZE) {
            /* 扩展 Chunk 清单（如果需要） */
            if (upload->chunk_count >= upload->chunk_entries_capacity) {
                uint32_t new_capacity = upload->chunk_entries_capacity * 2;
                blob_upload_chunk_entry_t *new_entries = (blob_upload_chunk_entry_t *)realloc(
                    upload->chunk_entries, new_capacity * sizeof(blob_upload_chunk_entry_t));
                if (!new_entries) {
                    return BLOB_UPLOAD_ERR_NOMEM;
                }
                upload->chunk_entries = new_entries;
                upload->chunk_entries_capacity = new_capacity;
            }

            /* 发布 Chunk */
            uint8_t chunk_id[BLOB_SHA256_SIZE];
            int rc = blob_chunk_write_tmp(upload->chunks_dir,
                                         upload->chunk_buffer,
                                         upload->chunk_buffer_used,
                                         upload->upload_id,
                                         chunk_id);
            if (rc != BLOB_OK) {
                return BLOB_UPLOAD_ERR_IO;
            }

            /* 记录 Chunk 条目 */
            blob_upload_chunk_entry_t *entry = &upload->chunk_entries[upload->chunk_count];
            memcpy(entry->chunk_sha256, chunk_id, BLOB_SHA256_SIZE);
            entry->logical_offset = upload->blob_size - upload->chunk_buffer_used;
            entry->chunk_size = (uint32_t)upload->chunk_buffer_used;
            upload->chunk_count++;

            /* 重置缓冲区 */
            upload->chunk_buffer_used = 0;
        }
    }

    return BLOB_UPLOAD_OK;
}

int blob_upload_finish(blob_upload_t *upload,
                       uint8_t out_blob_id[32]) {
    if (!upload || !out_blob_id) {
        return BLOB_UPLOAD_ERR_INVAL;
    }

    if (upload->state != BLOB_UPLOAD_ACTIVE) {
        return BLOB_UPLOAD_ERR_STATE;
    }

    upload->state = BLOB_UPLOAD_FINISHING;

    /* 处理剩余的缓冲区数据 */
    if (upload->chunk_buffer_used > 0) {
        /* 扩展 Chunk 清单（如果需要） */
        if (upload->chunk_count >= upload->chunk_entries_capacity) {
            uint32_t new_capacity = upload->chunk_entries_capacity * 2;
            blob_upload_chunk_entry_t *new_entries = (blob_upload_chunk_entry_t *)realloc(
                upload->chunk_entries, new_capacity * sizeof(blob_upload_chunk_entry_t));
            if (!new_entries) {
                return BLOB_UPLOAD_ERR_NOMEM;
            }
            upload->chunk_entries = new_entries;
            upload->chunk_entries_capacity = new_capacity;
        }

        /* 发布最后一个 Chunk */
        uint8_t chunk_id[BLOB_SHA256_SIZE];
        int rc = blob_chunk_write_tmp(upload->chunks_dir,
                                     upload->chunk_buffer,
                                     upload->chunk_buffer_used,
                                     upload->upload_id,
                                     chunk_id);
        if (rc != BLOB_OK) {
            return BLOB_UPLOAD_ERR_IO;
        }

        /* 记录 Chunk 条目 */
        blob_upload_chunk_entry_t *entry = &upload->chunk_entries[upload->chunk_count];
        memcpy(entry->chunk_sha256, chunk_id, BLOB_SHA256_SIZE);
        entry->logical_offset = upload->blob_size - upload->chunk_buffer_used;
        entry->chunk_size = (uint32_t)upload->chunk_buffer_used;
        upload->chunk_count++;
    }

    /* 计算整体 Blob SHA-256 */
    sha256_final(&upload->blob_sha_ctx, out_blob_id);

    /* C0: 所有 Chunk 完成并 fsync（已在 blob_chunk_write_tmp 中完成） */

    /* C1: Catalog BLOB_PREPARE + WAL fsync */
    /* 注意：这里需要访问 Catalog，但当前实现中 Catalog 是独立的
     * 实际实现中应该从 engine 获取 Catalog 句柄 */
    /* TODO: 实现 Catalog 操作 */

    /* C2: Manifest 临时写入 + fsync + rename */
    blob_manifest_t *manifest = blob_manifest_create(
        upload->chunk_count,
        upload->content_type[0] != '\0' ? upload->content_type : NULL,
        upload->metadata, upload->metadata_len);
    if (!manifest) {
        return BLOB_UPLOAD_ERR_NOMEM;
    }

    /* 填充 Manifest 头部 */
    manifest->header.blob_size = upload->blob_size;
    manifest->header.chunk_size = BLOB_MAX_CHUNK_SIZE;
    memcpy(manifest->header.blob_sha256, out_blob_id, BLOB_SHA256_SIZE);

    /* 填充 Chunk 条目 */
    for (uint32_t i = 0; i < upload->chunk_count; i++) {
        manifest->chunks[i].logical_offset = upload->chunk_entries[i].logical_offset;
        manifest->chunks[i].chunk_size = upload->chunk_entries[i].chunk_size;
        memcpy(manifest->chunks[i].chunk_sha256, upload->chunk_entries[i].chunk_sha256, BLOB_SHA256_SIZE);
        manifest->chunks[i].chunk_checksum = blob_manifest_chunk_checksum(&manifest->chunks[i]);
    }

    /* 写入 Manifest */
    int rc = blob_manifest_write_atomic(upload->manifests_dir, manifest, upload->upload_id);
    blob_manifest_free(manifest);
    if (rc != BLOB_OK) {
        return BLOB_UPLOAD_ERR_IO;
    }

    /* C3: Catalog BLOB_COMMIT + WAL fsync */
    /* TODO: 实现 Catalog 操作 */

    /* C4: 内存状态 COMMITTED */
    upload->state = BLOB_UPLOAD_COMMITTED;

    /* C5: 删除 upload 临时目录 */
    cleanup_session_dir(upload->session_dir);

    return BLOB_UPLOAD_OK;
}

int blob_upload_abort(blob_upload_t *upload) {
    if (!upload) {
        return BLOB_UPLOAD_ERR_INVAL;
    }

    if (upload->state != BLOB_UPLOAD_ACTIVE) {
        return BLOB_UPLOAD_ERR_STATE;
    }

    upload->state = BLOB_UPLOAD_ABORTED;

    /* 删除临时文件 */
    cleanup_session_dir(upload->session_dir);

    /* 注意：已发布的 Chunk 保留，因为可能被其他上传引用
     * 它们会在 GC 阶段被清理 */

    return BLOB_UPLOAD_OK;
}
