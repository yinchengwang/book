/* tso.h —— 全局时间戳服务（编解码 + 单调 Oracle） */
#ifndef DB_DISTRIBUTED_TSO_H
#define DB_DISTRIBUTED_TSO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TSO 编码：物理时间左移 18 位 + 逻辑计数（Spanner/Cockroach 风格） */
#define TSO_LOGICAL_BITS       18
#define TSO_LOGICAL_MASK       ((1 << TSO_LOGICAL_BITS) - 1)   /* 262143 */
#define TSO_BATCH_SIZE         100
#define TSO_CACHE_SIZE         1000
#define TSO_CACHE_REFILL_WMARK 100

typedef struct { int64_t physical_ms; uint32_t logical; } tso_ts_t;

int64_t tso_encode(const tso_ts_t *ts);
void tso_decode(int64_t raw, tso_ts_t *out);

typedef struct tso_oracle tso_oracle_t;

int  tso_oracle_init(tso_oracle_t **out);
void tso_oracle_destroy(tso_oracle_t *o);
int  tso_alloc(tso_oracle_t *o, int count, int64_t *out_start, int64_t *out_end);
int64_t tso_oracle_now(tso_oracle_t *o);

/* 可注入物理时钟源（测试） */
typedef int (*tso_clock_source_t)(int64_t *ms);
void tso_set_clock_source(tso_clock_source_t src);

/* 客户端批量缓存 + HLC 单调推进 */
typedef struct tso_client tso_client_t;

typedef int (*tso_timestamp_backend_t)(void *ctx, int count, int64_t *s, int64_t *e);

tso_client_t *tso_client_new(int cache_size);
void tso_client_destroy(tso_client_t *c);
int64_t tso_client_get(tso_client_t *c);
void tso_client_set_backend(tso_client_t *c, tso_timestamp_backend_t backend, void *ctx);

#ifdef __cplusplus
}
#endif
#endif /* DB_DISTRIBUTED_TSO_H */