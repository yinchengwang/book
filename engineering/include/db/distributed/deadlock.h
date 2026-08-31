/* deadlock.h —— 跨分片 Waits-For 死锁检测:节点=事务(start_ts),有向边=waiter 等待 holder
 * 释放锁;Tarjan SCC 找强连通环=死锁候选;按最早等待选 victim 打破死锁。 */
#ifndef DB_DISTRIBUTED_DEADLOCK_H
#define DB_DISTRIBUTED_DEADLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wfg_edge_s {
    int64_t waiter; int64_t holder;
    int64_t create_ms;                 /* 边生成时刻(用于超期剪枝) */
    const uint8_t *lock_key;           /* 引发等待的锁键(仅记录,不参与环判定) */
    size_t         lock_klen;
    struct wfg_edge_s *next;
} wfg_edge_t;

typedef struct wfg_txn_s {
    int64_t start_ts;
    int64_t wait_start_ms;             /* 该事务开始等待的时刻(选 victim 用,越小等待越久) */
} wfg_txn_t;

typedef struct wfg wfg_t;              /* 不透明,实现内含 txn 数组 + 边链 */

wfg_t *wfg_new(void);                  /* 失败返 NULL */
void   wfg_free(wfg_t *g);

/* 登记事务节点(重复 start_ts 幂等更新 wait_start_ms)。返回 0,分配失败 -1 */
int wfg_add_txn(wfg_t *g, int64_t start_ts, int64_t wait_start_ms);

/* 添加等待边:waiter 等待 holder,now_ms 为边生成时刻。lock_key/klen 仅记录。
 * 返回 0,分配失败 -1;waiter/holder 未登记时自动登记 */
int wfg_add_edge(wfg_t *g, int64_t waiter, int64_t holder,
                 const uint8_t *lock_key, size_t klen, int64_t now_ms);

/* Tarjan SCC 检测:返回发现的"环事务"数组(malloc),*out_n 为其个数;
 * 无环返回 NULL 且 *out_n=0。可多次调用(不破坏图)。返回数组含每个非平凡 SCC
 * (size>=2,或含自环)的全部成员。调用方 free。 */
int64_t *wfg_detect_cycles(wfg_t *g, int *out_n);

/* 剪除 create_ms < now_ms - edge_ttl_ms 的陈旧边(返回 0) */
int wfg_prune_edges_before(wfg_t *g, int64_t now_ms, int64_t edge_ttl_ms);

/* 在给定环(一个 SCC 的成员数组)内选 wait_start_ms 最小者(最早等待=等待最久)为 victim。
 * 保证 victim 必在传入 cycle 内,多环并存时不误跨环。n==0 返回 0。 */
int64_t wfg_pick_victim_scc(wfg_t *g, const int64_t *cycle, int n);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_DEADLOCK_H */