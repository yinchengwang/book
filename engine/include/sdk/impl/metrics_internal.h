/**
 * @file metrics_internal.h
 * @brief 内部埋点 API（仅供 src/sdk/ 下的 .c 文件使用）
 *
 * 提供给 SDK 内部模块埋点使用，不对外暴露。
 * 公共 API 见 mmdb_metrics.h。
 */
#ifndef SDK_IMPL_METRICS_INTERNAL_H
#define SDK_IMPL_METRICS_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 记录一次查询（成功或失败），同时累计延迟
 *
 * @param latency_ms 查询耗时（毫秒）
 * @param success    1 = 成功，0 = 失败
 */
void mmdb_metrics_record_query(double latency_ms, int success);

/**
 * @brief 记录一次缓存命中/未命中
 *
 * @param hit 1 = 命中，0 = 未命中
 */
void mmdb_metrics_record_cache(int hit);

/**
 * @brief 记录 HNSW 索引构建耗时
 *
 * @param time_ms 构建耗时（毫秒）
 */
void mmdb_metrics_record_hnsw_build(double time_ms);

/**
 * @brief 设置向量总数（用于启动时初始化）
 */
void mmdb_metrics_set_vectors_total(uint64_t n);

/**
 * @brief 增加向量总数（insert 时调用）
 */
void mmdb_metrics_inc_vectors_total(uint64_t delta);

/**
 * @brief 减少向量总数（delete 时调用，自动避免下溢）
 */
void mmdb_metrics_dec_vectors_total(uint64_t delta);

/**
 * @brief 更新资源使用快照
 */
void mmdb_metrics_set_resources(size_t mem_used, size_t mem_total,
                                size_t disk_used, size_t disk_total);

#ifdef __cplusplus
}
#endif

#endif /* SDK_IMPL_METRICS_INTERNAL_H */
