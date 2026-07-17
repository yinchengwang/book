/**
 * @file vector_index_persist.h
 * @brief 向量索引统一持久化格式
 *
 * ============================================================================
 * 设计目标
 * ============================================================================
 *
 * 定义统一的向量索引持久化格式，支持 HNSW、DiskANN、IVF-PQ 等索引类型。
 * 提供版本兼容性和完整性校验。
 *
 * ============================================================================
 * 文件格式
 * ============================================================================
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                     统一文件头 (128 bytes)                           │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │  magic[4]     : 魔数 "VIDX" (0x56494458)                           │
 * │  version[4]   : 版本号                                              │
 * │  index_type[4]: 索引类型                                             │
 * │  dims[4]      : 向量维度                                             │
 * │  n_total[8]   : 向量总数                                             │
 * │  created[8]   : 创建时间戳                                           │
 * │  modified[8]  : 修改时间戳                                           │
 * │  checksum[4]  : 头部校验和                                           │
 * │  reserved[84] : 保留字段                                             │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │                     索引特定数据区                                    │
 * │  (格式由 index_type 决定)                                           │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * ============================================================================
 * 索引类型
 * ============================================================================
 *
 * | 类型值 | 名称       | 说明                    |
 * |--------|------------|------------------------|
 * | 0      | HNSW       | 分层可导航小世界        |
 * | 1      | DiskANN    | 磁盘友好的近似最近邻    |
 * | 2      | IVF_PQ     | 倒排文件+乘积量化       |
 * | 3      | IVFFlat    | 倒排文件+原始向量       |
 * | 4      | LSH        | 局部敏感哈希            |
 * | 5      | OPQ        | 优化产品量化            |
 *
 * ============================================================================
 * 版本历史
 * ============================================================================
 *
 * - v1 (当前): 初始版本，支持 HNSW/DiskANN/IVF-PQ
 * - v2 (规划): 添加增量更新支持
 *
 * ============================================================================
 * 校验和方案
 * ============================================================================
 *
 * 使用 CRC32 校验和：
 * - 头部校验和：覆盖整个头部 (不包括 checksum 字段自身)
 * - 数据校验和：覆盖整个索引数据区
 *
 * CRC32 多项式: 0xEDB88320
 *
 */
#ifndef VECTOR_INDEX_PERSIST_H
#define VECTOR_INDEX_PERSIST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 魔数 "VIDX" (Vector Index) */
#define VECTOR_INDEX_MAGIC     0x56494458

/** 当前版本号 */
#define VECTOR_INDEX_VERSION   1

/** 文件头大小 (bytes) */
#define VECTOR_INDEX_HEADER_SIZE  128

/** 头部校验和字段偏移 */
#define VECTOR_INDEX_CHECKSUM_OFFSET 56

/** 最大版本号 (用于版本兼容性检查) */
#define VECTOR_INDEX_MAX_SUPPORTED_VERSION 1

/** CRC32 多项式 */
#define VECTOR_INDEX_CRC32_POLY  0xEDB88320

/* ========================================================================
 * 索引类型
 * ======================================================================== */

/**
 * @brief 向量索引类型
 */
typedef enum vector_index_type_e {
    VECTOR_INDEX_TYPE_HNSW    = 0,  /**< 分层可导航小世界 */
    VECTOR_INDEX_TYPE_DISKANN = 1,  /**< 磁盘友好的近似最近邻 */
    VECTOR_INDEX_TYPE_IVF_PQ  = 2,  /**< 倒排文件 + 乘积量化 */
    VECTOR_INDEX_TYPE_IVF_FLAT = 3, /**< 倒排文件 + 原始向量 */
    VECTOR_INDEX_TYPE_LSH     = 4,  /**< 局部敏感哈希 */
    VECTOR_INDEX_TYPE_OPQ     = 5,  /**< 优化产品量化 */
    VECTOR_INDEX_TYPE_UNKNOWN = 99  /**< 未知类型 */
} vector_index_type_t;

/**
 * @brief 获取索引类型名称
 */
static inline const char *vector_index_type_name(vector_index_type_t type) {
    switch (type) {
        case VECTOR_INDEX_TYPE_HNSW:    return "HNSW";
        case VECTOR_INDEX_TYPE_DISKANN: return "DiskANN";
        case VECTOR_INDEX_TYPE_IVF_PQ:  return "IVF-PQ";
        case VECTOR_INDEX_TYPE_IVF_FLAT: return "IVF-Flat";
        case VECTOR_INDEX_TYPE_LSH:     return "LSH";
        case VECTOR_INDEX_TYPE_OPQ:     return "OPQ";
        default:                        return "Unknown";
    }
}

/* ========================================================================
 * 统一文件头
 * ======================================================================== */

/**
 * @brief 向量索引统一文件头
 *
 * 所有向量索引类型使用统一的头部格式，便于：
 * - 快速识别索引类型
 * - 验证文件完整性
 * - 支持版本兼容性
 */
#pragma pack(push, 1)
typedef struct vector_index_header_s {
    /* 魔数和版本 */
    uint32_t magic;           /**< 魔数 0x56494458 ("VIDX") */
    uint32_t version;         /**< 版本号 (当前为 1) */

    /* 索引信息 */
    uint32_t index_type;      /**< 索引类型 (见 vector_index_type_t) */
    int32_t  dims;            /**< 向量维度 */
    uint64_t n_total;         /**< 向量总数 */

    /* 时间戳 */
    int64_t  created_at;      /**< 创建时间戳 (Unix epoch) */
    int64_t  modified_at;     /**< 最后修改时间戳 */

    /* 校验和 */
    uint32_t header_checksum; /**< 头部校验和 (CRC32) */
    uint32_t data_checksum;   /**< 数据区校验和 (CRC32) */

    /* 统计信息 */
    uint64_t data_size;       /**< 数据区大小 (bytes) */
    uint32_t flags;           /**< 标志位 */

    /* 保留字段 */
    uint8_t  reserved[64];    /**< 保留字段 (用于未来扩展) */
} vector_index_header_t;
#pragma pack(pop)

/* ========================================================================
 * 标志位定义
 * ======================================================================== */

/** 索引已构建完成 */
#define VECTOR_INDEX_FLAG_BUILT       0x0001

/** 索引支持增量更新 */
#define VECTOR_INDEX_FLAG_INCREMENTAL 0x0002

/** 索引已压缩 */
#define VECTOR_INDEX_FLAG_COMPRESSED  0x0004

/** 使用量化 */
#define VECTOR_INDEX_FLAG_QUANTIZED   0x0008

/** 使用增量索引 (如 FreshDiskANN) */
#define VECTOR_INDEX_FLAG_DYNAMIC     0x0010

/* ========================================================================
 * 校验和计算
 * ======================================================================== */

/**
 * @brief 计算 CRC32 校验和
 * @param data 数据指针
 * @param size 数据大小
 * @return CRC32 校验和
 */
uint32_t vector_index_crc32(const void *data, size_t size);

/**
 * @brief 验证头部校验和
 * @param header 文件头指针
 * @return 校验通过返回 true
 */
bool vector_index_verify_header_checksum(const vector_index_header_t *header);

/**
 * @brief 计算并设置头部校验和
 * @param header 文件头指针
 */
void vector_index_set_header_checksum(vector_index_header_t *header);

/**
 * @brief 验证文件头
 * @param header 文件头指针
 * @return 验证通过返回 true
 */
bool vector_index_verify_header(const vector_index_header_t *header);

/* ========================================================================
 * 文件操作
 * ======================================================================== */

/**
 * @brief 读取文件头
 * @param fp 文件指针 (已打开)
 * @param header 输出头部
 * @return 成功返回 0
 */
int vector_index_read_header(FILE *fp, vector_index_header_t *header);

/**
 * @brief 写入文件头
 * @param fp 文件指针 (已打开)
 * @param header 文件头
 * @return 成功返回 0
 */
int vector_index_write_header(FILE *fp, const vector_index_header_t *header);

/**
 * @brief 读取索引数据区
 * @param fp 文件指针
 * @param offset 数据区偏移
 * @param size 数据大小
 * @param buffer 输出缓冲区
 * @return 读取的字节数
 */
size_t vector_index_read_data(FILE *fp, uint64_t offset, size_t size, void *buffer);

/**
 * @brief 写入索引数据区
 * @param fp 文件指针
 * @param offset 数据区偏移
 * @param size 数据大小
 * @param data 数据指针
 * @return 写入的字节数
 */
size_t vector_index_write_data(FILE *fp, uint64_t offset, size_t size, const void *data);

/* ========================================================================
 * 版本兼容性
 * ======================================================================== */

/**
 * @brief 检查版本兼容性
 * @param version 文件版本号
 * @return 兼容返回 true
 */
bool vector_index_check_version_compatibility(uint32_t version);

/**
 * @brief 获取升级建议
 * @param from_version 源版本
 * @param to_version 目标版本
 * @return 建议描述 (调用方负责释放)
 */
char *vector_index_get_upgrade_hint(uint32_t from_version, uint32_t to_version);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 检测文件类型
 * @param path 文件路径
 * @return 索引类型，未知返回 VECTOR_INDEX_TYPE_UNKNOWN
 */
vector_index_type_t vector_index_detect_type(const char *path);

/**
 * @brief 获取文件大小
 * @param path 文件路径
 * @return 文件大小，失败返回 -1
 */
int64_t vector_index_file_size(const char *path);

/**
 * @brief 检查索引文件是否有效
 * @param path 文件路径
 * @return 有效返回 true
 */
bool vector_index_is_valid(const char *path);

/**
 * @brief 获取索引文件信息摘要
 * @param path 文件路径
 * @param summary 输出摘要 (需预分配 256 字节)
 * @return 成功返回 0
 */
int vector_index_get_summary(const char *path, char *summary);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_INDEX_PERSIST_H */
