/* percolator_cross_node.c —— 跨分片 Percolator 协调器：分片路由 + 全有或全无提交
 *                              + 协调器意图日志 + 崩溃重放
 *
 * 定位（Gap#1 链路打通，Task 9）：在已有的本地 Percolator/SI（Task 5-8）之上，
 * 用"多 ts_store = 多分片/多副本"的模型做跨分片 2PC 协调，不引入共识层。
 *
 * 语义要点：
 *  1. 分片路由：pcol_shard_hash（FNV-1a mod shard_count）把写集按键分组到各分片。
 *  2. 全有或全无：先对所有涉及分片各自 prewrite；全部成功后，才用**同一个** commit_ts
 *     对所有分片 commit。mvcc 读面按 commit_ts 选可见版本，于是"要么各分片都可见、
 *     要么各分片都不可见"——这正是跨分片原子性的来源。任一分片 prewrite 冲突，
 *     对已 prewrite 的全部分片事务 rollback，绝不留下部分提交。
 *  3. 死锁防御：全有或全无的结构（要么全持锁、要么全释放）本身已能避免协调器自身
 *     制造死锁；wfg（Task 8）作为防御/可观测层保留——锁冲突时用
 *     ts_store_pending_holder 找出实际持有者，登记 wait-edge(本事务 → 持有者)，
 *     若 Tarjan 检出的环里含本事务，回报 PCOL_ERR_DEADLOCK。
 *  4. 崩溃重放：协调器把每次跨分片事务的意图（start_ts / commit_ts / 各分片键集 /
 *     primary 分片）记入意图日志；重启后 pcol_cross_recover 只在 primary 分片对
 *     primary 键裁定一次提交态，再据此对全部分片的全部键补提交或回滚。
 *     这是对"Raft/TSO 高可用"的可行替代：协调器日志重放。
 *
 * 键缓冲布局（意图日志内部约定，见 percolator.h）：
 *   shard_keys[shard] = [uint32_t klen][key bytes][uint32_t klen][key bytes]...
 */
#include "distributed/percolator.h"
#include "distributed/tso.h"

#include <stdlib.h>
#include <string.h>

/* ---------- 分片路由 ---------- */

int pcol_shard_hash(const void *key, size_t klen, int shard_count) {
    const unsigned char *p = (const unsigned char *)key;
    uint32_t h = 2166136261u;      /* FNV-1a 32 位 offset basis */
    size_t i;
    if (shard_count <= 0) return 0;
    for (i = 0; i < klen; ++i) {
        h ^= (uint32_t)p[i];
        h *= 16777619u;            /* FNV prime */
    }
    return (int)(h % (uint32_t)shard_count);   /* 无符号取模，结果恒非负 */
}

/* ---------- 协调上下文 ---------- */

pcol_context_t *pcol_context_new(ts_store_t **stores, int shard_count, tso_oracle_t *oracle) {
    pcol_context_t *ctx;
    if (!stores || shard_count <= 0) return NULL;
    ctx = (pcol_context_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    /* 只深拷贝指针数组本身（允许调用方传栈上数组）；store 本体仍由调用方拥有 */
    ctx->stores = (ts_store_t **)malloc((size_t)shard_count * sizeof(ts_store_t *));
    if (!ctx->stores) { free(ctx); return NULL; }
    memcpy(ctx->stores, stores, (size_t)shard_count * sizeof(ts_store_t *));
    ctx->shard_count = shard_count;
    ctx->oracle      = oracle;
    return ctx;
}

void pcol_context_free(pcol_context_t *ctx) {
    if (!ctx) return;
    free(ctx->stores);
    free(ctx);
}

/* ---------- 跨分片原子提交 ---------- */

/* 单个分片的写集分组（键/值均直接引用调用方 writes，pcol_txn_add_write 内部会深拷贝） */
typedef struct shard_group_s {
    const pcol_write_t **ws;   /* 指向原 writes 的指针数组 */
    int  n, cap;
    pcol_txn_t *txn;           /* 该分片的本地事务（prewrite 阶段建立） */
} shard_group_t;

static int group_push(shard_group_t *g, const pcol_write_t *w) {
    if (g->n == g->cap) {
        int ncap = g->cap ? g->cap * 2 : 4;
        const pcol_write_t **nw =
            (const pcol_write_t **)realloc(g->ws, (size_t)ncap * sizeof(*nw));
        if (!nw) return -1;
        g->ws  = nw;
        g->cap = ncap;
    }
    g->ws[g->n++] = w;
    return 0;
}

/* 回滚并释放全部已建立的分片事务，随后释放分组数组 */
static void groups_abort(shard_group_t *groups, int shard_count) {
    int i;
    for (i = 0; i < shard_count; ++i) {
        if (groups[i].txn) {
            pcol_rollback(groups[i].txn);   /* 幂等：prewrite 内部已回滚过也无副作用 */
            pcol_txn_free(groups[i].txn);
            groups[i].txn = NULL;
        }
        free(groups[i].ws);
    }
    free(groups);
}

/* 锁冲突时的死锁防御：为本分片的每个写键找出实际锁持有者，登记 wait-edge；
 * 若 wfg 检出的环里含本事务，返回 1（判为死锁），否则返回 0。 */
static int detect_deadlock(wfg_t *dl, ts_store_t *store, const shard_group_t *g,
                           int64_t start_ts) {
    int64_t *cycle;
    int cycle_n = 0, i, hit = 0;
    int64_t now = pcol_now_ms();

    if (!dl) return 0;
    wfg_add_txn(dl, start_ts, now);
    for (i = 0; i < g->n; ++i) {
        int64_t holder = 0;
        if (ts_store_pending_holder(store, g->ws[i]->key, (uint32_t)g->ws[i]->klen,
                                    start_ts, &holder) != 0)
            continue;                       /* 该键上并无他人锁 */
        if (holder == start_ts) continue;   /* 自锁不入图 */
        wfg_add_edge(dl, start_ts, holder, g->ws[i]->key, g->ws[i]->klen, now);
    }

    cycle = wfg_detect_cycles(dl, &cycle_n);
    if (cycle) {
        for (i = 0; i < cycle_n; ++i) {
            if (cycle[i] == start_ts) { hit = 1; break; }   /* 环里含本事务 */
        }
        free(cycle);
    }
    return hit;
}

/* 意图日志自动接线：全部分片 prewrite 成功后、commit-all 之前，按已分组好的
 * per-shard 写集构造一条 intent 并记入日志，使"崩溃重放"端到端可由库自身打通，
 * 无需调用方手工 pcol_intent_log_add。
 * 尽力而为：内存不足等记录失败只跳过（提交本身不依赖日志），不阻断正常提交。
 * 键缓冲布局与 pcol_cross_recover 的 intent_key_at 解析严格一致：
 *   每分片一块连续缓冲 [uint32_t klen][key bytes][uint32_t klen][key bytes]...
 * pcol_intent_log_add 内部深拷贝（intent_clone 逐块 malloc+memcpy），
 * 故本函数返回前即可释放全部临时缓冲，无生命周期要求。 */
static void record_commit_intent(const shard_group_t *groups, int shard_count,
                                 int primary_shard, int64_t start_ts, int64_t commit_ts) {
    pcol_intent_t it;
    uint8_t **keys = NULL;
    size_t   *lens = NULL;
    int      *ns   = NULL;
    int s, i;

    keys = (uint8_t **)calloc((size_t)shard_count, sizeof(*keys));
    lens = (size_t *)calloc((size_t)shard_count, sizeof(*lens));
    ns   = (int *)calloc((size_t)shard_count, sizeof(*ns));
    if (!keys || !lens || !ns) goto fail;

    for (s = 0; s < shard_count; ++s) {
        const shard_group_t *g = &groups[s];
        size_t total = 0, off = 0;
        ns[s] = g->n;
        if (g->n == 0) continue;              /* 空分片：键指针/长度保持 0，recover 侧跳过 */
        for (i = 0; i < g->n; ++i)
            total += sizeof(uint32_t) + g->ws[i]->klen;
        keys[s] = (uint8_t *)malloc(total ? total : 1);
        if (!keys[s]) goto fail;
        for (i = 0; i < g->n; ++i) {
            uint32_t kl = (uint32_t)g->ws[i]->klen;
            memcpy(keys[s] + off, &kl, sizeof(kl));   /* 长度前缀（本机字节序，与解析侧一致） */
            off += sizeof(kl);
            memcpy(keys[s] + off, g->ws[i]->key, kl);
            off += kl;
        }
        lens[s] = total;
    }

    memset(&it, 0, sizeof(it));
    it.start_ts       = start_ts;
    it.commit_ts      = commit_ts;
    it.shard_count    = shard_count;
    it.primary_shard  = primary_shard;
    it.shard_keys     = keys;
    it.shard_keys_len = lens;
    it.shard_keys_n   = ns;
    (void)pcol_intent_log_add(&it);   /* 深拷贝进静态日志；返回值忽略：记录失败不阻断提交 */

    for (s = 0; s < shard_count; ++s) free(keys[s]);
    free(keys); free(lens); free(ns);
    return;

fail:
    if (keys) { for (s = 0; s < shard_count; ++s) free(keys[s]); free(keys); }
    free(lens); free(ns);
}

int pcol_commit_cross(pcol_context_t *ctx, pcol_shard_fn shard_fn,
                      const pcol_write_t *writes, int nwrites,
                      int64_t start_ts, int64_t commit_ts,
                      wfg_t *dl) {
    shard_group_t *groups;
    int i, s, rc = PCOL_OK;

    if (!ctx || !ctx->stores || !writes || nwrites <= 0) return PCOL_ERR_NOT_FOUND;
    if (!shard_fn) shard_fn = pcol_shard_hash;

    groups = (shard_group_t *)calloc((size_t)ctx->shard_count, sizeof(*groups));
    if (!groups) return PCOL_ERR_NOT_FOUND;

    /* 1) 按分片路由分组（保持写集内的原始顺序，首个写键天然成为其分片的 primary） */
    for (i = 0; i < nwrites; ++i) {
        s = shard_fn(writes[i].key, writes[i].klen, ctx->shard_count);
        if (s < 0 || s >= ctx->shard_count) s = 0;
        if (group_push(&groups[s], &writes[i]) != 0) {
            groups_abort(groups, ctx->shard_count);
            return PCOL_ERR_NOT_FOUND;
        }
    }

    /* 2) prewrite-all：任一分片冲突即全体回滚（全有或全无的第一半） */
    for (s = 0; s < ctx->shard_count; ++s) {
        shard_group_t *g = &groups[s];
        if (g->n == 0) continue;

        g->txn = pcol_txn_begin(ctx->stores[s], ctx->oracle, start_ts);
        if (!g->txn) { groups_abort(groups, ctx->shard_count); return PCOL_ERR_NOT_FOUND; }
        for (i = 0; i < g->n; ++i) pcol_txn_add_write(g->txn, g->ws[i]);

        rc = pcol_prewrite(g->txn);
        if (rc != PCOL_OK) {
            /* 锁冲突时启用死锁防御：登记 wait-edge 并判环 */
            if (rc == PCOL_ERR_LOCK_CONFLICT &&
                detect_deadlock(dl, ctx->stores[s], g, start_ts))
                rc = PCOL_ERR_DEADLOCK;
            groups_abort(groups, ctx->shard_count);   /* 原子回滚：无部分提交 */
            return rc;
        }
    }

    /* 2.5) 意图日志自动接线：全部 prewrite 成功之后、commit-all 之前记录本事务意图。
     *      冲突回滚路径在上方直接 return，不到达此处，故失败事务不留意图。
     *      primary 分片 = 整体首个写键（writes[0]）所在分片；组内保序，该键也是
     *      其分片缓冲中的第一个键，与 intent 的 primary 约定（缓冲首键）自洽。 */
    int primary_shard = shard_fn(writes[0].key, writes[0].klen, ctx->shard_count);
    if (primary_shard < 0 || primary_shard >= ctx->shard_count) primary_shard = 0;
    record_commit_intent(groups, ctx->shard_count, primary_shard, start_ts, commit_ts);

    /* 3) commit-all：**primary 分片先提交**（Percolator 协议的提交点），随后才提交
     *    其余分片。secondary 不可能先于 primary 提交，于是 commit 窗口内的崩溃态只剩
     *    「primary 已提交 → pcol_cross_recover promote 全体」与
     *    「全体未提交   → pcol_cross_recover discard 全体」两支，
     *    不会出现"secondary 已提交而 primary 未提交"的不可恢复撕裂态（按下标顺序
     *    提交时，primary 落非 0 分片且中途崩溃即会产生该态）。
     *    全部分片共享同一 commit_ts，跨分片可见性同时翻转。
     *    本地实现中 prewrite 全部成功后 commit 不可能失败（commit_ts>0 即可见、
     *    无冲突检测），故忽略 pcol_commit 返回值。 */
    if (groups[primary_shard].txn) pcol_commit(groups[primary_shard].txn, commit_ts);
    for (s = 0; s < ctx->shard_count; ++s) {
        if (s != primary_shard && groups[s].txn) pcol_commit(groups[s].txn, commit_ts);
    }
    for (s = 0; s < ctx->shard_count; ++s) {
        if (groups[s].txn) pcol_txn_free(groups[s].txn);
        free(groups[s].ws);
    }
    free(groups);
    return PCOL_OK;
}

/* ---------- 协调器意图日志 ---------- */

/* 进程内意图日志：教学级实现，简单动态数组 + 深拷贝键缓冲。
 * 真实系统里这块应落盘（WAL），此处只保证"协调器重启后仍能重放"的语义骨架。 */
static pcol_intent_t *g_intents = NULL;
static int g_intent_n = 0, g_intent_cap = 0;

/* 释放一条 intent 的全部深拷贝内容（不释放 intent 结构本身） */
static void intent_release(pcol_intent_t *it) {
    int i;
    if (it->shard_keys) {
        for (i = 0; i < it->shard_count; ++i) free(it->shard_keys[i]);
        free(it->shard_keys);
    }
    free(it->shard_keys_len);
    free(it->shard_keys_n);
    memset(it, 0, sizeof(*it));
}

/* 把 src 的键缓冲全部深拷贝到 dst */
static int intent_clone(pcol_intent_t *dst, const pcol_intent_t *src) {
    int i;
    memset(dst, 0, sizeof(*dst));
    dst->start_ts      = src->start_ts;
    dst->commit_ts     = src->commit_ts;
    dst->shard_count   = src->shard_count;
    dst->primary_shard = src->primary_shard;
    if (src->shard_count <= 0) return 0;

    dst->shard_keys     = (uint8_t **)calloc((size_t)src->shard_count, sizeof(uint8_t *));
    dst->shard_keys_len = (size_t *)calloc((size_t)src->shard_count, sizeof(size_t));
    dst->shard_keys_n   = (int *)calloc((size_t)src->shard_count, sizeof(int));
    if (!dst->shard_keys || !dst->shard_keys_len || !dst->shard_keys_n) {
        intent_release(dst);
        return -1;
    }
    for (i = 0; i < src->shard_count; ++i) {
        size_t len = src->shard_keys_len ? src->shard_keys_len[i] : 0;
        dst->shard_keys_len[i] = len;
        dst->shard_keys_n[i]   = src->shard_keys_n ? src->shard_keys_n[i] : 0;
        if (!len || !src->shard_keys || !src->shard_keys[i]) continue;
        dst->shard_keys[i] = (uint8_t *)malloc(len);
        if (!dst->shard_keys[i]) { intent_release(dst); return -1; }
        memcpy(dst->shard_keys[i], src->shard_keys[i], len);
    }
    return 0;
}

int pcol_intent_log_add(const pcol_intent_t *it) {
    int i;
    if (!it) return -1;

    /* 同 start_ts 覆盖（协调器对同一事务的意图只保留最新一份） */
    for (i = 0; i < g_intent_n; ++i) {
        if (g_intents[i].start_ts == it->start_ts) {
            pcol_intent_t tmp;
            if (intent_clone(&tmp, it) != 0) return -1;
            intent_release(&g_intents[i]);
            g_intents[i] = tmp;
            return 0;
        }
    }

    if (g_intent_n == g_intent_cap) {
        int ncap = g_intent_cap ? g_intent_cap * 2 : 8;
        pcol_intent_t *ni =
            (pcol_intent_t *)realloc(g_intents, (size_t)ncap * sizeof(*ni));
        if (!ni) return -1;
        g_intents    = ni;
        g_intent_cap = ncap;
    }
    if (intent_clone(&g_intents[g_intent_n], it) != 0) return -1;
    g_intent_n++;
    return 0;
}

int pcol_intent_log_get(int64_t start_ts, const pcol_intent_t **out) {
    int i;
    for (i = 0; i < g_intent_n; ++i) {
        if (g_intents[i].start_ts != start_ts) continue;
        if (out) *out = &g_intents[i];
        return 0;
    }
    if (out) *out = NULL;
    return -1;
}

void pcol_intent_log_clear(void) {
    int i;
    for (i = 0; i < g_intent_n; ++i) intent_release(&g_intents[i]);
    free(g_intents);
    g_intents    = NULL;
    g_intent_n   = 0;
    g_intent_cap = 0;
}

/* ---------- 崩溃重放 ---------- */

/* 从键缓冲里取第 idx 个键（长度前缀布局）。成功返回 0，并把键指针/长度回填到出参
 * （指向缓冲内部，不拷贝）。缓冲损坏或越界返回 -1。 */
static int intent_key_at(const uint8_t *buf, size_t buflen, int idx,
                         const uint8_t **key, uint32_t *klen) {
    size_t off = 0;
    int i = 0;
    if (!buf) return -1;
    while (off + sizeof(uint32_t) <= buflen) {
        uint32_t n;
        memcpy(&n, buf + off, sizeof(n));
        off += sizeof(n);
        if (off + n > buflen) return -1;      /* 缓冲损坏 */
        if (i == idx) {
            *key  = buf + off;
            *klen = n;
            return 0;
        }
        off += n;
        i++;
    }
    return -1;
}

int pcol_cross_recover(pcol_context_t *ctx, int64_t start_ts) {
    const pcol_intent_t *it = NULL;
    const uint8_t *pkey = NULL;
    uint32_t plen = 0;
    ts_version_t out;
    int64_t commit_ts;
    int committed, s, i, shard_n;

    if (!ctx || !ctx->stores) return PCOL_OK;
    if (pcol_intent_log_get(start_ts, &it) != 0 || !it) return PCOL_OK;  /* 无意图：无事 */

    shard_n = it->shard_count < ctx->shard_count ? it->shard_count : ctx->shard_count;
    if (shard_n <= 0) return PCOL_OK;
    if (it->primary_shard < 0 || it->primary_shard >= shard_n) return PCOL_OK;

    /* 1) 只在 primary 分片对 primary 键（该分片缓冲的第一个键）裁定一次提交态 */
    if (intent_key_at(it->shard_keys ? it->shard_keys[it->primary_shard] : NULL,
                      it->shard_keys_len ? it->shard_keys_len[it->primary_shard] : 0,
                      0, &pkey, &plen) != 0)
        return PCOL_OK;

    memset(&out, 0, sizeof(out));
    committed = (ts_store_get_by_start(ctx->stores[it->primary_shard], pkey, plen,
                                       start_ts, &out) == 0);
    commit_ts = committed ? out.commit_ts : 0;
    if (committed) ts_version_free(&out);
    /* intent 里也记着 commit_ts；primary 链上的 commit_ts 为权威，缺失时退回 intent 值 */
    if (committed && commit_ts <= 0) commit_ts = it->commit_ts;

    /* 2) 依据同一裁定结果对**全部分片的全部键**统一重放（其他分片不再重复裁定） */
    for (s = 0; s < shard_n; ++s) {
        int n = it->shard_keys_n ? it->shard_keys_n[s] : 0;
        const uint8_t *buf = it->shard_keys ? it->shard_keys[s] : NULL;
        size_t buflen = it->shard_keys_len ? it->shard_keys_len[s] : 0;
        for (i = 0; i < n; ++i) {
            const uint8_t *k = NULL;
            uint32_t kl = 0;
            if (intent_key_at(buf, buflen, i, &k, &kl) != 0) break;
            if (committed)
                ts_store_promote(ctx->stores[s], k, kl, start_ts, commit_ts); /* 幂等，-1 忽略 */
            else
                ts_store_discard_pending(ctx->stores[s], k, kl, start_ts);
        }
    }
    return PCOL_OK;
}
