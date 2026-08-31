/* percolator.c —— Percolator 两阶段提交（本地 2PC）实现
 *
 * 语义（Gap#1 写面，集成 mvcc_ts 版本链 + 预写锁）：
 *  - 写槽（pcol_write_slot_t）持有 key/value 的深拷贝与元信息，
 *    primary = 首个写入键（Task 7 真正锁表使用，本任务仅记录）。
 *  - prewrite：对每个写键检测
 *      写写冲突：以 read_ts=INT64_MAX 取"最新已提交版本"，若其 commit_ts > start_ts
 *                → PCOL_ERR_WRITE_CONFLICT（有一个晚于本事务开始的提交）；
 *      锁冲突  ：mvcc_ts 暴露的 ts_store_has_pending_write 查到"他事务 commit_ts==0
 *                的预写版本" → PCOL_ERR_LOCK_CONFLICT。
 *    无冲突则以 commit_ts=0 写预写版本（mvcc_ts 对 commit_ts==0 恒不可见，天然持锁）。
 *    任一键冲突：置回滚态并返回冲突错误码（先到先得：已成功预写的键保持，冲突者弃写）。
 *  - commit：把每个预写版本以 commit_ts 重新写一遍（ts_store_put(…, start_ts, commit_ts)），
 *    新增的 commit 版本 commit_ts>0 成为可见权威版本。幂等：committed 置位后二次提交
 *    返回 PCOL_ERR_ALREADY_COMMITTED。
 *  - rollback：commit_ts==0 恒不可见（读面天然弃写）；同时物理移除本事务的
 *    预写节点以收回锁，避免残留被后续 ts_store_has_pending_write 误判为他人锁。
 */
#include "distributed/percolator.h"
#include "distributed/tso.h"

#include <stdlib.h>
#include <string.h>

/* 写槽：prewrite/commit 的数据承载 */
typedef struct pcol_write_slot_s {
    uint8_t *key; size_t klen;
    uint8_t *value; size_t vlen;
    bool     is_delete;
    bool     is_primary;   /* 首个写入键 */
    bool     prewritten;   /* 是否已预写 */
} pcol_write_slot_t;

/* 事务结构 */
struct pcol_txn {
    ts_store_t    *store;    /* 版本链存储（调用方拥有，栈上） */
    tso_oracle_t  *oracle;   /* TSO（本任务不主动取值，记录以便后续扩展） */
    int64_t        start_ts;
    pcol_write_slot_t *writes;
    int            nwrites, cap;
    bool           any_prewritten;  /* 是否已发生预写 */
    bool           committed;       /* 已提交（置位后可判定幂等） */
    bool           rolled_back;     /* 已回滚（不再允许提交） */
    pcol_lock_table_t *lock_table;  /* 锁登记表（可选挂接；非空则 prewrite 登记本事务锁） */
};

/* ---------- 错误串 ---------- */

const char *pcol_error_string(int err) {
    switch (err) {
        case PCOL_OK:                   return "ok";
        case PCOL_ERR_WRITE_CONFLICT:   return "write-write conflict";
        case PCOL_ERR_LOCK_CONFLICT:    return "lock conflict";
        case PCOL_ERR_NOT_FOUND:        return "not found";
        case PCOL_ERR_ALREADY_COMMITTED:return "already committed";
        default:                        return "unknown";
    }
}

/* ---------- 生命周期 ---------- */

pcol_txn_t *pcol_txn_begin(ts_store_t *s, tso_oracle_t *o, int64_t start_ts) {
    pcol_txn_t *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->store   = s;
    t->oracle  = o;
    t->start_ts = start_ts;
    return t;
}

void pcol_txn_add_write(pcol_txn_t *t, const pcol_write_t *w) {
    if (!t || !w) return;
    if (t->nwrites == t->cap) {
        int ncap = t->cap ? t->cap * 2 : 4;
        pcol_write_slot_t *nw = realloc(t->writes, ncap * sizeof(*nw));
        if (!nw) return;
        t->writes = nw;
        t->cap = ncap;
    }
    pcol_write_slot_t *slot = &t->writes[t->nwrites];   /* 当前槽下标 */
    slot->klen = w->klen;
    slot->key  = malloc(w->klen ? w->klen : 1);
    if (slot->key && w->klen) memcpy(slot->key, w->key, w->klen);

    slot->vlen = w->vlen;
    slot->value = w->vlen ? malloc(w->vlen) : NULL;
    if (slot->value && w->vlen) memcpy(slot->value, w->value, w->vlen);

    slot->is_delete = w->is_delete;
    slot->is_primary = (t->nwrites == 0);   /* 首个写入键为 primary */
    slot->prewritten = false;
    t->nwrites++;
}

void pcol_txn_free(pcol_txn_t *t) {
    if (!t) return;
    for (int i = 0; i < t->nwrites; ++i) {
        free(t->writes[i].key);
        free(t->writes[i].value);
    }
    free(t->writes);
    free(t);
}

/* 挂接锁登记表：使 prewrite 把本事务锁登记进表（增强恢复语义） */
void pcol_txn_set_lock_table(pcol_txn_t *t, pcol_lock_table_t *lt) {
    if (t) t->lock_table = lt;
}

/* ---------- 冲突检测 ---------- */

/* 写写冲突：键存在"最新已提交版本"且其 commit_ts > start_ts。
 * ts_store_get(read_ts=INT64_MAX, active=NULL) 返回 commit_ts 最大的可见已提交版本；
 *   rc==0  得到非删已提交版本（含 commit_ts），用于比较；
 *   rc==-1 无已提交版本（键不存在或全未提交）：本事务为首写，无 WW 冲突；
 *   rc==-2 可见但已删除(tombstone)：最新已提交为一条删除。本地版本链对"删除后新写"
 *           天然正确（后续提交的 commit_ts 更大即更可见），本实现不判定为 WW 冲突。 */
static int ww_conflict(ts_store_t *store, const void *key, uint32_t klen,
                       int64_t start_ts) {
    ts_version_t cur;
    int rc = ts_store_get(store, key, klen, INT64_MAX, NULL, 0, &cur);
    if (rc != 0) return 0;                       /* 无已提交版本 或 已是删除标记 */
    int conflict = (cur.commit_ts > start_ts);
    ts_version_free(&cur);
    return conflict;
}

/* ---------- 两阶段 ---------- */

int pcol_prewrite(pcol_txn_t *t) {
    if (!t) return PCOL_ERR_NOT_FOUND;

    for (int i = 0; i < t->nwrites; ++i) {
        pcol_write_slot_t *slot = &t->writes[i];

        /* 写写冲突：有一个晚于本事务 start_ts 的已提交版本 */
        if (ww_conflict(t->store, slot->key, slot->klen, t->start_ts)) {
            pcol_rollback(t);
            return PCOL_ERR_WRITE_CONFLICT;
        }
        /* 锁冲突：存在他人未提交(commit_ts==0)预写版本（本事务自身的预写被 except 排除） */
        if (ts_store_has_pending_write(t->store, slot->key, slot->klen,
                                       t->start_ts)) {
            pcol_rollback(t);
            return PCOL_ERR_LOCK_CONFLICT;
        }

        /* 无冲突：写预写版本（commit_ts=0，mvcc_ts 对其恒不可见，实现持锁） */
        int rc = slot->is_delete
            ? ts_store_put_delete(t->store, slot->key, slot->klen,
                                  t->start_ts, 0)
            : ts_store_put(t->store, slot->key, slot->klen,
                           t->start_ts, 0, slot->value, slot->vlen);
        if (rc != 0) { pcol_rollback(t); return PCOL_ERR_NOT_FOUND; }
        slot->prewritten = true;
        t->any_prewritten = true;
        /* 登记本事务锁进登记表（尽力而为：登记失败仅失去恢复语义，不影响预写本身） */
        if (t->lock_table)
            pcol_lock_add(t->lock_table, slot->key, (uint32_t)slot->klen,
                          t->start_ts, slot->is_primary, pcol_now_ms());
    }
    return PCOL_OK;
}

int pcol_commit(pcol_txn_t *t, int64_t commit_ts) {
    if (!t) return PCOL_ERR_NOT_FOUND;
    if (t->committed) return PCOL_ERR_ALREADY_COMMITTED;  /* 幂等：二次提交拒绝 */
    if (t->rolled_back) return PCOL_ERR_NOT_FOUND;        /* 已回滚不可提交 */

    for (int i = 0; i < t->nwrites; ++i) {
        pcol_write_slot_t *slot = &t->writes[i];
        /* 以 commit_ts 重新写一份提交版本（commit_ts>0 即由 pick_best 选中成为可见权威） */
        if (slot->is_delete)
            ts_store_put_delete(t->store, slot->key, slot->klen,
                                t->start_ts, commit_ts);
        else
            ts_store_put(t->store, slot->key, slot->klen,
                         t->start_ts, commit_ts, slot->value, slot->vlen);
        /* 提交完成后释放本事务的预写锁（移除 commit_ts==0 残留，避免被误判他人锁） */
        ts_store_discard_pending(t->store, slot->key, slot->klen, t->start_ts);
    }
    t->committed = true;
    t->any_prewritten = false;   /* 预写已收敛为提交，锁随之失效 */
    /* 提交完成即清自己的锁，避免 recovery/GC 再见到本事务 */
    if (t->lock_table) pcol_lock_remove_start(t->lock_table, t->start_ts);
    return PCOL_OK;
}

int pcol_rollback(pcol_txn_t *t) {
    if (!t) return PCOL_ERR_NOT_FOUND;
    /* 释放本事务每个写键上的预写锁：物理移除 commit_ts==0 的预写节点，
     * 使后续事务不被本事务残留预写误判为锁冲突。读可见性本身不受影响
     * （commit_ts==0 恒不可见），删链仅为锁语义的收回。 */
    for (int i = 0; i < t->nwrites; ++i) {
        ts_store_discard_pending(t->store, t->writes[i].key,
                                 t->writes[i].klen, t->start_ts);
    }
    t->rolled_back = true;
    t->any_prewritten = false;
    /* 回滚完毕即清自己的锁，避免 recovery/GC 残留本事务 */
    if (t->lock_table) pcol_lock_remove_start(t->lock_table, t->start_ts);
    return PCOL_OK;
}