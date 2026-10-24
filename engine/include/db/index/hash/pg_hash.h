/**
 * @file include/db/index/hash/pg_hash.h
 * @brief PostgreSQL 风格 Hash 索引公共头文件
 */
#ifndef DB_INDEX_PG_HASH_H
#define DB_INDEX_PG_HASH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PG_HASH_DEFAULT_FILL_FACTOR   75
#define PG_HASH_MIN_FILL_FACTOR       10
#define PG_HASH_MAX_FILL_FACTOR       100
#define PG_HASH_PAGE_MAX_ITEMS        9

/* ── 前向声明 ── */
typedef struct pg_hash pg_hash_t;

/* ── 生命周期 ── */
pg_hash_t *pg_hash_create(uint32_t fill_factor);
void pg_hash_drop(pg_hash_t *index);

/* ── 核心操作 ── */
int pg_hash_insert(pg_hash_t *index,
                   const void *key,   uint32_t keylen,
                   const void *value, uint32_t valuelen);
int pg_hash_delete(pg_hash_t *index,
                   const void *key, uint32_t keylen);
int pg_hash_lookup(const pg_hash_t *index,
                   const void *key,        uint32_t keylen,
                   void **value_out,       uint32_t *valuelen_out);

/* ── 统计信息 ── */
uint32_t pg_hash_size(const pg_hash_t *index);
uint32_t pg_hash_bucket_count(const pg_hash_t *index);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_PG_HASH_H */