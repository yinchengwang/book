/* mvcc_ts.h —— 分布式 MVCC 时间戳版本链快照存储（SI 隔离读取面） */
#ifndef DB_DISTRIBUTED_MVCC_TS_H
#define DB_DISTRIBUTED_MVCC_TS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 版本节点：既作为键内版本链的链内节点，也作为对外返回(for get/scan)的副本载体。
 * get/scan 填写的 out 副本中 next 恒为 NULL，key/value 均为深拷贝，
 * 调用方用完后须调用 ts_version_free 释放。 */
typedef struct ts_version {
    int64_t  start_ts;   /* 该版本所属事务的 begin_ts */
    int64_t  commit_ts;  /* 0 表示未提交(prewrite)；>0 表示已提交 */
    int      deleted;    /* 1 = 该版本为删除标记(tombstone)，无 value */
    char    *key;        /* 键深拷贝（get/scan 返回时调用方负责 ts_version_free） */
    uint32_t klen;
    void    *value;      /* 值深拷贝；tombstone 或无值时为 NULL */
    size_t   value_len;
    struct ts_version *next; /* 键内版本链下一节点（较旧版本）；对外副本为 NULL */
} ts_version_t;

/* 存储句柄：结构体公开以便栈上分配；内部 entries 为不透明(ts_store_entry*)
 * 排序动态数组（布局私有，见实现）。直接 memset 复位即可，无需构造。 */
typedef struct ts_store {
    void   *entries;   /* 内部：按 key 升序的动态数组 ts_store_entry* */
    size_t  n;         /* 生效条目数 */
    size_t  cap;       /* 容量 */
} ts_store_t;

/* 生命周期：对象内存由调用方提供，init 清零，destroy 释放内部资源并把对象复位 */
void ts_store_init(ts_store_t *s);
void ts_store_destroy(ts_store_t *s);

/* 写入：为键追加一个版本。commit_ts 由调用方给出，0 表示未提交(prewrite)。
 * value/value_len 为待写入值；value 为空(NULL 且 len==0)时写入"无值"版本。
 * 返回 0 成功，非 0 失败（如内存不足）。 */
int ts_store_put(ts_store_t *s, const void *key, uint32_t klen,
                 int64_t start_ts, int64_t commit_ts,
                 const void *value, size_t value_len);

/* 写入删除标记：为键追加一个 tombstone 版本（无 value）。 */
int ts_store_put_delete(ts_store_t *s, const void *key, uint32_t klen,
                        int64_t start_ts, int64_t commit_ts);

/* 快照读取（Percolator 2PC 的 read 面）：
 *   沿版本链选择"commit_ts>0 且 commit_ts<=read_ts 且 start_ts 不在 active 集"
 *   的可见版本中 commit_ts 最大的版本作为可见候选。
 * 返回：
 *   0  = 命中可见非删版本（填 out，调用方须 ts_version_free）
 *  -1  = 当前快照下无可见版本（键不存在，或全部为未提交/被 active 隐藏）
 *  -2  = 快照下可见，但该键迄今已被删除(tombstone)
 * active 为需隐藏的事务 start_ts 集合（可为 NULL/active_n=0，表示不隐藏任何事务）。 */
int ts_store_get(ts_store_t *s, const void *key, uint32_t klen,
                 int64_t read_ts, const int64_t *active, size_t active_n,
                 ts_version_t *out);

/* 释放由 ts_store_get / ts_iter_next 得到的版本副本（释放其 key 与 value 深拷贝）。
 * 注意：只释放副本内容，不释放 out 对象本身（为其调用方栈/堆上的载体）。 */
void ts_version_free(ts_version_t *v);

/* ---------- 游标扫描（按 key 字节序升序） ---------- */
typedef struct ts_iter {
    ts_store_t    *store;
    int64_t        read_ts;
    const int64_t *active;
    size_t         active_n;
    size_t         next;   /* 下一个待考察的目录下标 */
} ts_iter_t;

/* 从 start 键（含）起按 key 字节序升序遍历；start 为 NULL 时从首个键起。
 * 可见性规则与 ts_store_get 一致。 */
void ts_store_scan(ts_store_t *s, const void *start, uint32_t start_klen,
                   int64_t read_ts, const int64_t *active, size_t active_n,
                   ts_iter_t *it);

/* 推进游标：返回 0 = 得到下一可见非删版本（填 out，调用方 ts_version_free）；
 *             返回 1 = 扫描结束（无可得分发）。
 * 不可见(未提交/被 active 隐藏)与已删除(tombstone)的键会被跳过。 */
int ts_iter_next(ts_iter_t *it, ts_version_t *out);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_MVCC_TS_H */