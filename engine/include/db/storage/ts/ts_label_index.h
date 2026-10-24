/**
 * @file ts_label_index.h
 * @brief 时序标签倒排索引接口
 */
#ifndef DB_STORAGE_TS_LABEL_INDEX_H
#define DB_STORAGE_TS_LABEL_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct ts_label_index_t ts_label_index_t;

/**
 * @brief 创建标签索引
 * @param high_cardinality_threshold 高基数阈值（默认 10000）
 * @return 索引指针，失败返回 NULL
 */
ts_label_index_t *ts_label_index_create(int64_t high_cardinality_threshold);

/**
 * @brief 销毁标签索引
 * @param idx 索引指针
 */
void ts_label_index_destroy(ts_label_index_t *idx);

/**
 * @brief 检查是否为高基数标签
 * @param idx 索引指针
 * @return true 表示高基数
 */
bool ts_label_index_is_high_cardinality(ts_label_index_t *idx);

/**
 * @brief 添加标签
 * @param idx 索引指针
 * @param series_id 序列 ID
 * @param label_key 标签键
 * @param label_value 标签值
 * @return 0 成功，-1 失败
 */
int ts_label_index_add(ts_label_index_t *idx, uint64_t series_id,
                       const char *label_key, const char *label_value);

/**
 * @brief 移除标签
 * @param idx 索引指针
 * @param series_id 序列 ID
 * @return 0 成功，-1 失败
 */
int ts_label_index_remove(ts_label_index_t *idx, uint64_t series_id);

/**
 * @brief 搜索标签
 * @param idx 索引指针
 * @param query 查询字符串
 * @param out_series_ids 输出序列 ID 数组
 * @param max_results 最大结果数
 * @param out_count 输出结果数
 * @return 0 成功，-1 失败
 */
int ts_label_index_search(ts_label_index_t *idx, const char *query,
                          uint64_t *out_series_ids, uint32_t max_results,
                          uint32_t *out_count);

/**
 * @brief 获取标签数量
 * @param idx 索引指针
 * @return 标签数量
 */
int64_t ts_label_index_count(ts_label_index_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_TS_LABEL_INDEX_H */