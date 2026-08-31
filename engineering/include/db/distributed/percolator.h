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
    PCOL_ERR_ALREADY_COMMITTED = -4 /* 重复提交（幂等拒绝） */
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

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_PERCOLATOR_H */