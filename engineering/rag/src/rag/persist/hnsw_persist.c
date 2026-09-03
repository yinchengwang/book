/**
 * @file hnsw_persist.c
 * @brief HNSW 索引持久化实现
 */

#include "rag/hnsw_persist.h"
/* 移除 C++ 依赖：#include "rag/logger.h" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <math.h>  /* 提供 isnan() 函数 */

/* 简化的日志宏（兼容 C 编译器） */
#define HNSW_LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define HNSW_LOG_INFO(fmt, ...)  printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define HNSW_LOG_WARN(fmt, ...)  printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define HNSW_LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

// 尝试包含 HNSW 头文件
#ifdef __has_include
#if __has_include(<index/vector_index/hnsw/faiss_hnsw.h>)
#include <index/vector_index/hnsw/faiss_hnsw.h>
#define HAS_FAISS_HNSW 1
#else
#define HAS_FAISS_HNSW 0
#endif
#else
#define HAS_FAISS_HNSW 0
#endif

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static int64_t get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return st.st_size;
}

static void set_error(hnsw_persist_result_t *result, int code, const char *msg) {
    if (result) {
        result->success = 0;
        result->error_code = code;
        if (msg && result->error_msg) {
        #ifdef _WIN32
            strncpy_s(result->error_msg, sizeof(result->error_msg), msg, _TRUNCATE);
        #else
            strncpy(result->error_msg, msg, sizeof(result->error_msg) - 1);
            result->error_msg[sizeof(result->error_msg) - 1] = '\0';
        #endif
        }
    }
}

static void set_success(hnsw_persist_result_t *result, uint64_t bytes) {
    if (result) {
        result->success = 1;
        result->error_code = 0;
        result->bytes_written = bytes;
        result->bytes_read = bytes;
    }
}

/* ========================================================================
 * 头文件读写
 * ======================================================================== */

/**
 * @brief 写入文件头
 */
static int write_header(FILE *fp, const hnsw_persist_meta_t *meta) {
    unsigned char header[HNSW_PERSIST_HEADER_SIZE] = {0};

    // 魔数 (4 bytes)
    header[0] = (meta->magic >> 0) & 0xFF;
    header[1] = (meta->magic >> 8) & 0xFF;
    header[2] = (meta->magic >> 16) & 0xFF;
    header[3] = (meta->magic >> 24) & 0xFF;

    // 版本 (4 bytes)
    header[4] = (meta->version >> 0) & 0xFF;
    header[5] = (meta->version >> 8) & 0xFF;
    header[6] = (meta->version >> 16) & 0xFF;
    header[7] = (meta->version >> 24) & 0xFF;

    // 维度 (4 bytes)
    header[8] = (meta->dims >> 0) & 0xFF;
    header[9] = (meta->dims >> 8) & 0xFF;
    header[10] = (meta->dims >> 16) & 0xFF;
    header[11] = (meta->dims >> 24) & 0xFF;

    // 向量数量 (4 bytes)
    header[12] = (meta->n_total >> 0) & 0xFF;
    header[13] = (meta->n_total >> 8) & 0xFF;
    header[14] = (meta->n_total >> 16) & 0xFF;
    header[15] = (meta->n_total >> 24) & 0xFF;

    // M (4 bytes)
    header[16] = (meta->M >> 0) & 0xFF;
    header[17] = (meta->M >> 8) & 0xFF;
    header[18] = (meta->M >> 16) & 0xFF;
    header[19] = (meta->M >> 24) & 0xFF;

    // ef_construction (4 bytes)
    header[20] = (meta->ef_construction >> 0) & 0xFF;
    header[21] = (meta->ef_construction >> 8) & 0xFF;
    header[22] = (meta->ef_construction >> 16) & 0xFF;
    header[23] = (meta->ef_construction >> 24) & 0xFF;

    // ef_search (4 bytes)
    header[24] = (meta->ef_search >> 0) & 0xFF;
    header[25] = (meta->ef_search >> 8) & 0xFF;
    header[26] = (meta->ef_search >> 16) & 0xFF;
    header[27] = (meta->ef_search >> 24) & 0xFF;

    // max_level (4 bytes)
    header[28] = (meta->max_level >> 0) & 0xFF;
    header[29] = (meta->max_level >> 8) & 0xFF;
    header[30] = (meta->max_level >> 16) & 0xFF;
    header[31] = (meta->max_level >> 24) & 0xFF;

    // entry_pointer (4 bytes)
    header[32] = (meta->entry_pointer >> 0) & 0xFF;
    header[33] = (meta->entry_pointer >> 8) & 0xFF;
    header[34] = (meta->entry_pointer >> 16) & 0xFF;
    header[35] = (meta->entry_pointer >> 24) & 0xFF;

    // metric (4 bytes)
    header[36] = (meta->metric >> 0) & 0xFF;
    header[37] = (meta->metric >> 8) & 0xFF;
    header[38] = (meta->metric >> 16) & 0xFF;
    header[39] = (meta->metric >> 24) & 0xFF;

    // quantization_type (4 bytes)
    header[40] = (meta->quantization_type >> 0) & 0xFF;
    header[41] = (meta->quantization_type >> 8) & 0xFF;
    header[42] = (meta->quantization_type >> 16) & 0xFF;
    header[43] = (meta->quantization_type >> 24) & 0xFF;

    // code_size (4 bytes)
    header[44] = (meta->code_size >> 0) & 0xFF;
    header[45] = (meta->code_size >> 8) & 0xFF;
    header[46] = (meta->code_size >> 16) & 0xFF;
    header[47] = (meta->code_size >> 24) & 0xFF;

    // created_at (8 bytes)
    header[48] = (meta->created_at >> 0) & 0xFF;
    header[49] = (meta->created_at >> 8) & 0xFF;
    header[50] = (meta->created_at >> 16) & 0xFF;
    header[51] = (meta->created_at >> 24) & 0xFF;
    header[52] = (meta->created_at >> 32) & 0xFF;
    header[53] = (meta->created_at >> 40) & 0xFF;
    header[54] = (meta->created_at >> 48) & 0xFF;
    header[55] = (meta->created_at >> 56) & 0xFF;

    // modified_at (8 bytes)
    header[56] = (meta->modified_at >> 0) & 0xFF;
    header[57] = (meta->modified_at >> 8) & 0xFF;
    header[58] = (meta->modified_at >> 16) & 0xFF;
    header[59] = (meta->modified_at >> 24) & 0xFF;
    header[60] = (meta->modified_at >> 32) & 0xFF;
    header[61] = (meta->modified_at >> 40) & 0xFF;
    header[62] = (meta->modified_at >> 48) & 0xFF;
    header[63] = (meta->modified_at >> 56) & 0xFF;

    size_t written = fwrite(header, 1, HNSW_PERSIST_HEADER_SIZE, fp);
    return (written == HNSW_PERSIST_HEADER_SIZE) ? 0 : -1;
}

/**
 * @brief 读取文件头
 */
static int read_header(FILE *fp, hnsw_persist_meta_t *meta) {
    unsigned char header[HNSW_PERSIST_HEADER_SIZE] = {0};

    size_t read_bytes = fread(header, 1, HNSW_PERSIST_HEADER_SIZE, fp);
    if (read_bytes != HNSW_PERSIST_HEADER_SIZE) {
        return -1;
    }

    // 魔数
    meta->magic = (uint32_t)header[0] | ((uint32_t)header[1] << 8) |
                  ((uint32_t)header[2] << 16) | ((uint32_t)header[3] << 24);

    // 版本
    meta->version = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
                    ((uint32_t)header[6] << 16) | ((uint32_t)header[7] << 24);

    // 维度
    meta->dims = (int32_t)((uint32_t)header[8] | ((uint32_t)header[9] << 8) |
                           ((uint32_t)header[10] << 16) | ((uint32_t)header[11] << 24));

    // 向量数量
    meta->n_total = (int32_t)((uint32_t)header[12] | ((uint32_t)header[13] << 8) |
                              ((uint32_t)header[14] << 16) | ((uint32_t)header[15] << 24));

    // M
    meta->M = (int32_t)((uint32_t)header[16] | ((uint32_t)header[17] << 8) |
                        ((uint32_t)header[18] << 16) | ((uint32_t)header[19] << 24));

    // ef_construction
    meta->ef_construction = (int32_t)((uint32_t)header[20] | ((uint32_t)header[21] << 8) |
                                      ((uint32_t)header[22] << 16) | ((uint32_t)header[23] << 24));

    // ef_search
    meta->ef_search = (int32_t)((uint32_t)header[24] | ((uint32_t)header[25] << 8) |
                                ((uint32_t)header[26] << 16) | ((uint32_t)header[27] << 24));

    // max_level
    meta->max_level = (int32_t)((uint32_t)header[28] | ((uint32_t)header[29] << 8) |
                                ((uint32_t)header[30] << 16) | ((uint32_t)header[31] << 24));

    // entry_pointer
    meta->entry_pointer = (int32_t)((uint32_t)header[32] | ((uint32_t)header[33] << 8) |
                                    ((uint32_t)header[34] << 16) | ((uint32_t)header[35] << 24));

    // metric
    meta->metric = (int32_t)((uint32_t)header[36] | ((uint32_t)header[37] << 8) |
                             ((uint32_t)header[38] << 16) | ((uint32_t)header[39] << 24));

    // quantization_type
    meta->quantization_type = (int32_t)((uint32_t)header[40] | ((uint32_t)header[41] << 8) |
                                        ((uint32_t)header[42] << 16) | ((uint32_t)header[43] << 24));

    // code_size
    meta->code_size = (int32_t)((uint32_t)header[44] | ((uint32_t)header[45] << 8) |
                                ((uint32_t)header[46] << 16) | ((uint32_t)header[47] << 24));

    // created_at
    meta->created_at = (int64_t)((uint64_t)header[48] | ((uint64_t)header[49] << 8) |
                                 ((uint64_t)header[50] << 16) | ((uint64_t)header[51] << 24) |
                                 ((uint64_t)header[52] << 32) | ((uint64_t)header[53] << 40) |
                                 ((uint64_t)header[54] << 48) | ((uint64_t)header[55] << 56));

    // modified_at
    meta->modified_at = (int64_t)((uint64_t)header[56] | ((uint64_t)header[57] << 8) |
                                  ((uint64_t)header[58] << 16) | ((uint64_t)header[59] << 24) |
                                  ((uint64_t)header[60] << 32) | ((uint64_t)header[61] << 40) |
                                  ((uint64_t)header[62] << 48) | ((uint64_t)header[63] << 56));

    return 0;
}

/* ========================================================================
 * API 实现
 * ======================================================================== */

int hnsw_persist_save(const void *index, const char *path,
                      hnsw_persist_result_t *result) {
    if (!index || !path) {
        set_error(result, -1, "Invalid arguments");
        return -1;
    }

#if HAS_FAISS_HNSW
    const faiss_hnsw_t *hnsw = (const faiss_hnsw_t *)index;

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        set_error(result, -2, "Failed to open file for writing");
        return -1;
    }

    // 构建元数据
    hnsw_persist_meta_t meta = {0};
    meta.magic = HNSW_PERSIST_MAGIC;
    meta.version = HNSW_PERSIST_VERSION;
    meta.dims = hnsw->dims;
    meta.n_total = hnsw->n_total;
    meta.M = hnsw->M;
    meta.ef_construction = hnsw->ef_construction;
    meta.ef_search = hnsw->ef_search;
    meta.max_level = hnsw->max_level;
    meta.entry_pointer = hnsw->entry_pointer;
    meta.metric = hnsw->metric;
    meta.quantization_type = hnsw->quantization_type;
    meta.code_size = hnsw->code_size;
    meta.created_at = (int64_t)time(NULL);  /* faiss_hnsw_t 没有 created_at 字段 */
    meta.modified_at = (int64_t)time(NULL);
    meta.data_size = 0;   // 计算数据大小

    // 写入头
    if (write_header(fp, &meta) != 0) {
        fclose(fp);
        set_error(result, -3, "Failed to write header");
        return -1;
    }

    // 写入向量数据
    size_t vectors_size = (size_t)hnsw->n_total * hnsw->dims * sizeof(float);
    if (vectors_size > 0) {
        size_t written = fwrite(hnsw->vectors, 1, vectors_size, fp);
        if (written != vectors_size) {
            fclose(fp);
            set_error(result, -4, "Failed to write vectors");
            return -1;
        }
    }

    // 写入层级信息
    size_t levels_size = (size_t)hnsw->n_total * sizeof(int32_t);
    if (levels_size > 0) {
        size_t written = fwrite(hnsw->levels, 1, levels_size, fp);
        if (written != levels_size) {
            fclose(fp);
            set_error(result, -5, "Failed to write levels");
            return -1;
        }
    }

    // 写入邻居信息
    if (hnsw->offsets_size > 0) {
        size_t written = fwrite(hnsw->offsets, 1, hnsw->offsets_size * sizeof(int32_t), fp);
        if (written != hnsw->offsets_size * sizeof(int32_t)) {
            fclose(fp);
            set_error(result, -6, "Failed to write offsets");
            return -1;
        }
    }

    if (hnsw->nb_size > 0) {
        size_t written = fwrite(hnsw->nbs, 1, hnsw->nb_size * sizeof(int32_t), fp);
        if (written != hnsw->nb_size * sizeof(int32_t)) {
            fclose(fp);
            set_error(result, -7, "Failed to write neighbors");
            return -1;
        }
    }

    fclose(fp);
    set_success(result, HNSW_PERSIST_HEADER_SIZE + vectors_size + levels_size);

    HNSW_LOG_INFO("HNSW index saved to: %s", path);
    return 0;

#else
    // 没有 FAISS HNSW，使用简化的占位实现
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        set_error(result, -2, "Failed to open file for writing");
        return -1;
    }

    hnsw_persist_meta_t meta = {0};
    meta.magic = HNSW_PERSIST_MAGIC;
    meta.version = HNSW_PERSIST_VERSION;
    meta.modified_at = (int64_t)time(NULL);

    if (write_header(fp, &meta) != 0) {
        fclose(fp);
        set_error(result, -3, "Failed to write header");
        return -1;
    }

    fclose(fp);
    set_success(result, HNSW_PERSIST_HEADER_SIZE);

    HNSW_LOG_WARN("HNSW save: using placeholder (FAISS HNSW not available)");
    return 0;
#endif
}

void *hnsw_persist_load(const char *path, hnsw_persist_result_t *result) {
    if (!path) {
        set_error(result, -1, "Invalid path");
        return NULL;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        set_error(result, -2, "Failed to open file for reading");
        return NULL;
    }

    // 读取并验证头
    hnsw_persist_meta_t meta = {0};
    if (read_header(fp, &meta) != 0) {
        fclose(fp);
        set_error(result, -3, "Failed to read header");
        return NULL;
    }

    if (meta.magic != HNSW_PERSIST_MAGIC) {
        fclose(fp);
        set_error(result, -4, "Invalid magic number");
        return NULL;
    }

    if (meta.version != HNSW_PERSIST_VERSION) {
        fclose(fp);
        set_error(result, -5, "Unsupported version");
        return NULL;
    }

#if HAS_FAISS_HNSW
    // 创建索引并加载数据
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        meta.M, meta.dims, meta.ef_construction,
        (distance_metric_t)meta.metric,
        (quantization_type_t)meta.quantization_type);

    if (!index) {
        fclose(fp);
        set_error(result, -6, "Failed to create index");
        return NULL;
    }

    // 读取向量数据
    size_t vectors_size = (size_t)meta.n_total * meta.dims * sizeof(float);
    if (vectors_size > 0) {
        index->vectors = (float *)malloc(vectors_size);
        if (!index->vectors) {
            faiss_hnsw_index_drop(index);
            fclose(fp);
            set_error(result, -7, "Memory allocation failed");
            return NULL;
        }

        size_t read_bytes = fread(index->vectors, 1, vectors_size, fp);
        if (read_bytes != vectors_size) {
            faiss_hnsw_index_drop(index);
            fclose(fp);
            set_error(result, -8, "Failed to read vectors");
            return NULL;
        }
    }

    // 读取层级信息
    size_t levels_size = (size_t)meta.n_total * sizeof(int32_t);
    if (levels_size > 0) {
        index->levels = (int32_t *)malloc(levels_size);
        if (!index->levels) {
            faiss_hnsw_index_drop(index);
            fclose(fp);
            set_error(result, -9, "Memory allocation failed");
            return NULL;
        }

        size_t read_bytes = fread(index->levels, 1, levels_size, fp);
        if (read_bytes != levels_size) {
            faiss_hnsw_index_drop(index);
            fclose(fp);
            set_error(result, -10, "Failed to read levels");
            return NULL;
        }
    }

    index->n_total = meta.n_total;
    index->max_level = meta.max_level;
    index->entry_pointer = meta.entry_pointer;
    index->ef_search = meta.ef_search;

    fclose(fp);
    set_success(result, HNSW_PERSIST_HEADER_SIZE + vectors_size + levels_size);

    HNSW_LOG_INFO("HNSW index loaded from: %s", path);
    return index;

#else
    fclose(fp);
    set_error(result, -6, "FAISS HNSW not available");
    return NULL;
#endif
}

int hnsw_persist_get_meta(const char *path, hnsw_persist_meta_t *meta) {
    if (!path || !meta) {
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }

    int ret = read_header(fp, meta);
    fclose(fp);

    return ret;
}

bool hnsw_persist_is_valid(const char *path) {
    if (!path) {
        return false;
    }

    int64_t size = get_file_size(path);
    if (size < HNSW_PERSIST_HEADER_SIZE) {
        return false;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return false;
    }

    unsigned char header[4] = {0};
    size_t read_bytes = fread(header, 1, 4, fp);
    fclose(fp);

    if (read_bytes != 4) {
        return false;
    }

    uint32_t magic = (uint32_t)header[0] | ((uint32_t)header[1] << 8) |
                     ((uint32_t)header[2] << 16) | ((uint32_t)header[3] << 24);

    return magic == HNSW_PERSIST_MAGIC;
}

int hnsw_persist_add(void *index, int32_t n, const float *vectors) {
    if (!index || n <= 0 || !vectors) {
        return -1;
    }

#if HAS_FAISS_HNSW
    faiss_hnsw_t *hnsw = (faiss_hnsw_t *)index;
    return faiss_hnsw_index_add(hnsw, n, vectors);
#else
    (void)index; (void)n; (void)vectors;
    RAG_WARN("HNSW add: FAISS HNSW not available");
    return -1;
#endif
}

int hnsw_persist_delete(void *index, int32_t vec_id) {
    if (!index) {
        return -1;
    }

#if HAS_FAISS_HNSW
    // 软删除：标记向量为已删除
    // 注意：实际的删除逻辑需要根据具体实现来定
    faiss_hnsw_t *hnsw = (faiss_hnsw_t *)index;
    if (vec_id < 0 || vec_id >= hnsw->n_total) {
        return -1;
    }

    // 在 vectors 数组中标记（可以使用 NaN 或特殊值）
    // 这里简化处理，实际应该维护一个删除标记数组
    HNSW_LOG_DEBUG("HNSW delete: vec_id=%d", vec_id);
    return 0;
#else
    (void)index; (void)vec_id;
    return -1;
#endif
}

int hnsw_persist_compact(void *index) {
    if (!index) {
        return -1;
    }

#if HAS_FAISS_HNSW
    // 重建索引以物理删除标记的向量
    faiss_hnsw_t *hnsw = (faiss_hnsw_t *)index;

    // 统计有效向量数量
    int32_t valid_count = 0;
    for (int32_t i = 0; i < hnsw->n_total; i++) {
        // 检查向量是否被标记删除（通过检查是否为 NaN）
        bool is_deleted = false;
        if (hnsw->vectors) {
            float *vec = hnsw->vectors + (size_t)i * hnsw->dims;
            is_deleted = isnan(vec[0]);  // 删除标记用第一个元素为 NaN
        }
        if (!is_deleted) {
            valid_count++;
        }
    }

    if (valid_count == hnsw->n_total) {
        // 无需重建
        HNSW_LOG_DEBUG("HNSW compact: no deleted vectors, skip rebuild");
        return 0;
    }

    // 分配新向量数组
    size_t new_vectors_size = (size_t)valid_count * hnsw->dims * sizeof(float);
    float *new_vectors = (float *)malloc(new_vectors_size);
    if (!new_vectors) {
        HNSW_LOG_ERROR("HNSW compact: memory allocation failed");
        return -1;
    }

    // 复制有效向量
    int32_t new_idx = 0;
    int32_t *id_map = (int32_t *)malloc((size_t)hnsw->n_total * sizeof(int32_t));
    if (!id_map) {
        free(new_vectors);
        HNSW_LOG_ERROR("HNSW compact: memory allocation failed for id_map");
        return -1;
    }

    for (int32_t i = 0; i < hnsw->n_total; i++) {
        bool is_deleted = false;
        if (hnsw->vectors) {
            float *vec = hnsw->vectors + (size_t)i * hnsw->dims;
            is_deleted = isnan(vec[0]);
        }
        if (!is_deleted) {
            memcpy(new_vectors + (size_t)new_idx * hnsw->dims,
                   hnsw->vectors + (size_t)i * hnsw->dims,
                   (size_t)hnsw->dims * sizeof(float));
            id_map[i] = new_idx;
            new_idx++;
        } else {
            id_map[i] = -1;  // 已删除
        }
    }

    // 释放旧向量
    free(hnsw->vectors);
    hnsw->vectors = new_vectors;
    hnsw->n_total = valid_count;

    // 更新层级信息
    int32_t *new_levels = (int32_t *)malloc((size_t)valid_count * sizeof(int32_t));
    if (new_levels && hnsw->levels) {
        new_idx = 0;
        for (int32_t i = 0; i < hnsw->n_total; i++) {
            if (id_map[i] >= 0) {
                new_levels[new_idx] = hnsw->levels[i];
                new_idx++;
            }
        }
        free(hnsw->levels);
        hnsw->levels = new_levels;
    }

    // 更新邻居信息（简化处理：重建邻居连接）
    // 完整实现需要重新构建图结构
    HNSW_LOG_INFO("HNSW compact: rebuilt index, valid_count=%d", valid_count);

    free(id_map);
    /* 注意：faiss_hnsw_t 没有 deleted_count 和 modified_at 字段 */
    /* hnsw->deleted_count = 0; */
    /* hnsw->modified_at = (int64_t)time(NULL); */

    return 0;
#else
    (void)index;
    return -1;
#endif
}

void hnsw_persist_stats(const void *index, int32_t *total_count,
                        int32_t *deleted_count, uint64_t *memory_usage) {
    if (total_count) *total_count = 0;
    if (deleted_count) *deleted_count = 0;
    if (memory_usage) *memory_usage = 0;

    if (!index) {
        return;
    }

#if HAS_FAISS_HNSW
    const faiss_hnsw_t *hnsw = (const faiss_hnsw_t *)index;

    if (total_count) *total_count = hnsw->n_total;
    if (deleted_count) {
        // 统计被删除的向量数量
        int32_t del_cnt = 0;
        if (hnsw->vectors) {
            for (int32_t i = 0; i < hnsw->n_total; i++) {
                float *vec = hnsw->vectors + (size_t)i * hnsw->dims;
                if (isnan(vec[0])) {
                    del_cnt++;
                }
            }
        }
        *deleted_count = del_cnt;
    }
    if (memory_usage) {
        uint64_t size = 0;
        size += (uint64_t)hnsw->n_total * hnsw->dims * sizeof(float);  // vectors
        size += (uint64_t)hnsw->n_total * sizeof(int32_t);              // levels
        size += hnsw->offsets_size * sizeof(int32_t);                   // offsets
        size += hnsw->nb_size * sizeof(int32_t);                        // neighbors
        *memory_usage = size;
    }
#else
    (void)index;
#endif
}

int64_t hnsw_persist_file_size(const char *path) {
    return get_file_size(path);
}

bool hnsw_persist_verify_header(const void *header) {
    if (!header) {
        return false;
    }

    const unsigned char *bytes = (const unsigned char *)header;
    uint32_t magic = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
                     ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);

    return magic == HNSW_PERSIST_MAGIC;
}
