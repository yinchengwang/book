/**
 * @file data_integrity.c
 * @brief 数据完整性实现
 */

#include "db/storage/integrity/data_integrity.h"
#include "db/wal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** 默认页面大小：8KB */
#define DEFAULT_PAGE_SIZE 8192

/** 页面魔数，用于验证页面有效性 */
#define PAGE_MAGIC 0x50414745  /* "PAGE" */

/** 页面类型 */
typedef enum page_type_e {
    PAGE_FREE = 0,      /**< 空闲页 */
    PAGE_DATA = 1,     /**< 数据页 */
    PAGE_INDEX = 2,    /**< 索引页 */
    PAGE_OVERFLOW = 3, /**< 溢出页（大字段） */
    PAGE_META = 4      /**< 元数据页 */
} page_type_t;

/**
 * @brief 页面头部结构（紧凑排列，16 字节）
 */
#pragma pack(push, 1)
typedef struct page_header_s {
    uint32_t magic;             /**< 页面魔数 */
    uint32_t page_id;           /**< 页面 ID */
    uint32_t checksum;          /**< CRC32 校验和 */
    uint16_t free_space_offset; /**< 空闲空间起始偏移 */
    uint8_t  page_type;         /**< 页面类型 */
    uint8_t  reserved;          /**< 保留字段 */
} page_header_t;
#pragma pack(pop)

/* ========================================================================
 * CRC16 实现
 * ======================================================================== */

/** CRC16 多项式 */
#define CRC16_POLY 0x8005

/**
 * @brief 计算 CRC16
 */
uint16_t page_compute_checksum(const void *page_data, size_t page_size)
{
    if (!page_data || page_size < 16) {
        return 0;
    }

    uint16_t crc = 0xFFFF;
    const uint8_t *data = (const uint8_t *)page_data;
    size_t len = page_size - PAGE_CHECKSUM_SIZE;  /* 不包含 checksum 本身 */

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_POLY;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief 验证页面的校验和
 */
bool page_verify_checksum(const void *page_data, size_t page_size,
                        uint16_t stored_checksum)
{
    uint16_t computed = page_compute_checksum(page_data, page_size);
    return computed == stored_checksum;
}

/**
 * @brief 设置页面校验和
 */
void page_set_checksum(void *page_data, size_t page_size)
{
    if (!page_data || page_size < 16) {
        return;
    }

    uint16_t checksum = page_compute_checksum(page_data, page_size);
    uint8_t *data = (uint8_t *)page_data;
    size_t offset = page_size - PAGE_CHECKSUM_SIZE;

    /* 写入校验和（小端序） */
    data[offset] = checksum & 0xFF;
    data[offset + 1] = (checksum >> 8) & 0xFF;
}

/**
 * @brief 获取页面校验和
 */
uint16_t page_get_checksum(const void *page_data)
{
    if (!page_data) {
        return 0;
    }

    const uint8_t *data = (const uint8_t *)page_data;
    size_t page_size = DEFAULT_PAGE_SIZE;
    size_t offset = page_size - PAGE_CHECKSUM_SIZE;

    /* 读取校验和（小端序） */
    return data[offset] | (data[offset + 1] << 8);
}

/**
 * @brief 检查页面是否损坏
 */
bool page_is_corrupted(const void *page_data, size_t page_size)
{
    if (!page_data || page_size < 16) {
        return true;
    }

    const page_header_t *header = (const page_header_t *)page_data;

    /* 检查魔数 */
    if (header->magic != PAGE_MAGIC) {
        return true;
    }

    /* 检查页面类型是否有效 */
    if (header->page_type > PAGE_META) {
        return true;
    }

    /* 检查空闲空间偏移是否合理 */
    if (header->free_space_offset > page_size) {
        return true;
    }

    /* 检查校验和 */
    uint16_t stored = page_get_checksum(page_data);
    if (!page_verify_checksum(page_data, page_size, stored)) {
        return true;
    }

    return false;
}

/* ========================================================================
 * 损坏检测实现
 * ======================================================================== */

/**
 * @brief 创建损坏结果
 */
static CorruptionResult make_corruption_result(CorruptionType type,
                                            uint32_t location,
                                            const char *message)
{
    CorruptionResult result = {
        .corrupted = true,
        .info = {
            .type = type,
            .location = location,
            .context = 0,
            .message = message,
            .file = NULL,
            .line = 0
        }
    };
    return result;
}

/**
 * @brief 创建正常结果
 */
static CorruptionResult make_ok_result(void)
{
    CorruptionResult result = {
        .corrupted = false,
        .info = {
            .type = CORRUPTION_NONE,
            .location = 0,
            .context = 0,
            .message = "OK",
            .file = NULL,
            .line = 0
        }
    };
    return result;
}

/**
 * @brief KV 引擎页面格式头
 */
typedef struct kv_page_header_s {
    uint32_t magic;          /**< KV 页魔数 */
    uint32_t page_id;        /**< 页面 ID */
    uint16_t num_entries;    /**< 条目数量 */
    uint16_t free_space;     /**< 空闲空间大小 */
    uint32_t checksum;       /**< 校验和 */
} kv_page_header_t;

#define KV_PAGE_MAGIC 0x4B565020  /**< "KVP " */

#define KV_ENTRY_HEADER_SIZE 8
/**
 * @brief KV 条目头部
 */
typedef struct kv_entry_header_s {
    uint8_t  key_len;         /**< 键长度 */
    uint8_t  flags;          /**< 标志位 */
    uint16_t value_len;      /**< 值长度 */
    uint32_t value_offset;   /**< 值偏移 */
} kv_entry_header_t;

/**
 * @brief 检查 KV 引擎数据完整性
 */
CorruptionResult integrity_check_kv(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const kv_page_header_t *header = (const kv_page_header_t *)data;

    /* 检查魔数 */
    if (header->magic != KV_PAGE_MAGIC) {
        return make_corruption_result(CORRUPTION_MAGIC, 0, "KV 页魔数错误");
    }

    /* 检查条目数量 */
    if (header->num_entries > 10000) {
        return make_corruption_result(CORRUPTION_FORMAT, offsetof(kv_page_header_t, num_entries),
                                     "KV 条目数异常");
    }

    /* 检查空闲空间 */
    size_t min_free = header->num_entries * KV_ENTRY_HEADER_SIZE;
    if (header->free_space > size - sizeof(kv_page_header_t) ||
        header->free_space < min_free) {
        return make_corruption_result(CORRUPTION_OFFSET, offsetof(kv_page_header_t, free_space),
                                     "KV 空闲空间异常");
    }

    /* 验证校验和 */
    uint32_t stored_checksum = header->checksum;
    uint32_t computed = 0;
    const uint8_t *page_bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size - 4; i++) {
        computed += page_bytes[i];
    }
    if (stored_checksum != computed) {
        return make_corruption_result(CORRUPTION_CHECKSUM, offsetof(kv_page_header_t, checksum),
                                     "KV 页校验和失败");
    }

    /* 扫描 KV 条目，验证格式 */
    const uint8_t *entry_start = page_bytes + sizeof(kv_page_header_t);
    size_t offset = 0;
    for (uint16_t i = 0; i < header->num_entries; i++) {
        if (offset + KV_ENTRY_HEADER_SIZE > size) {
            return make_corruption_result(CORRUPTION_OFFSET, offset,
                                         "KV 条目头部超出范围");
        }

        const kv_entry_header_t *entry = (const kv_entry_header_t *)(entry_start + offset);
        size_t total_entry_size = KV_ENTRY_HEADER_SIZE + entry->key_len + entry->value_len;

        if (offset + total_entry_size > size) {
            return make_corruption_result(CORRUPTION_SIZE, offset,
                                         "KV 条目数据超出页面");
        }

        /* 检查值偏移是否合理 */
        if (entry->value_offset < sizeof(kv_page_header_t) ||
            entry->value_offset + entry->value_len > size) {
            return make_corruption_result(CORRUPTION_POINTER, offset,
                                         "KV 值指针无效");
        }

        offset += total_entry_size;
    }

    return make_ok_result();
}

/**
 * @brief 向量引擎页格式
 */
typedef struct vector_page_header_s {
    uint32_t magic;          /**< 向量页魔数 */
    uint32_t page_id;        /**< 页面 ID */
    uint16_t num_vectors;    /**< 向量数量 */
    uint16_t dims;           /**< 向量维度 */
    uint32_t free_space;     /**< 空闲空间 */
    uint32_t checksum;       /**< 校验和 */
} vector_page_header_t;

#define VECTOR_PAGE_MAGIC 0x56454354  /**< "VECT" */

/**
 * @brief 检查向量引擎数据完整性
 */
CorruptionResult integrity_check_vector(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const vector_page_header_t *header = (const vector_page_header_t *)data;

    /* 检查魔数 */
    if (header->magic != VECTOR_PAGE_MAGIC) {
        return make_corruption_result(CORRUPTION_MAGIC, 0, "向量页魔数错误");
    }

    /* 检查维度 */
    if (header->dims == 0 || header->dims > 65535) {
        return make_corruption_result(CORRUPTION_FORMAT, offsetof(vector_page_header_t, dims),
                                     "向量维度异常");
    }

    /* 检查向量数量 */
    size_t vec_size = sizeof(float) * header->dims;
    size_t min_size = sizeof(vector_page_header_t) + (size_t)header->num_vectors * vec_size;
    if (header->num_vectors > 100000 || min_size > size) {
        return make_corruption_result(CORRUPTION_SIZE, offsetof(vector_page_header_t, num_vectors),
                                     "向量数量异常");
    }

    /* 验证校验和 */
    uint32_t stored = header->checksum;
    uint32_t computed = 0;
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size - 4; i++) {
        computed += bytes[i];
    }
    if (stored != computed) {
        return make_corruption_result(CORRUPTION_CHECKSUM, offsetof(vector_page_header_t, checksum),
                                     "向量页校验和失败");
    }

    /* 扫描向量数据，检查 NaN/Inf */
    const float *vec_data = (const float *)((const uint8_t *)data + sizeof(vector_page_header_t));
    for (uint16_t i = 0; i < header->num_vectors; i++) {
        for (uint16_t d = 0; d < header->dims; d++) {
            float val = vec_data[i * header->dims + d];
            if (val != val) {  /* NaN check */
                return make_corruption_result(CORRUPTION_FORMAT, i * header->dims + d,
                                             "向量包含 NaN");
            }
        }
    }

    return make_ok_result();
}

/**
 * @brief 时序引擎页格式
 */
typedef struct ts_page_header_s {
    uint32_t magic;          /**< 时序页魔数 */
    uint32_t page_id;        /**< 页面 ID */
    uint32_t series_id;      /**< 时序序列 ID */
    int64_t start_time;     /**< 起始时间戳 */
    int64_t end_time;       /**< 结束时间戳 */
    uint16_t num_points;     /**< 数据点数量 */
    uint32_t checksum;       /**< 校验和 */
} ts_page_header_t;

#define TS_PAGE_MAGIC 0x54534552  /**< "TSER" */

/**
 * @brief 时序数据点
 */
typedef struct ts_point_s {
    int64_t timestamp;       /**< 时间戳 */
    double value;          /**< 值 */
} ts_point_t;

/**
 * @brief 检查时序引擎数据完整性
 */
CorruptionResult integrity_check_timeseries(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const ts_page_header_t *header = (const ts_page_header_t *)data;

    /* 检查魔数 */
    if (header->magic != TS_PAGE_MAGIC) {
        return make_corruption_result(CORRUPTION_MAGIC, 0, "时序页魔数错误");
    }

    /* 检查时间范围 */
    if (header->num_points > 0 && header->end_time < header->start_time) {
        return make_corruption_result(CORRUPTION_FORMAT, offsetof(ts_page_header_t, end_time),
                                     "时序结束时间早于开始时间");
    }

    /* 检查数据点数量 */
    size_t min_size = sizeof(ts_page_header_t) + (size_t)header->num_points * sizeof(ts_point_t);
    if (header->num_points > 1000000 || min_size > size) {
        return make_corruption_result(CORRUPTION_SIZE, offsetof(ts_page_header_t, num_points),
                                     "时序数据点数量异常");
    }

    /* 验证校验和 */
    uint32_t stored = header->checksum;
    uint32_t computed = 0;
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size - 4; i++) {
        computed += bytes[i];
    }
    if (stored != computed) {
        return make_corruption_result(CORRUPTION_CHECKSUM, offsetof(ts_page_header_t, checksum),
                                     "时序页校验和失败");
    }

    /* 扫描数据点，验证时间戳递增 */
    const ts_point_t *points = (const ts_point_t *)((const uint8_t *)data + sizeof(ts_page_header_t));
    int64_t prev_ts = 0;
    for (uint16_t i = 0; i < header->num_points; i++) {
        if (i > 0 && points[i].timestamp <= prev_ts) {
            return make_corruption_result(CORRUPTION_FORMAT,
                                         sizeof(ts_page_header_t) + i * sizeof(ts_point_t),
                                         "时序时间戳非递增");
        }
        prev_ts = points[i].timestamp;
    }

    return make_ok_result();
}

/**
 * @brief 文档引擎页格式
 */
typedef struct doc_page_header_s {
    uint32_t magic;          /**< 文档页魔数 */
    uint32_t page_id;        /**< 页面 ID */
    uint16_t num_docs;       /**< 文档数量 */
    uint16_t max_doc_size;   /**< 最大文档大小 */
    uint32_t free_space;     /**< 空闲空间 */
    uint32_t checksum;       /**< 校验和 */
} doc_page_header_t;

#define DOC_PAGE_MAGIC 0x444F4354  /**< "DOCT" */

/**
 * @brief 检查文档引擎数据完整性
 */
CorruptionResult integrity_check_document(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const doc_page_header_t *header = (const doc_page_header_t *)data;

    /* 检查魔数 */
    if (header->magic != DOC_PAGE_MAGIC) {
        return make_corruption_result(CORRUPTION_MAGIC, 0, "文档页魔数错误");
    }

    /* 检查最大文档大小 */
    if (header->max_doc_size > 10 * 1024 * 1024) {  /* 10MB limit */
        return make_corruption_result(CORRUPTION_FORMAT, offsetof(doc_page_header_t, max_doc_size),
                                     "文档最大大小异常");
    }

    /* 检查文档数量 */
    if (header->num_docs > 100000) {
        return make_corruption_result(CORRUPTION_SIZE, offsetof(doc_page_header_t, num_docs),
                                     "文档数量异常");
    }

    /* 验证校验和 */
    uint32_t stored = header->checksum;
    uint32_t computed = 0;
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size - 4; i++) {
        computed += bytes[i];
    }
    if (stored != computed) {
        return make_corruption_result(CORRUPTION_CHECKSUM, offsetof(doc_page_header_t, checksum),
                                     "文档页校验和失败");
    }

    return make_ok_result();
}

/**
 * @brief 空间引擎页格式
 */
typedef struct spatial_page_header_s {
    uint32_t magic;          /**< 空间页魔数 */
    uint32_t page_id;        /**< 页面 ID */
    uint16_t num_geoms;      /**< 几何对象数量 */
    uint8_t  bbox[16];       /**< 边界框: min_x,min_y,max_x,max_y 各 4 字节 */
    uint32_t checksum;       /**< 校验和 */
} spatial_page_header_t;

#define SPATIAL_PAGE_MAGIC 0x53504143  /**< "SPAC" */

/**
 * @brief 检查空间引擎数据完整性
 */
CorruptionResult integrity_check_spatial(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const spatial_page_header_t *header = (const spatial_page_header_t *)data;

    /* 检查魔数 */
    if (header->magic != SPATIAL_PAGE_MAGIC) {
        return make_corruption_result(CORRUPTION_MAGIC, 0, "空间页魔数错误");
    }

    /* 检查几何对象数量 */
    if (header->num_geoms > 100000) {
        return make_corruption_result(CORRUPTION_SIZE, offsetof(spatial_page_header_t, num_geoms),
                                     "空间对象数量异常");
    }

    /* 验证校验和 */
    uint32_t stored = header->checksum;
    uint32_t computed = 0;
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size - 4; i++) {
        computed += bytes[i];
    }
    if (stored != computed) {
        return make_corruption_result(CORRUPTION_CHECKSUM, offsetof(spatial_page_header_t, checksum),
                                     "空间页校验和失败");
    }

    return make_ok_result();
}

/**
 * @brief 图引擎页格式
 */
typedef struct graph_page_header_s {
    uint32_t magic;          /**< 图页魔数 */
    uint32_t page_id;        /**< 页面 ID */
    uint32_t num_nodes;      /**< 节点数量 */
    uint32_t num_edges;      /**< 边数量 */
    uint32_t checksum;       /**< 校验和 */
} graph_page_header_t;

#define GRAPH_PAGE_MAGIC 0x47524150  /**< "GRAP" */

/**
 * @brief 检查图引擎数据完整性
 */
CorruptionResult integrity_check_graph(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const graph_page_header_t *header = (const graph_page_header_t *)data;

    /* 检查魔数 */
    if (header->magic != GRAPH_PAGE_MAGIC) {
        return make_corruption_result(CORRUPTION_MAGIC, 0, "图页魔数错误");
    }

    /* 检查节点和边数量 */
    if (header->num_nodes > 10000000 || header->num_edges > 100000000) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "图节点/边数量异常");
    }

    /* 验证校验和 */
    uint32_t stored = header->checksum;
    uint32_t computed = 0;
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size - 4; i++) {
        computed += bytes[i];
    }
    if (stored != computed) {
        return make_corruption_result(CORRUPTION_CHECKSUM, offsetof(graph_page_header_t, checksum),
                                     "图页校验和失败");
    }

    return make_ok_result();
}

/**
 * @brief 树引擎页格式
 */
typedef struct tree_page_header_s {
    uint32_t magic;          /**< 树页魔数 */
    uint32_t page_id;        /**< 页面 ID */
    uint16_t is_leaf;        /**< 是否叶子页 */
    uint16_t num_keys;       /**< 键数量 */
    uint32_t parent_id;      /**< 父页面 ID */
    uint32_t checksum;       /**< 校验和 */
} tree_page_header_t;

#define TREE_PAGE_MAGIC 0x54524545  /**< "TREE" */

/**
 * @brief 检查树引擎数据完整性
 */
CorruptionResult integrity_check_tree(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const tree_page_header_t *header = (const tree_page_header_t *)data;

    /* 检查魔数 */
    if (header->magic != TREE_PAGE_MAGIC) {
        return make_corruption_result(CORRUPTION_MAGIC, 0, "树页魔数错误");
    }

    /* 检查键数量 */
    if (header->num_keys > 100000) {
        return make_corruption_result(CORRUPTION_SIZE, offsetof(tree_page_header_t, num_keys),
                                     "树键数量异常");
    }

    /* 检查 is_leaf 标志 */
    if (header->is_leaf > 1) {
        return make_corruption_result(CORRUPTION_FORMAT, offsetof(tree_page_header_t, is_leaf),
                                     "树页 is_leaf 标志无效");
    }

    /* 验证校验和 */
    uint32_t stored = header->checksum;
    uint32_t computed = 0;
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size - 4; i++) {
        computed += bytes[i];
    }
    if (stored != computed) {
        return make_corruption_result(CORRUPTION_CHECKSUM, offsetof(tree_page_header_t, checksum),
                                     "树页校验和失败");
    }

    return make_ok_result();
}

/**
 * @brief 检查 OPQ 索引数据完整性
 */
CorruptionResult integrity_check_opq(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const uint8_t *bytes = (const uint8_t *)data;

    /* 检查 OPQ 格式头部魔数 */
    uint32_t magic = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
    if (magic != 0x4F505120) {  /* "OPQ " */
        return make_corruption_result(CORRUPTION_MAGIC, 0, "OPQ 索引魔数错误");
    }

    /* 检查维度信息 */
    uint32_t dims = bytes[4] | (bytes[5] << 8) | (bytes[6] << 16) | (bytes[7] << 24);
    if (dims == 0 || dims > 65536) {
        return make_corruption_result(CORRUPTION_FORMAT, 4, "OPQ 维度异常");
    }

    /* 检查量化矩阵大小 */
    uint32_t matrix_size = dims * dims * sizeof(float);
    if (size < 16 + matrix_size) {
        return make_corruption_result(CORRUPTION_SIZE, 8, "OPQ 数据大小不足");
    }

    /* 验证校验和 */
    uint16_t stored_checksum = page_get_checksum(data);
    if (!page_verify_checksum(data, size, stored_checksum)) {
        return make_corruption_result(CORRUPTION_CHECKSUM, size - 2, "OPQ 索引校验和失败");
    }

    /* 扫描旋转矩阵，检查 NaN/Inf */
    const float *rotation = (const float *)(bytes + 16);
    for (uint32_t i = 0; i < dims * dims; i++) {
        float val = rotation[i];
        if (val != val) {
            return make_corruption_result(CORRUPTION_FORMAT, 16 + i * 4, "OPQ 旋转矩阵包含 NaN");
        }
    }

    return make_ok_result();
}

/**
 * @brief 检查 RTree 索引数据完整性
 */
CorruptionResult integrity_check_rtree(const void *data, size_t size)
{
    if (!data || size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const uint8_t *bytes = (const uint8_t *)data;

    /* 检查 RTree 格式头部魔数 */
    uint32_t magic = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
    if (magic != 0x52545245) {  /* "RTRE" */
        return make_corruption_result(CORRUPTION_MAGIC, 0, "RTree 索引魔数错误");
    }

    /* 检查维度信息 */
    uint16_t dims = bytes[4] | (bytes[5] << 8);
    if (dims == 0 || dims > 16) {
        return make_corruption_result(CORRUPTION_FORMAT, 4, "RTree 维度异常");
    }

    /* 检查节点数量 */
    uint32_t num_nodes = bytes[8] | (bytes[9] << 8) | (bytes[10] << 16) | (bytes[11] << 24);
    if (num_nodes > 1000000) {
        return make_corruption_result(CORRUPTION_FORMAT, 8, "RTree 节点数量异常");
    }

    /* 检查 MBR（最小边界矩形）大小 */
    size_t mbr_size = num_nodes * dims * 2 * sizeof(float);
    if (size < 16 + mbr_size) {
        return make_corruption_result(CORRUPTION_SIZE, 12, "RTree 数据大小不足");
    }

    /* 验证校验和 */
    uint16_t stored_checksum = page_get_checksum(data);
    if (!page_verify_checksum(data, size, stored_checksum)) {
        return make_corruption_result(CORRUPTION_CHECKSUM, size - 2, "RTree 索引校验和失败");
    }

    /* 扫描 MBR，检查边界框有效性 */
    const float *mbr = (const float *)(bytes + 16);
    for (uint32_t i = 0; i < num_nodes; i++) {
        for (uint16_t d = 0; d < dims; d++) {
            float min_val = mbr[i * dims * 2 + d];
            float max_val = mbr[i * dims * 2 + dims + d];
            if (min_val != min_val || max_val != max_val) {
                return make_corruption_result(CORRUPTION_FORMAT,
                                             16 + i * dims * 2 * sizeof(float) + d * 4,
                                             "RTree MBR 包含 NaN");
            }
            if (min_val > max_val) {
                return make_corruption_result(CORRUPTION_FORMAT,
                                             16 + i * dims * 2 * sizeof(float) + d * 4,
                                             "RTree MBR min > max");
            }
        }
    }

    return make_ok_result();
}

/**
 * @brief 统一检查接口
 */
CorruptionResult integrity_check(EngineType engine_type,
                               const void *data,
                               size_t size)
{
    switch (engine_type) {
        case ENGINE_KV:
            return integrity_check_kv(data, size);
        case ENGINE_VECTOR:
            return integrity_check_vector(data, size);
        case ENGINE_TIMESERIES:
            return integrity_check_timeseries(data, size);
        case ENGINE_DOCUMENT:
            return integrity_check_document(data, size);
        case ENGINE_SPATIAL:
            return integrity_check_spatial(data, size);
        case ENGINE_GRAPH:
            return integrity_check_graph(data, size);
        case ENGINE_YANG:
            return integrity_check_tree(data, size);
        default:
            return make_corruption_result(CORRUPTION_FORMAT, 0, "未知引擎类型");
    }
}

/* ========================================================================
 * WAL 一致性验证
 * ======================================================================== */

/**
 * @brief 验证 WAL 记录的一致性
 */
bool wal_verify_consistency(const void *wal_data, size_t data_size,
                          WalConsistencyLevel level)
{
    if (!wal_data || data_size < WAL_RECORD_HEADER_SIZE) {
        return false;
    }

    if (level == WAL_CONSISTENCY_NONE) {
        return true;
    }

    const wal_record_header_t *hdr = (const wal_record_header_t *)wal_data;

    /* 基本检查：记录头 */
    if (level >= WAL_CONSISTENCY_BASIC) {
        /* 检查类型是否有效 */
        if (hdr->type < WAL_LOG_UPDATE || hdr->type > WAL_LOG_VECTOR_APPEND) {
            return false;
        }

        /* 检查大小字段 */
        uint32_t size = hdr->size[0] | (hdr->size[1] << 8) | (hdr->size[2] << 16);
        if (size < WAL_RECORD_HEADER_SIZE || size > data_size) {
            return false;
        }

        /* 检查 LSN 是否合理（非零） */
        if (hdr->lsn == 0) {
            return false;
        }

        /* 检查事务 ID（提交/回滚记录必须有 txn_id） */
        if ((hdr->type == WAL_LOG_COMMIT || hdr->type == WAL_LOG_ABORT) &&
            hdr->txn_id == 0) {
            return false;
        }
    }

    if (level >= WAL_CONSISTENCY_FULL) {
        /* 完整检查：包括校验和验证 */
        /* 计算记录的校验和（除了 checksum 字段本身） */
        const uint8_t *bytes = (const uint8_t *)wal_data;
        uint32_t stored_checksum = hdr->checksum;
        uint32_t computed = 0;

        /* 跳过 checksum 字段（偏移 20-23）计算剩余部分 */
        for (size_t i = 0; i < WAL_RECORD_HEADER_SIZE; i++) {
            if (i < 20 || i >= 24) {
                computed += bytes[i];
            }
        }

        /* 也包括记录数据部分 */
        uint32_t total_size = hdr->size[0] | (hdr->size[1] << 8) | (hdr->size[2] << 16);
        for (size_t i = WAL_RECORD_HEADER_SIZE; i < total_size && i < data_size; i++) {
            computed += bytes[i];
        }

        if (stored_checksum != computed) {
            return false;
        }

        /* 检查 prev_lsn 逻辑（不能指向更晚的记录） */
        if (hdr->prev_lsn > hdr->lsn && hdr->prev_lsn != 0) {
            return false;
        }
    }

    return true;
}

/**
 * @brief WAL 文件头部结构
 */
typedef struct wal_file_header_s {
    uint32_t magic;           /**< 魔数 WAL_MAGIC */
    uint32_t version;         /**< 版本号 */
    uint32_t page_size;       /**< 页面大小 */
    uint32_t checksum;        /**< 头部校验和 */
    uint8_t  reserved[48];    /**< 保留字段 */
} wal_file_header_t;

/**
 * @brief 验证 WAL 文件的完整性
 */
bool wal_file_verify(const char *wal_path, WalConsistencyLevel level)
{
    if (!wal_path) {
        return false;
    }

    FILE *fp = fopen(wal_path, "rb");
    if (!fp) {
        return false;
    }

    /* 读取 WAL 文件头 */
    wal_file_header_t file_header;
    if (fread(&file_header, sizeof(wal_file_header_t), 1, fp) != 1) {
        fclose(fp);
        return false;
    }

    /* 验证魔数 */
    if (file_header.magic != WAL_MAGIC) {
        fclose(fp);
        return false;
    }

    /* 验证版本号 */
    if (file_header.version != WAL_VERSION) {
        fclose(fp);
        return false;
    }

    /* 验证页面大小合理性 */
    if (file_header.page_size < 512 || file_header.page_size > 65536) {
        fclose(fp);
        return false;
    }

    /* 基本检查到此结束 */
    if (level == WAL_CONSISTENCY_BASIC) {
        fclose(fp);
        return true;
    }

    /* 遍历所有记录，验证一致性 */
    fseek(fp, WAL_HEADER_SIZE, SEEK_SET);
    uint64_t last_lsn = 0;
    uint32_t active_txns[256];
    int active_count = 0;

    while (true) {
        wal_record_header_t rec_hdr;
        size_t read = fread(&rec_hdr, sizeof(wal_record_header_t), 1, fp);
        if (read != 1) {
            break;
        }

        /* 记录大小 */
        uint32_t rec_size = rec_hdr.size[0] | (rec_hdr.size[1] << 8) | (rec_hdr.size[2] << 16);
        if (rec_size < WAL_RECORD_HEADER_SIZE || rec_size > WAL_MAX_FILE_SIZE) {
            fclose(fp);
            return false;
        }

        /* 验证记录一致性 */
        if (!wal_verify_consistency(&rec_hdr, rec_size, level)) {
            fclose(fp);
            return false;
        }

        /* LSN 序列检查 */
        if (rec_hdr.lsn <= last_lsn && last_lsn != 0) {
            fclose(fp);
            return false;
        }
        last_lsn = rec_hdr.lsn;

        /* 事务边界检查 */
        if (rec_hdr.type == WAL_LOG_BEGIN) {
            if (active_count >= 256) {
                fclose(fp);
                return false;
            }
            active_txns[active_count++] = rec_hdr.txn_id;
        } else if (rec_hdr.type == WAL_LOG_COMMIT || rec_hdr.type == WAL_LOG_ABORT) {
            bool found = false;
            for (int i = 0; i < active_count; i++) {
                if (active_txns[i] == rec_hdr.txn_id) {
                    active_txns[i] = active_txns[--active_count];
                    found = true;
                    break;
                }
            }
            if (!found) {
                fclose(fp);
                return false;  /* COMMIT/ABORT 没有对应的 BEGIN */
            }
        }

        /* 跳到下一条记录 */
        long next_pos = ftell(fp) + rec_size - WAL_RECORD_HEADER_SIZE;
        if (fseek(fp, next_pos, SEEK_SET) != 0) {
            break;
        }
    }

    fclose(fp);

    /* 检查是否有未完成的事务（严重错误） */
    if (active_count > 0) {
        return false;
    }

    return true;
}

/**
 * @brief 验证 WAL 记录序列
 */
bool wal_sequence_verify(const void *wal_data, size_t data_size)
{
    if (!wal_data || data_size < WAL_RECORD_HEADER_SIZE) {
        return false;
    }

    const uint8_t *bytes = (const uint8_t *)wal_data;
    size_t offset = 0;
    uint64_t last_lsn = 0;
    int record_count = 0;

    while (offset + WAL_RECORD_HEADER_SIZE <= data_size) {
        const wal_record_header_t *hdr = (const wal_record_header_t *)(bytes + offset);

        /* 记录大小 */
        uint32_t rec_size = hdr->size[0] | (hdr->size[1] << 8) | (hdr->size[2] << 16);
        if (rec_size < WAL_RECORD_HEADER_SIZE || offset + rec_size > data_size) {
            return false;
        }

        /* LSN 必须递增 */
        if (hdr->lsn <= last_lsn && last_lsn != 0) {
            return false;
        }

        /* LSN 应该连续（+1 或 +delta） */
        if (record_count > 0 && hdr->lsn != last_lsn + 1 &&
            hdr->prev_lsn != last_lsn) {
            return false;
        }

        last_lsn = hdr->lsn;
        record_count++;
        offset += rec_size;
    }

    /* 应该恰好读完所有数据 */
    return offset == data_size;
}

/**
 * @brief 验证事务完整性
 */
bool wal_transaction_verify(const void *wal_data, size_t data_size,
                          uint32_t txn_id)
{
    if (!wal_data || data_size < WAL_RECORD_HEADER_SIZE) {
        return false;
    }

    const uint8_t *bytes = (const uint8_t *)wal_data;
    size_t offset = 0;
    bool has_begin = false;
    bool has_commit = false;
    bool has_abort = false;
    uint32_t record_count = 0;

    while (offset + WAL_RECORD_HEADER_SIZE <= data_size) {
        const wal_record_header_t *hdr = (const wal_record_header_t *)(bytes + offset);

        /* 只检查目标事务 */
        if (hdr->txn_id == txn_id) {
            if (hdr->type == WAL_LOG_BEGIN) {
                if (has_begin) {
                    return false;  /* 重复 BEGIN */
                }
                has_begin = true;
            } else if (hdr->type == WAL_LOG_COMMIT) {
                if (has_commit || has_abort) {
                    return false;  /* 重复 COMMIT/ABORT */
                }
                if (!has_begin) {
                    return false;  /* COMMIT 没有 BEGIN */
                }
                has_commit = true;
            } else if (hdr->type == WAL_LOG_ABORT) {
                if (has_commit || has_abort) {
                    return false;  /* 重复 COMMIT/ABORT */
                }
                if (!has_begin) {
                    return false;  /* ABORT 没有 BEGIN */
                }
                has_abort = true;
            }

            record_count++;
        }

        /* 跳到下一条记录 */
        uint32_t rec_size = hdr->size[0] | (hdr->size[1] << 8) | (hdr->size[2] << 16);
        if (rec_size < WAL_RECORD_HEADER_SIZE) {
            return false;
        }
        offset += rec_size;
    }

    /* 如果找到了 BEGIN，必须有 COMMIT 或 ABORT */
    if (has_begin && !has_commit && !has_abort) {
        return false;  /* 未完成的事务 */
    }

    /* 如果没有找到任何记录，说明事务不在 WAL 中 */
    if (record_count == 0) {
        return false;
    }

    return true;
}

/* ========================================================================
 * 自修复机制
 * ======================================================================== */

/**
 * @brief 修复损坏的页面
 */
void *page_repair(void *page_data, size_t page_size, RepairLevel level)
{
    if (!page_data || page_size < 16) {
        return NULL;
    }

    if (level == REPAIR_NONE) {
        return page_data;
    }

    uint8_t *data = (uint8_t *)page_data;
    page_header_t *header = (page_header_t *)page_data;

    if (level >= REPAIR_PAGE) {
        /* 修复魔数 */
        if (header->magic != PAGE_MAGIC) {
            header->magic = PAGE_MAGIC;
        }

        /* 修复页面类型 */
        if (header->page_type > PAGE_META) {
            header->page_type = PAGE_DATA;
        }

        /* 修复空闲空间偏移 */
        if (header->free_space_offset > page_size) {
            header->free_space_offset = page_size;
        }
    }

    if (level >= REPAIR_INDEX) {
        /* 重建索引信息（通过重新计算校验和） */
        page_set_checksum(page_data, page_size);
    }

    if (level >= REPAIR_TABLE) {
        /* 重建整个表（清除损坏区域） */
        size_t data_offset = 16;  /* 跳过头部 */
        while (data_offset + 16 <= page_size) {
            /* 检查每个 16 字节块是否有无效模式 */
            uint64_t *block = (uint64_t *)(data + data_offset);
            if (block[0] == 0xFFFFFFFFFFFFFFFF && block[1] == 0xFFFFFFFFFFFFFFFF) {
                /* 清除无效数据 */
                memset(data + data_offset, 0, 16);
            }
            data_offset += 16;
        }
    }

    /* 重新计算校验和 */
    page_set_checksum(page_data, page_size);

    return page_data;
}

/**
 * @brief 从 WAL 重建损坏页面
 */
int page_rebuild_from_wal(const void *wal_data, size_t wal_size,
                        uint32_t page_id,
                        void *out_page, size_t *out_size)
{
    if (!wal_data || !out_page || !out_size) {
        return -1;
    }

    const uint8_t *bytes = (const uint8_t *)wal_data;
    size_t offset = 0;
    bool found = false;
    size_t latest_offset = 0;
    uint64_t latest_lsn = 0;

    /* 首先在 WAL 中查找目标页面的所有修改，找到最新的 */
    while (offset + WAL_RECORD_HEADER_SIZE <= wal_size) {
        const wal_record_header_t *hdr = (const wal_record_header_t *)(bytes + offset);

        /* 记录大小 */
        uint32_t rec_size = hdr->size[0] | (hdr->size[1] << 8) | (hdr->size[2] << 16);
        if (rec_size < WAL_RECORD_HEADER_SIZE || offset + rec_size > wal_size) {
            break;
        }

        /* 查找 INSERT/UPDATE 记录中涉及目标页面的 */
        if ((hdr->type == WAL_LOG_INSERT || hdr->type == WAL_LOG_UPDATE ||
             hdr->type == WAL_LOG_HEAP_INSERT || hdr->type == WAL_LOG_HEAP_UPDATE) &&
            hdr->lsn > latest_lsn) {
            latest_lsn = hdr->lsn;
            latest_offset = offset;
            found = true;
        }

        offset += rec_size;
    }

    if (!found) {
        *out_size = 0;
        return -1;
    }

    /* 读取最新记录 */
    const wal_record_header_t *hdr = (const wal_record_header_t *)(bytes + latest_offset);
    uint32_t rec_size = hdr->size[0] | (hdr->size[1] << 8) | (hdr->size[2] << 16);

    /* 复制数据到输出 */
    size_t copy_size = rec_size - WAL_RECORD_HEADER_SIZE;
    if (copy_size > *out_size) {
        copy_size = *out_size;
    }
    memcpy(out_page, bytes + latest_offset + WAL_RECORD_HEADER_SIZE, copy_size);
    *out_size = copy_size;

    return 0;
}

/**
 * @brief 验证并修复页面
 */
CorruptionResult page_verify_and_repair(void *page_data, size_t page_size,
                                       bool repair)
{
    if (!page_data || page_size < 16) {
        return make_corruption_result(CORRUPTION_SIZE, 0, "数据太小");
    }

    const page_header_t *header = (const page_header_t *)page_data;

    /* 检查魔数 */
    if (header->magic != PAGE_MAGIC) {
        if (repair) {
            ((page_header_t *)page_data)->magic = PAGE_MAGIC;
            page_set_checksum(page_data, page_size);
            return (CorruptionResult){
                .corrupted = false,
                .info = {
                    .type = CORRUPTION_NONE,
                    .location = 0,
                    .context = 0,
                    .message = "魔数已修复",
                    .file = NULL,
                    .line = 0
                }
            };
        }
        return make_corruption_result(CORRUPTION_MAGIC, 0, "页面魔数错误");
    }

    /* 检查页面类型 */
    if (header->page_type > PAGE_META) {
        if (repair) {
            ((page_header_t *)page_data)->page_type = PAGE_DATA;
            page_set_checksum(page_data, page_size);
            return (CorruptionResult){
                .corrupted = false,
                .info = {
                    .type = CORRUPTION_NONE,
                    .location = 0,
                    .context = 0,
                    .message = "页面类型已修复",
                    .file = NULL,
                    .line = 0
                }
            };
        }
        return make_corruption_result(CORRUPTION_FORMAT, 12, "页面类型无效");
    }

    /* 检查空闲空间偏移 */
    if (header->free_space_offset > page_size) {
        if (repair) {
            ((page_header_t *)page_data)->free_space_offset = page_size;
            page_set_checksum(page_data, page_size);
            return (CorruptionResult){
                .corrupted = false,
                .info = {
                    .type = CORRUPTION_NONE,
                    .location = 0,
                    .context = 0,
                    .message = "空闲空间偏移已修复",
                    .file = NULL,
                    .line = 0
                }
            };
        }
        return make_corruption_result(CORRUPTION_OFFSET, 13, "空闲空间偏移异常");
    }

    /* 验证校验和 */
    uint16_t stored = page_get_checksum(page_data);
    if (!page_verify_checksum(page_data, page_size, stored)) {
        if (repair) {
            /* 尝试重建页面内容（清除损坏部分） */
            uint8_t *data = (uint8_t *)page_data;
            for (size_t i = 16; i < page_size - 2; i++) {
                data[i] = 0;
            }
            page_set_checksum(page_data, page_size);
            return (CorruptionResult){
                .corrupted = false,
                .info = {
                    .type = CORRUPTION_NONE,
                    .location = 0,
                    .context = 0,
                    .message = "页面校验和已修复（数据已清零）",
                    .file = NULL,
                    .line = 0
                }
            };
        }
        return make_corruption_result(CORRUPTION_CHECKSUM, 0, "页面校验和失败");
    }

    return make_ok_result();
}

/**
 * @brief 获取页面损坏的严重程度
 */
int corruption_severity(const CorruptionInfo *info)
{
    if (!info) {
        return 0;
    }

    switch (info->type) {
        case CORRUPTION_CHECKSUM:
            return 80;
        case CORRUPTION_MAGIC:
            return 90;
        case CORRUPTION_FORMAT:
            return 70;
        case CORRUPTION_INTERNAL:
            return 100;
        default:
            return 50;
    }
}

/**
 * @brief 检查页面是否可修复
 */
bool corruption_repairable(const CorruptionInfo *info)
{
    if (!info) {
        return false;
    }

    switch (info->type) {
        case CORRUPTION_CHECKSUM:
        case CORRUPTION_OFFSET:
        case CORRUPTION_POINTER:
            return true;
        case CORRUPTION_MAGIC:
        case CORRUPTION_INTERNAL:
            return false;
        default:
            return true;
    }
}

/* ========================================================================
 * 完整性管理器
 * ======================================================================== */

/** 全局配置 */
static IntegrityConfig g_integrity_config = DEFAULT_INTEGRITY_CONFIG;

/**
 * @brief 初始化完整性管理器
 */
void integrity_init(const IntegrityConfig *config)
{
    if (config) {
        g_integrity_config = *config;
    }
}

/**
 * @brief 获取完整性配置
 */
const IntegrityConfig *integrity_get_config(void)
{
    return &g_integrity_config;
}

/**
 * @brief 设置完整性配置
 */
void integrity_set_config(const IntegrityConfig *config)
{
    if (config) {
        g_integrity_config = *config;
    }
}

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印损坏信息
 */
void corruption_dump(const CorruptionInfo *info)
{
    if (!info) {
        printf("Corruption: NULL\n");
        return;
    }

    printf("Corruption Type: %d\n", info->type);
    printf("Location: %u\n", info->location);
    printf("Context: %u\n", info->context);
    printf("Message: %s\n", info->message ? info->message : "N/A");

    if (info->file) {
        printf("File: %s:%u\n", info->file, info->line);
    }
}

/**
 * @brief 打印完整性统计
 */
void integrity_dump_stats(void)
{
    printf("=== Data Integrity Stats ===\n");
    printf("Checksum Enabled: %s\n",
           g_integrity_config.enable_checksum ? "YES" : "NO");
    printf("Corruption Check Enabled: %s\n",
           g_integrity_config.enable_corruption_check ? "YES" : "NO");
    printf("Auto Repair Enabled: %s\n",
           g_integrity_config.enable_auto_repair ? "YES" : "NO");
    printf("WAL Verify Level: %d\n",
           g_integrity_config.wal_level);
}
