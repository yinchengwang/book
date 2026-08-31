/* deadlock.c —— 跨分片 Waits-For 图 + Tarjan SCC 死锁环检测。
 * 节点 = 事务(start_ts),有向边 = waiter 等待 holder 释放某锁;
 * Tarjan SCC 找强连通环作为死锁候选;按最早等待选 victim 打破死锁。
 * 独立模块,仅依赖标准 C,不依赖 ts_store/percolator。 */
#include "distributed/deadlock.h"

#include <stdlib.h>
#include <string.h>

struct wfg {
    wfg_txn_t     *txns;      /* 事务数组 */
    size_t         n, cap;
    wfg_edge_t    *edges;     /* 等待边链表 */
};

wfg_t *wfg_new(void) {
    return (wfg_t *)calloc(1, sizeof(wfg_t));
}

void wfg_free(wfg_t *g) {
    wfg_edge_t *e;
    if (!g) return;
    while (g->edges) { e = g->edges; g->edges = e->next; free(e); }
    free(g->txns);
    free(g);
}

static wfg_txn_t *find_txn(wfg_t *g, int64_t start_ts) {
    size_t i;
    for (i = 0; i < g->n; i++) {
        if (g->txns[i].start_ts == start_ts) return &g->txns[i];
    }
    return NULL;
}

static int ensure_cap(wfg_t *g) {
    wfg_txn_t *nt;
    size_t ncap;
    if (g->n < g->cap) return 0;
    ncap = g->cap ? g->cap * 2 : 16;
    nt = (wfg_txn_t *)realloc(g->txns, ncap * sizeof *g->txns);
    if (!nt) return -1;
    g->txns = nt;
    g->cap  = ncap;
    return 0;
}

int wfg_add_txn(wfg_t *g, int64_t start_ts, int64_t wait_start_ms) {
    wfg_txn_t *t;
    if (!g) return -1;
    t = find_txn(g, start_ts);
    if (t) { t->wait_start_ms = wait_start_ms; return 0; }   /* 幂等更新 */
    if (ensure_cap(g) != 0) return -1;
    g->txns[g->n].start_ts      = start_ts;
    g->txns[g->n].wait_start_ms = wait_start_ms;
    g->n++;
    return 0;
}

int wfg_add_edge(wfg_t *g, int64_t waiter, int64_t holder,
                 const uint8_t *lock_key, size_t klen, int64_t now_ms) {
    wfg_edge_t *e;
    if (!g) return -1;
    /* waiter/holder 未登记时自动登记(等待时刻取 now_ms 简化) */
    if (wfg_add_txn(g, waiter, now_ms) != 0) return -1;
    if (wfg_add_txn(g, holder, now_ms) != 0) return -1;
    e = (wfg_edge_t *)malloc(sizeof *e);
    if (!e) return -1;
    e->waiter    = waiter;
    e->holder    = holder;
    e->create_ms = now_ms;
    e->lock_key  = lock_key;
    e->lock_klen = klen;
    e->next      = g->edges;
    g->edges     = e;
    return 0;
}

/* ---------- Tarjan SCC ---------- */

/* 邻接表:实际就是 edges 中以 waiter 为出点的边,动态构造。 */
typedef struct tarjan_ctx_s {
    int      *index;      /* DFS 访问序,0=未访问 */
    int      *lowlink;
    int      *on_stack;   /* 是否在 SCC 栈上 */
    int64_t  *stack;      /* SCC 栈 */
    size_t    top;
    int       counter;
    int64_t  *out;        /* 收集到的环成员 */
    size_t    used, cap;
} tarjan_ctx_t;

static void dfs_visit(wfg_t *g, size_t v, tarjan_ctx_t *c) {
    wfg_edge_t *e;
    int has_self_loop = 0;
    int64_t v_ts = g->txns[v].start_ts;
    int scc_size;

    c->counter++;
    c->index[v] = c->lowlink[v] = c->counter;
    c->stack[c->top++] = v_ts;
    c->on_stack[v] = 1;

    for (e = g->edges; e; e = e->next) {
        if (e->waiter != v_ts) continue;
        if (e->holder == v_ts) { has_self_loop = 1; }
        {
            size_t w = (size_t)(find_txn(g, e->holder) - g->txns);
            if (c->index[w] == 0) {
                dfs_visit(g, w, c);
                if (c->lowlink[w] < c->lowlink[v]) c->lowlink[v] = c->lowlink[w];
            } else if (c->on_stack[w]) {
                if (c->index[w] < c->lowlink[v]) c->lowlink[v] = c->index[w];
            }
        }
    }

    /* 强连通根:弹出一个 SCC。栈内自 v_ts 位置(p-1)到栈顶即该 SCC 成员 */
    if (c->lowlink[v] == c->index[v]) {
        size_t p = c->top;
        while (p > 0 && c->stack[p - 1] != v_ts) p--;   /* p-1 即 v_ts 栈下标 */
        {
            size_t beg = p - 1;
            scc_size = (int)(c->top - beg);

            if (scc_size >= 2 || has_self_loop) {
                size_t i;
                for (i = beg; i < c->top; i++) {
                    int64_t ts = c->stack[i];
                    size_t j;
                    int present = 0;
                    for (j = 0; j < c->used; j++) if (c->out[j] == ts) { present = 1; break; }
                    if (!present) {
                        if (c->used >= c->cap) {
                            int64_t *nn = (int64_t *)realloc(c->out, c->cap * 2 * sizeof *c->out);
                            if (nn) { c->out = nn; c->cap *= 2; }
                        }
                        c->out[c->used++] = ts;
                    }
                }
            }
            /* 弹出该 SCC */
            while (c->top > beg) {
                c->top--;
                c->on_stack[(size_t)(find_txn(g, c->stack[c->top]) - g->txns)] = 0;
            }
        }
    }
}

int64_t *wfg_detect_cycles(wfg_t *g, int *out_n) {
    tarjan_ctx_t ctx;
    size_t i;
    int64_t *ret;
    if (out_n) *out_n = 0;
    if (!g || g->n == 0) return NULL;

    memset(&ctx, 0, sizeof ctx);
    ctx.index   = (int *)calloc(g->n, sizeof *ctx.index);
    ctx.lowlink = (int *)calloc(g->n, sizeof *ctx.lowlink);
    ctx.on_stack= (int *)calloc(g->n, sizeof *ctx.on_stack);
    ctx.stack   = (int64_t *)malloc(g->n * sizeof *ctx.stack);
    ctx.cap     = g->n;
    ctx.out     = (int64_t *)malloc((g->n + 1) * sizeof *ctx.out);
    if (!ctx.index || !ctx.lowlink || !ctx.on_stack || !ctx.stack || !ctx.out) {
        free(ctx.index); free(ctx.lowlink); free(ctx.on_stack); free(ctx.stack); free(ctx.out);
        return NULL;
    }

    for (i = 0; i < g->n; i++) {
        if (ctx.index[i] == 0) dfs_visit(g, i, &ctx);
    }

    if (ctx.used == 0) {
        free(ctx.index); free(ctx.lowlink); free(ctx.on_stack); free(ctx.stack); free(ctx.out);
        return NULL;
    }
    ret = (int64_t *)malloc(ctx.used * sizeof *ret);
    if (ret) {
        memcpy(ret, ctx.out, ctx.used * sizeof *ret);
        if (out_n) *out_n = (int)ctx.used;
    }
    free(ctx.index); free(ctx.lowlink); free(ctx.on_stack); free(ctx.stack); free(ctx.out);
    return ret;
}

int wfg_prune_edges_before(wfg_t *g, int64_t now_ms, int64_t edge_ttl_ms) {
    wfg_edge_t **pp;
    int64_t cutoff = now_ms - edge_ttl_ms;
    if (!g) return 0;
    pp = &g->edges;
    while (*pp) {
        if ((*pp)->create_ms < cutoff) {
            wfg_edge_t *dead = *pp;
            *pp = dead->next;
            free(dead);
        } else {
            pp = &(*pp)->next;
        }
    }
    return 0;
}

int64_t wfg_pick_victim_scc(wfg_t *g, const int64_t *cycle, int n) {
    int i;
    int64_t victim = 0;
    int64_t best = 0;
    int set = 0;
    if (!g || n <= 0 || !cycle) return 0;
    for (i = 0; i < n; i++) {
        wfg_txn_t *t = find_txn(g, cycle[i]);
        if (!t) continue;
        if (!set || t->wait_start_ms < best) { best = t->wait_start_ms; victim = cycle[i]; set = 1; }
    }
    return victim;
}