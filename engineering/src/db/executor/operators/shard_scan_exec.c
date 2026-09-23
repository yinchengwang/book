/* shard_scan_exec.c - Gap#4 shard 裁剪 + 扇出（重写 gap06 骨架） */
#include "db/executor/exec_shard.h"
#include "db/executor/exec_exchange.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int px_shard_prune(const shard_router_t *router, const vecx_pred_t *pred,
                   int *out_ids, int max) {
    if (!router || !out_ids || max <= 0) return 0;

    if (!pred) {
        /* 无分片键谓词：全分片扇出 */
        int total = shard_count(router);
        shard_info_t *all = (shard_info_t *)calloc((size_t)total, sizeof(shard_info_t));
        if (!all) return 0;
        int n = shard_get_all(router, all, total);
        int out = 0;
        for (int i = 0; i < n && out < max; i++) out_ids[out++] = all[i].shard_id;
        free(all);
        return out;
    }

    int64_t key = pred->i64;
    switch (pred->op) {
        case CMP_EQ: {
            int id = shard_route(router, &key, sizeof(key));
            if (id < 0) return 0;
            /* 域校验：RANGE 路由的 shard_route 对不命中任何区间的键回退
             * shard 0（sharding.c，gap06 与写路径共享，不可改），直接信任
             * 会把域外等值键错扫到 shard 0。用 shard_route_range([key,key])
             * 复判：RANGE 域外 → 0 命中；HASH 对任意键返回全分片（域内，
             * 放行）。 */
            int probe[1];
            if (shard_route_range(router, &key, &key, probe, 1) == 0)
                return 0;   /* 域外等值键：无候选分片 */
            out_ids[0] = id;
            return 1;
        }
        case CMP_LT:  /* (-inf, key) */
        case CMP_LE: {
            int64_t lo = INT64_MIN;
            int64_t hi = (pred->op == CMP_LT) ? key - 1 : key;
            return shard_route_range(router, &lo, &hi, out_ids, max);
        }
        case CMP_GT:  /* (key, +inf) */
        case CMP_GE: {
            int64_t lo = (pred->op == CMP_GT) ? key + 1 : key;
            int64_t hi = INT64_MAX;
            return shard_route_range(router, &lo, &hi, out_ids, max);
        }
        default:
            /* NE 等无法裁剪：全分片 */
            return px_shard_prune(router, NULL, out_ids, max);
    }
}

/* ---- 扇出 ---- */

typedef struct {
    shard_router_t *router;
    vecx_pred_t pred;             /* 拷贝；has_pred=0 表示全分片 */
    int has_pred;
    px_shard_scan_fn open_shard;
    void *ctx;
    int shard_ids[256];
    int nshards;
    int next_idx;                 /* 工厂调用序号（open 内同步，无竞争） */
} ShardFanoutCtx;

static ExecNode *shard_subtree_fn(void *vctx) {
    ShardFanoutCtx *c = (ShardFanoutCtx *)vctx;
    if (c->next_idx >= c->nshards) return NULL;
    int shard_id = c->shard_ids[c->next_idx++];
    return c->open_shard(shard_id, c->ctx);
}

static void shard_fanout_ctx_destroy(void *vctx) {
    free(vctx);
}

ExecNode *exec_create_shard_fanout(shard_router_t *router, const vecx_pred_t *pred,
                                   px_shard_scan_fn open_shard, void *ctx) {
    if (!router || !open_shard) return NULL;

    ShardFanoutCtx *c = (ShardFanoutCtx *)calloc(1, sizeof(ShardFanoutCtx));
    if (!c) return NULL;
    c->router = router;
    c->open_shard = open_shard;
    c->ctx = ctx;
    if (pred) { c->pred = *pred; c->has_pred = 1; }

    c->nshards = px_shard_prune(router, pred, c->shard_ids, 256);
    if (c->nshards <= 0) { free(c); return NULL; }

    /* dop = 候选分片数；ctx_destroy 释放本闭包（Task 4 的 _ex 语义）。
     * exchange 创建失败时不会调 ctx_destroy，此处自行回收避免泄漏。 */
    ExecNode *ex = exec_create_exchange_ex(shard_subtree_fn, c, c->nshards,
                                           shard_fanout_ctx_destroy);
    if (!ex) free(c);
    return ex;
}
