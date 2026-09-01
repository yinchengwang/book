/* mvcc_ts.c —— 时间戳版本链快照存储（SI 隔离读取面）实现
 *
 * 数据结构设计（计划题建议，自洽化如下）：
 *  - 键目录：一个按 key 字节序升序排列的动态数组 ts_store_entry[]。
 *    插入用 lower_bound 二分定位 + memmove 线性搬移，仍保证升序，
 *    从而 scan 可按确定字典序惰性推进（不用哈希，避免破坏序）。
 *  - 版本链：每个键一条以 ts_version_t 为节点的单向链表。
 *    本次实现刻意采用"简单头插"，未维护 commit_ts 降序；
 *    读取时通过遍历整链挑选"commit_ts<=read_ts 且可见且 commit_ts 最大"的
 *    版本。这样插入顺序与读取正确性解耦（牺牲一点遍历性能换取最简单正确性），
 *    与计划中"可简化"选项一致。
 *  - 可见性：候选 = commit_ts>0 && commit_ts<=read_ts && start_ts 不在 active 集。
 *    commit_ts==0 的 prewrite 未提交版本天然被 commit_ts>0 过滤掉（符合"get 过滤未提交"）。
 *  - 深拷贝：value 与 key 在 put/返回副本时均深拷贝；ts_version_free 负责释放返回副本。
 */
#include "distributed/mvcc_ts.h"

#include <stdlib.h>
#include <string.h>

/* ---------- 内部类型 ---------- */

/* 键目录条目 */
typedef struct ts_store_entry {
    char         *key;       /* 目录键深拷贝，用于排序与比对 */
    uint32_t      klen;
    ts_version_t *versions;  /* 该键的版本链头（简单头插，序无关） */
} ts_store_entry;

/* ts_store_t 结构体（含 void* entries/n/cap）已在头文件 mvcc_ts.h 中定义，
 * 其中 entries 字段在实现中以 ts_store_entry* 使用（需做 void*→指针转型）。 */

/* ---------- 工具函数 ---------- */

/* 按字节序比较两个键：先比公共前缀，再比长度 */
static int key_cmp(const void *a, uint32_t alen, const void *b, uint32_t blen) {
    size_t m = alen < blen ? alen : blen;
    int c = m ? memcmp(a, b, m) : 0;
    if (c != 0) return c;
    return (alen > blen) - (alen < blen);
}

/* 深拷贝一段字节串（用于键/目录键） */
static char *str_dup_bytes(const void *p, uint32_t len) {
    char *buf = malloc(len ? len : 1);
    if (!buf) return NULL;
    if (len) memcpy(buf, p, len);
    return buf;
}

/* lower_bound：返回首个 key>=给定 key 的下标；若下标处恰好等于给定键则 *found=1。
 * 既是插入点（未命中）也是查询点（命中），保证升序稳定。 */
static size_t lower_bound(ts_store_t *s, const void *key, uint32_t klen, int *found) {
    size_t lo = 0, hi = s->n;
    ts_store_entry *entries = (ts_store_entry *)s->entries;
    while (lo < hi) {
        size_t mid = (lo + hi) >> 1;
        if (key_cmp(entries[mid].key, entries[mid].klen, key, klen) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    *found = (lo < s->n) &&
             key_cmp(entries[lo].key, entries[lo].klen, key, klen) == 0;
    return lo;
}

/* start_ts 是否在 active 隐藏集中 */
static int in_active(const int64_t *active, size_t n, int64_t ts) {
    for (size_t i = 0; i < n; i++)
        if (active[i] == ts) return 1;
    return 0;
}

/* 在键目录 pos 位置插入一个新条目（调用方保证该位置为合法插入点） */
static int insert_entry(ts_store_t *s, size_t pos, const void *key, uint32_t klen) {
    char *kcopy = str_dup_bytes(key, klen);
    if (!kcopy) return -1;
    ts_store_entry *entries = (ts_store_entry *)s->entries;
    if (s->n == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 8;
        ts_store_entry *ne = realloc(s->entries, ncap * sizeof(*ne));
        if (!ne) { free(kcopy); return -1; }
        s->entries = ne;
        s->cap = ncap;
        entries = ne;
    }
    if (pos < s->n)
        memmove(&entries[pos + 1], &entries[pos],
                (s->n - pos) * sizeof(ts_store_entry));
    entries[pos].key = kcopy;
    entries[pos].klen = klen;
    entries[pos].versions = NULL;
    s->n++;
    return 0;
}

/* 在链中挑选"commit_ts 最大"的可见候选版本；无可见候选则返回 NULL */
static ts_version_t *pick_best(ts_version_t *v, int64_t read_ts,
                               const int64_t *active, size_t active_n) {
    ts_version_t *best = NULL;
    for (; v; v = v->next) {
        if (v->commit_ts <= 0) continue;        /* 未提交/非法，不可见 */
        if (v->commit_ts > read_ts) continue;   /* 晚于快照，不可见 */
        if (in_active(active, active_n, v->start_ts)) continue; /* 被事务隐藏 */
        if (!best || v->commit_ts > best->commit_ts) best = v;
    }
    return best;
}

/* 深拷贝一个可见版本到 out（调用方负责 ts_version_free） */
static int copy_version(const ts_version_t *src, ts_version_t *out) {
    char *k = NULL;
    if (src->klen) {
        k = str_dup_bytes(src->key, src->klen);
        if (!k) return -1;
    }
    void *val = NULL;
    if (src->value_len) {
        val = malloc(src->value_len);
        if (!val) { free(k); return -1; }
        memcpy(val, src->value, src->value_len);
    }
    out->start_ts  = src->start_ts;
    out->commit_ts = src->commit_ts;
    out->deleted   = src->deleted;
    out->key       = k;
    out->klen      = src->klen;
    out->value     = val;
    out->value_len = src->value_len;
    out->next      = NULL;
    return 0;
}

/* ---------- 生命周期 ---------- */

void ts_store_init(ts_store_t *s) {
    if (s) memset(s, 0, sizeof(*s));
}

void ts_store_destroy(ts_store_t *s) {
    if (!s) return;
    ts_store_entry *entries = (ts_store_entry *)s->entries;
    for (size_t i = 0; i < s->n; i++) {
        ts_store_entry *e = &entries[i];
        free(e->key);
        ts_version_t *v = e->versions;
        while (v) {
            ts_version_t *nx = v->next;
            free(v->key);
            free(v->value);
            free(v);
            v = nx;
        }
    }
    free(s->entries);
    memset(s, 0, sizeof(*s));
}

void ts_version_free(ts_version_t *v) {
    if (!v) return;
    free(v->key);
    free(v->value);
}

/* ---------- 写入 ---------- */

int ts_store_put(ts_store_t *s, const void *key, uint32_t klen,
                 int64_t start_ts, int64_t commit_ts,
                 const void *value, size_t value_len) {
    if (!s || !key) return -1;

    int found = 0;
    size_t pos = lower_bound(s, key, klen, &found);
    if (!found) {
        if (insert_entry(s, pos, key, klen) != 0) return -1;
    }
    ts_store_entry *e = &((ts_store_entry *)s->entries)[pos];

    /* 深拷贝键与值 */
    char *kcopy = str_dup_bytes(key, klen);
    if (!kcopy) return -1;
    void *vcopy = NULL;
    if (value && value_len) {
        vcopy = malloc(value_len);
        if (!vcopy) { free(kcopy); return -1; }
        memcpy(vcopy, value, value_len);
    }

    ts_version_t *v = malloc(sizeof(*v));
    if (!v) { free(kcopy); free(vcopy); return -1; }
    v->start_ts  = start_ts;
    v->commit_ts = commit_ts;
    v->deleted   = 0;
    v->key       = kcopy;
    v->klen      = klen;
    v->value     = vcopy;
    v->value_len = value_len;
    v->next      = e->versions;   /* 简单头插；序无关，读取时选最大 commit_ts */
    e->versions  = v;
    return 0;
}

int ts_store_put_delete(ts_store_t *s, const void *key, uint32_t klen,
                        int64_t start_ts, int64_t commit_ts) {
    if (!s || !key) return -1;

    int found = 0;
    size_t pos = lower_bound(s, key, klen, &found);
    if (!found) {
        if (insert_entry(s, pos, key, klen) != 0) return -1;
    }
    ts_store_entry *e = &((ts_store_entry *)s->entries)[pos];

    char *kcopy = str_dup_bytes(key, klen);
    if (!kcopy) return -1;

    ts_version_t *v = malloc(sizeof(*v));
    if (!v) { free(kcopy); return -1; }
    v->start_ts  = start_ts;
    v->commit_ts = commit_ts;
    v->deleted   = 1;
    v->key       = kcopy;
    v->klen      = klen;
    v->value     = NULL;
    v->value_len = 0;
    v->next      = e->versions;
    e->versions  = v;
    return 0;
}

/* ---------- 读取 ---------- */

int ts_store_get(ts_store_t *s, const void *key, uint32_t klen,
                 int64_t read_ts, const int64_t *active, size_t active_n,
                 ts_version_t *out) {
    if (!s || !key || !out) return -1;

    int found = 0;
    size_t pos = lower_bound(s, key, klen, &found);
    if (!found) return -1;

    ts_version_t *best = pick_best(((ts_store_entry *)s->entries)[pos].versions, read_ts, active, active_n);
    if (!best) return -1;                 /* 无可见版本（不存在/全隐藏/未提交） */
    if (best->deleted) return -2;         /* 可见但已删除 */

    return copy_version(best, out) == 0 ? 0 : -1;
}

/* 待提交写检查（Percolator 判锁 helper）：见头文件声明语义 */
int ts_store_has_pending_write(ts_store_t *s, const void *key, uint32_t klen,
                               int64_t except_start_ts) {
    if (!s || !key) return 0;

    int found = 0;
    size_t pos = lower_bound(s, key, klen, &found);
    if (!found) return 0;   /* 键不存在：无任何未提交写 */

    for (ts_version_t *v = ((ts_store_entry *)s->entries)[pos].versions; v; v = v->next) {
        if (v->commit_ts == 0 && v->start_ts != except_start_ts)
            return 1;       /* 存在他人未提交(prewrite)版本 → 锁冲突 */
    }
    return 0;
}

/* 锁持有者查询：与 has_pending_write 同一遍历，命中时回填持有者 start_ts */
int ts_store_pending_holder(ts_store_t *s, const void *key, uint32_t klen,
                            int64_t except_start_ts, int64_t *holder) {
    if (!s || !key) return -1;

    int found = 0;
    size_t pos = lower_bound(s, key, klen, &found);
    if (!found) return -1;   /* 键不存在：无任何预写锁 */

    for (ts_version_t *v = ((ts_store_entry *)s->entries)[pos].versions; v; v = v->next) {
        if (v->commit_ts == 0 && v->start_ts != except_start_ts) {
            if (holder) *holder = v->start_ts;   /* 他人正持本键的预写锁 */
            return 0;
        }
    }
    return -1;   /* 仅有已提交版本，或预写者恰为 except 本人 */
}

/* 释放预写锁：物理移除键链上"commit_ts==0 且 start_ts 匹配"的预写节点 */
void ts_store_discard_pending(ts_store_t *s, const void *key, uint32_t klen,
                              int64_t start_ts) {
    if (!s || !key) return;

    int found = 0;
    size_t pos = lower_bound(s, key, klen, &found);
    if (!found) return;

    ts_store_entry *e = &((ts_store_entry *)s->entries)[pos];
    ts_version_t **pp = &e->versions;
    while (*pp) {
        ts_version_t *v = *pp;
        if (v->commit_ts == 0 && v->start_ts == start_ts) {
            *pp = v->next;                  /* 摘链并释放该预写节点 */
            free(v->key);
            free(v->value);
            free(v);
        } else {
            pp = &v->next;
        }
    }
}

/* InDoubt 提权：把"commit_ts==0 且 start_ts 匹配"的预写节点原地置为已提交 */
int ts_store_promote(ts_store_t *s, const void *key, uint32_t klen,
                     int64_t start_ts, int64_t commit_ts) {
    if (!s || !key) return -1;

    int found = 0;
    size_t pos = lower_bound(s, key, klen, &found);
    if (!found) return -1;   /* 键不存在：无待提权节点 */

    for (ts_version_t *v = ((ts_store_entry *)s->entries)[pos].versions; v; v = v->next) {
        if (v->commit_ts == 0 && v->start_ts == start_ts) {
            v->commit_ts = commit_ts;   /* value 保留在链内，仅提权 */
            return 0;
        }
    }
    return -1;   /* 链尽未中：节点已提交 / 无匹配 start_ts */
}

/* 按 start_ts 裁定键的提交态；命中已提交则深拷贝填 out */
int ts_store_get_by_start(ts_store_t *s, const void *key, uint32_t klen,
                          int64_t start_ts, ts_version_t *out) {
    if (!s || !key || !out) return -1;

    int found = 0;
    size_t pos = lower_bound(s, key, klen, &found);
    if (!found) return -1;   /* 无任何 start_ts 匹配的节点 */

    for (ts_version_t *v = ((ts_store_entry *)s->entries)[pos].versions; v; v = v->next) {
        if (v->start_ts != start_ts) continue;
        if (v->commit_ts > 0)          /* 已提交：深拷贝填 out */
            return copy_version(v, out) == 0 ? 0 : -1;
        return 1;                      /* 仍为预写（commit_ts==0，未提交） */
    }
    return -1;   /* 链内无匹配 start_ts */
}

/* ---------- 扫描 ---------- */

void ts_store_scan(ts_store_t *s, const void *start, uint32_t start_klen,
                   int64_t read_ts, const int64_t *active, size_t active_n,
                   ts_iter_t *it) {
    if (it) {
        it->store    = s;
        it->read_ts  = read_ts;
        it->active   = active;
        it->active_n = active_n;
        it->next     = 0;
        if (start && s) {
            int f = 0;
            it->next = lower_bound(s, start, start_klen, &f);   /* start 含 */
        }
    }
}

int ts_iter_next(ts_iter_t *it, ts_version_t *out) {
    if (!it || !it->store || !out) return 1;
    while (it->next < it->store->n) {
        ts_store_entry *e = &((ts_store_entry *)it->store->entries)[it->next];
        it->next++;
        ts_version_t *best = pick_best(e->versions, it->read_ts, it->active, it->active_n);
        if (!best || best->deleted) continue;   /* 不可见或已删：跳过该键 */
        return copy_version(best, out) == 0 ? 0 : 1;
    }
    return 1;   /* 结束 */
}