/* tso_client.c —— 客户端批量缓存 + HLC 单调推进 */
#include "distributed/tso.h"
#include <stdlib.h>

struct tso_client {
    int cache_size;        /* 缓存批次大小 */
    int64_t next_ts;       /* 缓存区间游标 */
    int64_t max_ts;        /* 缓存区间终点 */
    int remaining;         /* 剩余可发戳数 */
    tso_timestamp_backend_t backend;   /* 注入的后端（测试） */
    void *backend_ctx;
};

/* 默认后端：直接交给真 Oracle 分配（单调由 Oracle 保证） */
static int default_backend(void *ctx, int count, int64_t *s, int64_t *e) {
    return tso_alloc((tso_oracle_t *)ctx, count, s, e);
}

tso_client_t *tso_client_new(int cache_size) {
    tso_client_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->cache_size = cache_size > 0 ? cache_size : TSO_CACHE_SIZE;
    c->remaining = 0;
    c->backend = NULL;
    c->backend_ctx = NULL;
    return c;
}

void tso_client_destroy(tso_client_t *c) {
    if (c) free(c);
}

void tso_client_set_backend(tso_client_t *c, tso_timestamp_backend_t backend, void *ctx) {
    if (!c) return;
    c->backend = backend;
    c->backend_ctx = ctx;
}

/* 批量取戳填充缓存区间 */
static int refill(tso_client_t *c) {
    tso_timestamp_backend_t b = c->backend ? c->backend : default_backend;
    void *ctx = c->backend ? c->backend_ctx : NULL;
    /* 无可用后端（默认后端无 Oracle ctx）时无法供给 */
    if (!b) return -1;
    int got = c->cache_size;
    if (b(ctx, got, &c->next_ts, &c->max_ts) != 0) return -1;
    c->remaining = (int)(c->max_ts - c->next_ts + 1);
    return 0;
}

/* HLC：向缓存区间顺序取一个戳。物理回退不倒退由底层 Oracle 保证，这里确保取出为单调递增 */
int64_t tso_client_get(tso_client_t *c) {
    if (!c) return 0;
    if (c->remaining <= 0) {
        if (refill(c) != 0) return 0;
    }
    int64_t ts = c->next_ts;
    if (ts > c->max_ts) {   /* 防御：游标越界则重灌 */
        if (refill(c) != 0) return 0;
        ts = c->next_ts;
    }
    c->next_ts = ts + 1;
    c->remaining--;
    return ts;
}