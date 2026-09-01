/* percolator.h —— 分布式 Percolator 两阶段提交（2PC）头文件
 *
 * Gap#1：分布式事务写面。在 mvcc_ts 版本链之上实现本地 2PC：
 *   1. prewrite：对每个写键做冲突检测（写写冲突 / 锁冲突），
 *      无冲突则以 commit_ts=0 写入不可见的预写版本并持锁；
 *   2. commit：把预写版本以 commit_ts 提升为已提交版本（幂等）；
 *   3. rollback：本地语义下预写版本恒不可见，原子地弃写即可。
 * 真正的锁表/崖壁恢复留待后续任务（Task 7）引入。
 */
#ifndef DB_DISTRIBUTED_PERCOLATOR_H
#define DB_DISTRIBUTED_PERCOLATOR_H

#include "distributed/deadlock.h"
#include "distributed/mvcc_ts.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 锁统一 TTL（毫秒）：本任务记录语义，Task 7 用于过期检测 */
#define PERCOLATOR_LOCK_TTL_MS 10000

/* Percolator 错误码 */
typedef enum {
    PCOL_OK = 0,
    PCOL_ERR_WRITE_CONFLICT  = -1,  /* 写写冲突：键的最新已提交版本 commit_ts > start_ts */
    PCOL_ERR_LOCK_CONFLICT   = -2,  /* 锁冲突：存在他人未提交的预写版本 */
    PCOL_ERR_NOT_FOUND       = -3,  /* 事务/对象缺失 */
    PCOL_ERR_ALREADY_COMMITTED = -4,/* 重复提交（幂等拒绝） */
    PCOL_ERR_DEADLOCK        = -5   /* 跨分片死锁：wfg 中检测到含本事务的等待环 */
} pcol_error_t;

/* 单条写请求：key/value 由调用方持有，add_write 内部深拷贝 */
typedef struct pcol_write_s {
    const uint8_t *key;
    size_t         klen;
    const uint8_t *value;
    size_t         vlen;
    bool           is_delete;   /* true = 删除标记写 */
} pcol_write_t;

/* 不透明事务句柄 */
typedef struct pcol_txn pcol_txn_t;
typedef struct tso_oracle tso_oracle_t;

/* 开启事务：绑定版本链存储与 TSO oracle，记录本次事务 start_ts */
pcol_txn_t *pcol_txn_begin(ts_store_t *s, tso_oracle_t *o, int64_t start_ts);

/* 追加一条写请求（深拷贝 key/value，首个写键为 primary） */
void pcol_txn_add_write(pcol_txn_t *t, const pcol_write_t *w);

/* 释放事务（含内部写槽深拷贝） */
void pcol_txn_free(pcol_txn_t *t);

/* 预写：对全部写进行冲突检测，无冲突则写入 commit_ts=0 的预写版本并持锁。
 * 返回 PCOL_OK 或冲突错误码（PCOL_ERR_WRITE_CONFLICT / PCOL_ERR_LOCK_CONFLICT）。 */
int pcol_prewrite(pcol_txn_t *t);

/* 提交：把每个预写版本以 commit_ts 提升为已提交版本。
 * 幂等：事务已提交后再次调用返回 PCOL_ERR_ALREADY_COMMITTED 而非重复写。 */
int pcol_commit(pcol_txn_t *t, int64_t commit_ts);

/* 回滚：本地语义下预写版本恒不可见，置回滚标记并清空预写意向即可。 */
int pcol_rollback(pcol_txn_t *t);

/* 错误码转可读字符串（中文注释，字符串为英文便于误报排查），未知返回 "unknown" */
const char *pcol_error_string(int err);

/* ---------- Lock（InDoubt 登记表）与宕机恢复 ----------
 * 锁登记表独立于 pcol_txn 存活（客户端崩溃后句柄被 free，恢复须借表找死事务的键集）。
 * 锁条目记录：所属事务 start_ts、absolute 过期毫秒、键、是否 primary。 */
typedef struct pcol_lock_entry_s {
    int64_t   start_ts;
    int64_t   expire_ms;
    uint8_t  *key;                /* 键深拷贝 */
    uint32_t  klen;
    bool      is_primary;
    struct pcol_lock_entry_s *next;
} pcol_lock_entry_t;

typedef struct pcol_lock_table {
    pcol_lock_entry_t *head;      /* 无锁头插入，教学级 */
    size_t n;
} pcol_lock_table_t;

/* 生命周期：调用方栈上提供对象，init 清零，destroy 释放全部 deep-copy 键并把对象复位 */
int  pcol_lock_table_init(pcol_lock_table_t *lt);
void pcol_lock_table_destroy(pcol_lock_table_t *lt);

/* 登记一把锁：深拷贝 key；expire_ms = now_ms + pcol_lock_expire_ms_get()。
 * 返回 0 成功，-1 参数非法/内存不足。 */
int  pcol_lock_add(pcol_lock_table_t *lt, const uint8_t *key, uint32_t klen,
                   int64_t start_ts, bool is_primary, int64_t now_ms);

/* 移除指定事务 start_ts 的全部锁条目（commit/rollback/recover 后调用，释放 deep-copy 键） */
void pcol_lock_remove_start(pcol_lock_table_t *lt, int64_t start_ts);

int  pcol_lock_expire_ms_get(void);  /* 返回 PERCOLATOR_LOCK_TTL_MS(10000) */

/* 当前绝对毫秒（可注入，测试用）。实现提供默认系统时钟，并暴露
 * `void pcol_set_now_ms_source(int64_t (*)(void));` 供测试注入。 */
int64_t pcol_now_ms(void);
void pcol_set_now_ms_source(int64_t (*src)(void));

/* 使 prewrite 把本事务锁登记进表（可选挂接；不挂接则仅本地2PC、无恢复语义） */
void pcol_txn_set_lock_table(pcol_txn_t *t, pcol_lock_table_t *lt);

/* InDoubt 决策：依据 primary 已提交态裁定死事务。
 *  primary 已提交(ts_store_get_by_start==0)→ 对同事务其余锁键用 primary 的 commit_ts
 *    逐个 ts_store_promote（补提交，幂等）；
 *  否则 → 对同事务每个锁键 ts_store_discard_pending(start_ts)（回滚）。
 * 无论何种分支，最后移除该事务全部锁条目。
 * 要点：裁定只看 primary；primary 的 commit_ts 取 ts_store_get_by_start 返回的 out->commit_ts。
 * 返回 0 成功（不一定代表事务被提交——要看 committed 与 rollbacked 两个出参）；
 * 若 lt 中无该 start_ts 的任何条目，直接返回 0 且两个计数为 0（无操作）。
 * 计数语义：committed 出参置 1 = 裁定为已提交且补提交完成；rollbacked 置 1 = 裁定回滚完成。 */
int pcol_recover_txn(ts_store_t *s, pcol_lock_table_t *lt,
                     const uint8_t *primary, uint32_t plen, int64_t start_ts,
                     int *committed, int *rollbacked);

/* 超期锁 GC：遍历登记表，逐事务判定是否任一条目 expire_ms < now_ms；
 *  是 → primary 已提交? 留锁定条目并对其余锁键补提交 : 对该事务全部锁键回滚；
 *       处理完移除该事务全部条目（见 `n_committed`/`n_rollbacked` 计数）。
 * 整个 txn 若要被清，只需任一条目过期即触发（简单实现，不细究部分过期）。
 * 返回 0 成功；计数出参分别累计裁定为提交/回滚的事务个数（可为 NULL）。 */
int pcol_gc_stale_locks(ts_store_t *s, pcol_lock_table_t *lt, int64_t now_ms,
                        int *n_committed, int *n_rollbacked);

/* ---------- 跨分片协调（Task 9：链路打通） ----------
 * 把一个事务的写集按分片路由函数分组到多个独立 ts_store（每分片 = 一个副本/节点），
 * 以"prewrite-all → 共享同一 commit_ts 的 commit-all"实现跨分片原子提交：
 * 任一分片 prewrite 冲突即全体回滚，绝不产生部分提交。 */

/* 分片路由：键 → 分片下标（返回值须落在 [0, shard_count) 内） */
typedef int (*pcol_shard_fn)(const void *key, size_t klen, int shard_count);

/* 跨分片协调上下文：一组分片 store + 共享 TSO。
 * stores 数组由调用方提供（可为栈上数组），内部只深拷贝指针数组本身。 */
typedef struct pcol_context_s {
    ts_store_t  **stores;      /* 每分片一个 ts_store（store 本体由调用方拥有） */
    int           shard_count;
    tso_oracle_t *oracle;      /* 共享时间源 */
} pcol_context_t;

pcol_context_t *pcol_context_new(ts_store_t **stores, int shard_count, tso_oracle_t *oracle);
void            pcol_context_free(pcol_context_t *ctx);

/* 默认分片路由：FNV-1a 32 位哈希后 mod shard_count（结果非负） */
int pcol_shard_hash(const void *key, size_t klen, int shard_count);

/* 跨分片原子提交：writes 按 shard_fn 分组，prewrite-all → 共享 commit_ts 的 commit-all；
 * 任一 prewrite 冲突 → 全体 rollback 并返回冲突码（原子，无部分提交）。
 * 参数 dl 可选：非 NULL 时，锁冲突会把 wait-edge(start_ts → 实际持有者) 记入该 wfg，
 * 并在检测到含本事务的等待环时返回 PCOL_ERR_DEADLOCK（死锁防御）；NULL 则不启用。
 * 返回 PCOL_OK / PCOL_ERR_WRITE_CONFLICT / PCOL_ERR_LOCK_CONFLICT / PCOL_ERR_DEADLOCK。 */
int pcol_commit_cross(pcol_context_t *ctx, pcol_shard_fn shard_fn,
                      const pcol_write_t *writes, int nwrites,
                      int64_t start_ts, int64_t commit_ts,
                      wfg_t *dl);

/* 协调器意图日志条目（跨分片事务意图，供崩溃重放）。
 * 键存放采用扁平约定：shard_keys[shard] 指向一块连续缓冲，缓冲内以
 *   [uint32_t klen][key 字节] 顺序拼接该分片的全部键；
 * shard_keys_len[shard] = 该缓冲的总字节数，shard_keys_n[shard] = 键个数。
 * primary 键约定为 shard_keys[primary_shard] 缓冲中的第一个键。 */
typedef struct pcol_intent_s {
    int64_t   start_ts, commit_ts;
    int       shard_count;
    int      *shard_keys_n;      /* [shard_count] 每分片键数 */
    uint8_t **shard_keys;        /* [shard_count] 每分片键缓冲（长度前缀拼接） */
    size_t   *shard_keys_len;    /* [shard_count] 每分片键缓冲总字节数 */
    int       primary_shard;
} pcol_intent_t;

/* 记录一条 intent（深拷贝全部键缓冲；同 start_ts 覆盖）。返回 0 成功，-1 失败。 */
int pcol_intent_log_add(const pcol_intent_t *it);

/* 查询某 start_ts 的 intent；*out 为日志内常驻引用（只读，勿 free）。无则返回 -1。 */
int pcol_intent_log_get(int64_t start_ts, const pcol_intent_t **out);

/* 清空意图日志（释放全部深拷贝；测试与进程收尾用） */
void pcol_intent_log_clear(void);

/* 崩溃重放：取 start_ts 的 intent，在 primary 分片对 primary 键判一次提交态——
 *  已提交 → 用其 commit_ts 对每个分片每个键 ts_store_promote（补提交，幂等）；
 *  未提交 → 对每个分片每个键 ts_store_discard_pending（回滚）。
 * 无该 intent 时视为无事可做。返回 PCOL_OK(0)。 */
int pcol_cross_recover(pcol_context_t *ctx, int64_t start_ts);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_PERCOLATOR_H */
