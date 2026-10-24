/**
 * @file include/db/index/hash/cceh.h
 * @brief CCEH (Cache-Conscious Extendible Hashing) 索引公共头文件
 */
#ifndef DB_INDEX_CCEH_H
#define DB_INDEX_CCEH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CCEH_DEFAULT_SEGMENT_CAPACITY 8u
#define CCEH_DEFAULT_GLOBAL_DEPTH     1u
#define CCEH_MIN_SEGMENT_CAPACITY     2u

/* ── 前向声明 ── */
typedef struct cceh_index cceh_index_t;

/* ── 生命周期 ── */
cceh_index_t *cceh_index_create(uint32_t segment_capacity, uint32_t initial_global_depth);
void cceh_index_drop(cceh_index_t *index);

/* ── 核心操作 ── */
int cceh_index_insert(cceh_index_t *index,
                      const void *key,   uint32_t keylen,
                      const void *value, uint32_t valuelen);
int cceh_index_upsert(cceh_index_t *index,
                      const void *key,   uint32_t keylen,
                      const void *value, uint32_t valuelen);
int cceh_index_delete(cceh_index_t *index,
                      const void *key, uint32_t keylen);
int cceh_index_lookup(const cceh_index_t *index,
                      const void *key, uint32_t keylen,
                      void **value_out, uint32_t *valuelen_out);

/* ── 统计信息 ── */
uint32_t cceh_index_size(const cceh_index_t *index);
uint32_t cceh_index_global_depth(const cceh_index_t *index);
uint32_t cceh_index_directory_size(const cceh_index_t *index);
uint32_t cceh_index_segment_count(const cceh_index_t *index);
uint32_t cceh_index_thread_epoch_count(const cceh_index_t *index);
uint32_t cceh_index_segment_local_depth(const cceh_index_t *index, uint32_t directory_slot);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_CCEH_H */