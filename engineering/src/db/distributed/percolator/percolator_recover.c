/* percolator_recover.c —— 锁登记表 + 崖壁恢复(InDoubt 决策) + 超期锁 GC
 *
 * 语义（Percolator 恢复面，集成 mvcc_ts 提权/裁定原语）：
 *  - 锁登记表独立于 pcol_txn 存活：客户端崩溃后句柄被 free，恢复只能借表找到
 *    死事务的键集合。prewrite 时登记、commit/rollback/recover 后移除。
 *  - pcol_recover_txn：对单事务做 InDoubt 裁定——primary 已提交（ts_store_get_by_start
 *    ==0）则对其余锁键用 primary 的 commit_ts 逐个 ts_store_promote 补提交；
 *    primary 未提交/无则对整个事务 ts_store_discard_pending 回滚。无论何分支均移除锁。
 *  - pcol_gc_stale_locks：扫描登记表，任一条目过期（expire_ms < now_ms）即按同上
 *    规则处理该事务，循环扫描直到某轮无过期事务。
 */
#include "distributed/percolator.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 可注入绝对毫秒时钟源（NULL = 用默认系统时钟） */
static int64_t (*g_now_ms_source)(void) = NULL;

int64_t pcol_now_ms(void) {
    if (g_now_ms_source) return g_now_ms_source();
    return (int64_t)(((double)clock() * 1000.0) / CLOCKS_PER_SEC);
}

void pcol_set_now_ms_source(int64_t (*src)(void)) {
    g_now_ms_source = src;
}

int pcol_lock_expire_ms_get(void) {
    return PERCOLATOR_LOCK_TTL_MS;
}

/* ---------- 锁登记表 ---------- */

int pcol_lock_table_init(pcol_lock_table_t *lt) {
    if (!lt) return -1;
    memset(lt, 0, sizeof(*lt));
    return 0;
}

void pcol_lock_table_destroy(pcol_lock_table_t *lt) {
    if (!lt) return;
    pcol_lock_entry_t *e = lt->head;
    while (e) {
        pcol_lock_entry_t *nx = e->next;
        free(e->key);
        free(e);
        e = nx;
    }
    memset(lt, 0, sizeof(*lt));
}

int pcol_lock_add(pcol_lock_table_t *lt, const uint8_t *key, uint32_t klen,
                  int64_t start_ts, bool is_primary, int64_t now_ms) {
    if (!lt || !key) return -1;

    pcol_lock_entry_t *e = calloc(1, sizeof(*e));
    if (!e) return -1;
    e->key = malloc(klen ? klen : 1);
    if (!e->key) { free(e); return -1; }
    if (klen) memcpy(e->key, key, klen);

    e->klen       = klen;
    e->start_ts   = start_ts;
    e->is_primary = is_primary;
    e->expire_ms  = now_ms + pcol_lock_expire_ms_get();
    e->next       = lt->head;   /* 无锁头插入 */
    lt->head      = e;
    lt->n++;
    return 0;
}

void pcol_lock_remove_start(pcol_lock_table_t *lt, int64_t start_ts) {
    if (!lt) return;
    pcol_lock_entry_t **pp = &lt->head;
    while (*pp) {
        pcol_lock_entry_t *e = *pp;
        if (e->start_ts == start_ts) {
            *pp = e->next;
            free(e->key);
            free(e);
            lt->n--;
        } else {
            pp = &e->next;
        }
    }
}

/* ---------- 共享判定/动作 helper ---------- */

/* 判断锁表中是否存在指定 start_ts 的任一条目 */
static int lock_has_start(const pcol_lock_table_t *lt, int64_t start_ts) {
    for (const pcol_lock_entry_t *e = lt->head; e; e = e->next)
        if (e->start_ts == start_ts) return 1;
    return 0;
}

/* 判断指定事务（start_ts）是否任一条目已过期 */
static int txn_expired(const pcol_lock_table_t *lt, int64_t start_ts, int64_t now_ms) {
    for (const pcol_lock_entry_t *e = lt->head; e; e = e->next)
        if (e->start_ts == start_ts && e->expire_ms < now_ms) return 1;
    return 0;
}

/* 裁定并处理单个死事务：primary 键从登记表条目取（无 primary 条目按未提交回滚）。
 * 依据 primary 提交态补提交/回滚，最后移除该事务全部锁条目。 */
static int act_txn(ts_store_t *s, pcol_lock_table_t *lt, int64_t start_ts,
                   int *committed, int *rollbacked) {
    if (committed)  *committed = 0;
    if (rollbacked) *rollbacked = 0;
    if (!s || !lt) return 0;

    /* 定位 primary 条目以取 primary 键；无 primary 条目时按未提交回滚 */
    pcol_lock_entry_t *primary_e = NULL;
    for (pcol_lock_entry_t *e = lt->head; e; e = e->next) {
        if (e->start_ts != start_ts) continue;
        if (e->is_primary) { primary_e = e; break; }
    }
    if (!primary_e) {
        pcol_lock_remove_start(lt, start_ts);
        if (rollbacked) *rollbacked = 1;
        return 0;
    }

    ts_version_t out;
    memset(&out, 0, sizeof(out));
    int rc = ts_store_get_by_start(s, primary_e->key, primary_e->klen,
                                   start_ts, &out);
    if (rc == 0) {
        /* primary 已提交 → 用其 commit_ts 对同事务其余锁键补提交（提权，幂等） */
        int64_t commit_ts = out.commit_ts;
        ts_version_free(&out);
        for (pcol_lock_entry_t *e = lt->head; e; ) {
            pcol_lock_entry_t *nx = e->next;
            if (e->start_ts == start_ts && !e->is_primary)
                ts_store_promote(s, e->key, e->klen, start_ts, commit_ts);
            e = nx;
        }
        if (committed) *committed = 1;
    } else {
        /* primary 未提交（含无 start_ts 匹配）→ 对整个事务全部锁键回滚 */
        for (pcol_lock_entry_t *e = lt->head; e; ) {
            pcol_lock_entry_t *nx = e->next;
            if (e->start_ts == start_ts)
                ts_store_discard_pending(s, e->key, e->klen, start_ts);
            e = nx;
        }
        if (rollbacked) *rollbacked = 1;
    }

    pcol_lock_remove_start(lt, start_ts);
    return 0;
}

/* ---------- InDoubt 决策 ---------- */

int pcol_recover_txn(ts_store_t *s, pcol_lock_table_t *lt,
                     const uint8_t *primary, uint32_t plen, int64_t start_ts,
                     int *committed, int *rollbacked) {
    (void)primary;       /* primary 键由登记表 primary 条目提供，无需额外传入 */
    (void)plen;
    if (committed)  *committed = 0;
    if (rollbacked) *rollbacked = 0;
    if (!lt) return 0;
    if (!lock_has_start(lt, start_ts)) return 0;  /* 无该事务任何条目：无操作 */
    return act_txn(s, lt, start_ts, committed, rollbacked);
}

/* ---------- 超期锁 GC ---------- */

int pcol_gc_stale_locks(ts_store_t *s, pcol_lock_table_t *lt, int64_t now_ms,
                        int *n_committed, int *n_rollbacked) {
    if (n_committed)  *n_committed = 0;
    if (n_rollbacked) *n_rollbacked = 0;
    if (!s || !lt) return 0;

    int64_t *processed = NULL;   /* 已处理的 start_ts 集，防重复 */
    size_t nproc = 0, cap = 0;
    int changed = 1;
    while (changed) {
        changed = 0;
        for (pcol_lock_entry_t *e = lt->head; e; e = e->next) {
            int64_t start_ts = e->start_ts;
            if (!txn_expired(lt, start_ts, now_ms)) continue;

            size_t k;
            for (k = 0; k < nproc && processed[k] != start_ts; k++) {}
            if (k < nproc) continue;   /* 该事务已处理过 */

            int c = 0, r = 0;
            act_txn(s, lt, start_ts, &c, &r);
            if (c && n_committed)  (*n_committed)++;
            if (r && n_rollbacked) (*n_rollbacked)++;

            if (nproc == cap) {
                size_t ncap = cap ? cap * 2 : 8;
                int64_t *np = realloc(processed, ncap * sizeof(*np));
                if (!np) { free(processed); return 0; }
                processed = np;
                cap = ncap;
            }
            processed[nproc++] = start_ts;
            changed = 1;                 /* 表已变，重扫一轮 */
            break;
        }
    }
    free(processed);
    return 0;
}