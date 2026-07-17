/**
 * @file ts_tag_index.h
 * @brief 时序数据 Tag 倒排索引头文件
 *
 * 实现基于 Tag 的倒排索引，支持：
 * 1. Tag 值到时间序列的映射
 * 2. 多 Tag 组合查询
 * 3. Tag 基数统计
 */
#ifndef DB_TS_TAG_INDEX_H
#define DB_TS_TAG_INDEX_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Tag 定义
 * ======================================================================== */

/** Tag 值类型 */
typedef enum TagValueType_e {
    TAG_STRING = 0,   /**< 字符串类型 */
    TAG_INT = 1,      /**< 整数类型 */
    TAG_FLOAT = 2     /**< 浮点类型 */
} TagValueType;

/** Tag 值联合体 */
typedef union TagValue_u {
    int64_t int_val;
    double float_val;
    struct {
        char *str;
        int len;
    } str_val;
} TagValue;

/** 单个 Tag */
typedef struct Tag_s {
    char key[64];              /**< Tag 键名 */
    TagValueType value_type;   /**< 值类型 */
    TagValue value;            /**< 值 */
} Tag;

/** Tag 集合 */
typedef struct TagSet_s {
    Tag *tags;
    uint32_t count;
    uint32_t capacity;
} TagSet;

/**
 * @brief 创建 Tag 集合
 */
TagSet *tagset_create(uint32_t capacity);

/**
 * @brief 添加 Tag
 */
int tagset_add(TagSet *set, const char *key, const char *str_value);
int tagset_add_int(TagSet *set, const char *key, int64_t int_value);
int tagset_add_float(TagSet *set, const char *key, double float_value);

/**
 * @brief 获取 Tag
 */
const Tag *tagset_get(const TagSet *set, const char *key);

/**
 * @brief 检查 Tag 是否存在
 */
bool tagset_contains(const TagSet *set, const char *key);

/**
 * @brief 释放 Tag 集合
 */
void tagset_free(TagSet *set);

/* ========================================================================
 * 倒排索引
 * ======================================================================== */

/** 倒排列表项 */
typedef struct InvertedEntry_s {
    int64_t series_id;     /**< 时间序列 ID */
    int64_t timestamp;     /**< 时间戳 */
    uint32_t count;        /**< 出现次数 */
} InvertedEntry;

/** 倒排列表 */
typedef struct InvertedList_s {
    InvertedEntry *entries;  /**< 条目数组 */
    uint32_t count;          /**< 条目数 */
    uint32_t capacity;       /**< 容量 */
} InvertedList;

/** Tag 倒排索引 */
typedef struct TagIndex_s {
    char data_dir[512];       /**< 数据目录 */

    /* 索引结构：tag_key -> tag_value -> series_ids */
    void *index_root;         /**< 索引根节点 */

    /* 统计信息 */
    uint64_t total_series;    /**< 总时间序列数 */
    uint64_t total_tags;      /**< 总 Tag 数 */
    uint64_t index_size;      /**< 索引大小 */
    uint64_t query_count;     /**< 查询次数 */

    /* 元数据 */
    uint32_t num_keys;        /**< Tag 键数 */
} TagIndex;

/**
 * @brief 创建 Tag 索引
 */
TagIndex *tag_index_create(const char *data_dir);

/**
 * @brief 打开已存在的 Tag 索引
 */
TagIndex *tag_index_open(const char *data_dir);

/**
 * @brief 保存 Tag 索引
 */
int tag_index_save(TagIndex *index);

/**
 * @brief 销毁 Tag 索引
 */
void tag_index_destroy(TagIndex *index);

/* ========================================================================
 * 索引操作
 * ======================================================================== */

/**
 * @brief 注册时间序列
 *
 * @param index Tag 索引
 * @param series_id 时间序列 ID
 * @param tags Tag 集合
 * @return 0 成功，-1 失败
 */
int tag_index_register_series(TagIndex *index, int64_t series_id, const TagSet *tags);

/**
 * @brief 注销时间序列
 *
 * @param index Tag 索引
 * @param series_id 时间序列 ID
 * @return 0 成功，-1 失败
 */
int tag_index_unregister_series(TagIndex *index, int64_t series_id);

/**
 * @brief 更新序列 Tag
 */
int tag_index_update_series(TagIndex *index, int64_t series_id, const TagSet *new_tags);

/**
 * @brief 记录数据点（用于索引关联）
 */
int tag_index_record_point(TagIndex *index, int64_t series_id, int64_t timestamp);

/**
 * @brief 批量记录数据点
 */
int tag_index_record_points_batch(TagIndex *index,
                                  const int64_t *series_ids,
                                  const int64_t *timestamps,
                                  uint32_t count);

/* ========================================================================
 * 查询操作
 * ======================================================================== */

/** 查询条件操作符 */
typedef enum TagQueryOp_e {
    TAG_OP_EQ = 0,        /**< 等于 */
    TAG_OP_NE = 1,        /**< 不等于 */
    TAG_OP_IN = 2,        /**< 包含 */
    TAG_OP_REGEX = 3,     /**< 正则匹配 */
    TAG_OP_EXISTS = 4,    /**< 存在 */
    TAG_OP_NOT_EXISTS = 5 /**< 不存在 */
} TagQueryOp;

/** 查询条件 */
typedef struct TagQuery_s {
    char key[64];              /**< Tag 键名 */
    TagQueryOp op;             /**< 操作符 */
    TagValue value;            /**< 值 */
    TagValueType value_type;   /**< 值类型 */
    char **in_values;          /**< IN 操作的值列表 */
    uint32_t in_count;         /**< IN 值数量 */
    char *regex_pattern;       /**< 正则表达式 */
} TagQuery;

/** 查询结果 */
typedef struct TagQueryResult_s {
    int64_t *series_ids;       /**< 匹配的序列 ID */
    uint32_t count;            /**< 匹配数量 */
    uint32_t capacity;         /**< 容量 */
} TagQueryResult;

/**
 * @brief 创建 Tag 查询
 */
TagQuery *tag_query_create(const char *key, TagQueryOp op);

/**
 * @brief 添加 IN 值
 */
int tag_query_add_in_value(TagQuery *query, const char *value);

/**
 * @brief 释放 Tag 查询
 */
void tag_query_free(TagQuery *query);

/**
 * @brief 执行单条件查询
 *
 * @param index Tag 索引
 * @param query 查询条件
 * @param result 输出结果（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int tag_index_query(const TagIndex *index, const TagQuery *query, TagQueryResult *result);

/**
 * @brief 执行多条件查询（AND 组合）
 *
 * @param index Tag 索引
 * @param queries 查询条件数组
 * @param num_queries 条件数量
 * @param result 输出结果
 * @return 0 成功，-1 失败
 */
int tag_index_query_and(const TagIndex *index,
                        const TagQuery **queries,
                        uint32_t num_queries,
                        TagQueryResult *result);

/**
 * @brief 执行多条件查询（OR 组合）
 */
int tag_index_query_or(const TagIndex *index,
                       const TagQuery **queries,
                       uint32_t num_queries,
                       TagQueryResult *result);

/**
 * @brief 释放查询结果
 */
void tag_query_result_free(TagQueryResult *result);

/**
 * @brief 获取匹配数量（不返回 ID）
 */
uint32_t tag_index_count(const TagIndex *index, const TagQuery *query);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/** Tag 基数统计 */
typedef struct TagCardinality_s {
    char key[64];              /**< Tag 键名 */
    uint64_t cardinality;      /**< 基数（唯一值数） */
    uint64_t total_count;      /**< 总出现次数 */
} TagCardinality;

/**
 * @brief 获取 Tag 基数统计
 */
int tag_index_cardinality(const TagIndex *index,
                          const char *key,
                          TagCardinality *out_stats);

/**
 * @brief 获取所有 Tag 键的基数
 */
int tag_index_all_cardinalities(const TagIndex *index,
                                TagCardinality **out_stats,
                                uint32_t *out_count);

/**
 * @brief 获取最常见的 Tag 值
 */
int tag_index_top_values(const TagIndex *index,
                         const char *key,
                         uint32_t top_n,
                         char ***out_values,
                         uint64_t **out_counts,
                         uint32_t *out_count);

/* ========================================================================
 * 索引维护
 * ======================================================================== */

/**
 * @brief 优化索引
 *
 * 合并碎片，提高查询性能
 */
int tag_index_optimize(TagIndex *index);

/**
 * @brief 获取索引统计
 */
void tag_index_stats(const TagIndex *index,
                     uint64_t *out_total_series,
                     uint64_t *out_total_tags,
                     uint64_t *out_index_size);

/* ========================================================================
 * 降采样索引
 * ======================================================================== */

/** 降采样类型 */
typedef enum DownsampleType_e {
    DOWNSAMPLE_NONE = 0,       /**< 不降采样 */
    DOWNSAMPLE_MIN = 1,        /**< 保留最小值 */
    DOWNSAMPLE_MAX = 2,        /**< 保留最大值 */
    DOWNSAMPLE_AVG = 3,        /**< 保留平均值 */
    DOWNSAMPLE_FIRST = 4,      /**< 保留第一个值 */
    DOWNSAMPLE_LAST = 5        /**< 保留最后一个值 */
} DownsampleType;

/**
 * @brief 创建降采样索引
 *
 * 用于快速预览和降分辨率查询
 */
int tag_index_create_downsample(TagIndex *index,
                                const char *series_key,
                                int64_t interval_ms,
                                DownsampleType type);

/**
 * @brief 查询降采样数据
 */
int tag_index_query_downsample(const TagIndex *index,
                               const char *series_key,
                               int64_t start_time,
                               int64_t end_time,
                               int64_t interval_ms,
                               DownsampleType type,
                               double *out_values,
                               int64_t *out_timestamps,
                               uint32_t max_count,
                               uint32_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* DB_TS_TAG_INDEX_H */
