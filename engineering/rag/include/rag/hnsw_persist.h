/**
 * @file hnsw_persist.h
 * @brief HNSW 索引持久化
 *
 * 实现 HNSW 索引的序列化（save）和反序列化（load），
 * 支持增量插入和删除操作。
 */
#ifndef RAG_HNSW_PERSIST_H
#define RAG_HNSW_PERSIST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** HNSW 持久化文件魔数 "HNSW" */
#define HNSW_PERSIST_MAGIC 0x484E5357

/** HNSW 持久化版本 */
#define HNSW_PERSIST_VERSION 1

/** 文件头大小 */
#define HNSW_PERSIST_HEADER_SIZE 64

/* ========================================================================
 * 元数据
 * ======================================================================== */

/**
 * @brief HNSW 索引元数据
 */
typedef struct hnsw_persist_meta_s {
    uint32_t magic;              /**< 魔数 */
    uint32_t version;            /**< 版本号 */
    int32_t dims;                /**< 向量维度 */
    int32_t n_total;             /**< 向量数量 */
    int32_t M;                   /**< 每个节点的连接数 */
    int32_t ef_construction;     /**< 构建时的搜索范围 */
    int32_t ef_search;           /**< 查询时的搜索范围 */
    int32_t max_level;           /**< 最大层数 */
    int32_t entry_pointer;       /**< 入口节点 */
    int32_t metric;              /**< 距离度量类型 */
    int32_t quantization_type;   /**< 量化类型 */
    int32_t code_size;           /**< 编码大小 */
    int64_t created_at;          /**< 创建时间戳 */
    int64_t modified_at;         /**< 修改时间戳 */
    uint64_t data_size;          /**< 数据部分大小 */
    int32_t deleted_count;       /**< 已删除向量数量 */
    int32_t reserved[7];         /**< 保留字段（对齐到 64 字节） */
} hnsw_persist_meta_t;

/**
 * @brief 持久化结果
 */
typedef struct hnsw_persist_result_s {
    int32_t success;             /**< 是否成功 */
    int32_t error_code;          /**< 错误码 */
    char error_msg[256];         /**< 错误信息 */
    uint64_t bytes_written;      /**< 写入字节数 */
    uint64_t bytes_read;         /**< 读取字节数 */
} hnsw_persist_result_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/**
 * @brief 保存 HNSW 索引到文件
 *
 * @param index HNSW 索引指针
 * @param path 文件路径
 * @param result 保存结果（可为 NULL）
 * @return 0 成功，-1 失败
 */
int hnsw_persist_save(const void *index, const char *path,
                      hnsw_persist_result_t *result);

/**
 * @brief 从文件加载 HNSW 索引
 *
 * @param path 文件路径
 * @param result 加载结果（可为 NULL）
 * @return 索引指针，失败返回 NULL
 */
void *hnsw_persist_load(const char *path, hnsw_persist_result_t *result);

/**
 * @brief 获取索引元数据（不加载完整数据）
 *
 * @param path 文件路径
 * @param meta 元数据输出（不为 NULL）
 * @return 0 成功，-1 失败
 */
int hnsw_persist_get_meta(const char *path, hnsw_persist_meta_t *meta);

/**
 * @brief 检查索引文件是否存在且有效
 *
 * @param path 文件路径
 * @return true 有效，false 无效或不存在
 */
bool hnsw_persist_is_valid(const char *path);

/**
 * @brief 增量插入向量
 *
 * @param index HNSW 索引指针
 * @param n 插入数量
 * @param vectors 向量数据（连续存储）
 * @return 0 成功，-1 失败
 */
int hnsw_persist_add(void *index, int32_t n, const float *vectors);

/**
 * @brief 增量删除向量（标记删除）
 *
 * @param index HNSW 索引指针
 * @param vec_id 要删除的向量 ID
 * @return 0 成功，-1 失败
 */
int hnsw_persist_delete(void *index, int32_t vec_id);

/**
 * @brief 重建索引（物理删除标记的向量）
 *
 * @param index HNSW 索引指针
 * @return 0 成功，-1 失败
 */
int hnsw_persist_compact(void *index);

/**
 * @brief 获取索引统计信息
 *
 * @param index HNSW 索引指针
 * @param total_count 向量总数输出
 * @param deleted_count 已删除数量输出
 * @param memory_usage 内存使用量输出
 */
void hnsw_persist_stats(const void *index, int32_t *total_count,
                        int32_t *deleted_count, uint64_t *memory_usage);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取文件大小
 *
 * @param path 文件路径
 * @return 文件大小，失败返回 -1
 */
int64_t hnsw_persist_file_size(const char *path);

/**
 * @brief 验证文件头
 *
 * @param header 文件头数据
 * @return true 有效，false 无效
 */
bool hnsw_persist_verify_header(const void *header);

#ifdef __cplusplus
}
#endif

#endif /* HNSW_PERSIST_H */
