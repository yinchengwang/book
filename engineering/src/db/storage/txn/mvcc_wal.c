/**
 * @file mvcc_wal.c
 * @brief MVCC 与 WAL 集成实现
 *
 * 参考 PostgreSQL: src/backend/access/transam/xlog.c
 */

#include "db/storage/txn/mvcc_wal.h"
#include "db/storage/txn/mvcc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/* ========================================================================
 * 全局状态
 * ======================================================================== */

/** 全局 CSN 计数器 */
uint64_t g_commit_seqno = 0;

/** CSN 到事务 ID 映射表（简化实现） */
#define CSN_TABLE_SIZE 8192
static XactCommitSeqno g_csn_table[CSN_TABLE_SIZE];
static int g_csn_table_count = 0;

/** 全局恢复上下文 */
static MvccRecoveryCtx *g_recovery_ctx = NULL;

/* ========================================================================
 * 初始化
 * ======================================================================== */

/**
 * @brief 初始化 MVCC WAL 集成
 */
void mvcc_wal_init(void)
{
    g_commit_seqno = 0;
    g_csn_table_count = 0;
    memset(g_csn_table, 0, sizeof(g_csn_table));
}

/**
 * @brief 关闭 MVCC WAL 集成
 */
void mvcc_wal_shutdown(void)
{
    if (g_recovery_ctx) {
        mvcc_recovery_cleanup(g_recovery_ctx);
        g_recovery_ctx = NULL;
    }
}

/* ========================================================================
 * CSN 管理
 * ======================================================================== */

/**
 * @brief 分配新的提交序列号
 */
uint64_t mvcc_alloc_csn(void)
{
    return atomic_fetch_add(&g_commit_seqno, 1) + 1;
}

/**
 * @brief 注册事务的提交序列号
 *
 * 事务提交时调用，记录 xid -> csn 映射
 */
static void register_csn(TransactionId xid, uint64_t csn)
{
    if (g_csn_table_count >= CSN_TABLE_SIZE) {
        /* 表满，需要清理旧的记录（简化：清空） */
        g_csn_table_count = 0;
    }

    g_csn_table[g_csn_table_count].xid = xid;
    g_csn_table[g_csn_table_count].csn = csn;
    g_csn_table[g_csn_table_count].commit_time = 0;  /* TODO: 获取当前时间 */
    g_csn_table_count++;
}

/**
 * @brief 获取 CSN 对应的事务 ID
 */
TransactionId mvcc_csn_to_xid(uint64_t csn)
{
    for (int i = 0; i < g_csn_table_count; i++) {
        if (g_csn_table[i].csn == csn) {
            return g_csn_table[i].xid;
        }
    }
    return INVALID_TRANSACTION_ID;
}

/**
 * @brief 检查 CSN 是否已提交
 */
bool mvcc_csn_committed(uint64_t csn)
{
    return csn > 0 && csn <= g_commit_seqno;
}

/**
 * @brief 获取元组的提交序列号
 */
uint64_t mvcc_get_tuple_csn(TransactionId xid)
{
    for (int i = 0; i < g_csn_table_count; i++) {
        if (g_csn_table[i].xid == xid) {
            return g_csn_table[i].csn;
        }
    }
    return 0;  /* 未提交或未知 */
}

/* ========================================================================
 * MVCC WAL 写入
 * ======================================================================== */

/**
 * @brief 构建 MVCC WAL 头
 */
static void build_mvcc_header(MvccWalHeader *header,
                             TransactionId xmin,
                             TransactionId xmax,
                             uint32_t ctid,
                             heap_xmin_status_t xmin_status,
                             heap_xmax_status_t xmax_status)
{
    header->xmin = xmin;
    header->xmax = xmax;
    header->t_ctid = ctid;
    header->xmin_status = (uint8_t)xmin_status;
    header->xmax_status = (uint8_t)xmax_status;
    header->info = 0;
}

/**
 * @brief 写入 MVCC 堆表插入 WAL
 */
uint64_t mvcc_wal_heap_insert(wal_t *wal,
                              uint32_t txn_id,
                              uint32_t relfilenode,
                              uint32_t blocknum,
                              uint32_t offset,
                              TransactionId xmin,
                              const void *data,
                              size_t data_len)
{
    (void)wal;
    (void)txn_id;
    (void)relfilenode;
    (void)blocknum;
    (void)offset;
    (void)xmin;
    (void)data;
    (void)data_len;

    /* TODO: 实现实际的 WAL 写入 */
    /* 1. 构建 MVCC WAL 头
     * 2. 追加 MVCC WAL 头 + 数据到 WAL 缓冲区
     * 3. 返回 LSN
     */

    return 0;
}

/**
 * @brief 写入 MVCC 堆表更新 WAL
 */
uint64_t mvcc_wal_heap_update(wal_t *wal,
                             uint32_t txn_id,
                             uint32_t relfilenode,
                             uint32_t blocknum,
                             uint32_t offset,
                             uint32_t new_blocknum,
                             uint32_t new_offset,
                             TransactionId xmin,
                             TransactionId xmax,
                             const void *old_data,
                             size_t old_len,
                             const void *new_data,
                             size_t new_len)
{
    (void)wal;
    (void)txn_id;
    (void)relfilenode;
    (void)blocknum;
    (void)offset;
    (void)new_blocknum;
    (void)new_offset;
    (void)xmin;
    (void)xmax;
    (void)old_data;
    (void)old_len;
    (void)new_data;
    (void)new_len;

    /* TODO: 实现实际的 WAL 写入 */

    return 0;
}

/**
 * @brief 写入 MVCC 堆表删除 WAL
 */
uint64_t mvcc_wal_heap_delete(wal_t *wal,
                              uint32_t txn_id,
                              uint32_t relfilenode,
                              uint32_t blocknum,
                              uint32_t offset,
                              TransactionId xmin,
                              TransactionId xmax,
                              const void *old_data,
                              size_t old_len)
{
    (void)wal;
    (void)txn_id;
    (void)relfilenode;
    (void)blocknum;
    (void)offset;
    (void)xmin;
    (void)xmax;
    (void)old_data;
    (void)old_len;

    /* TODO: 实现实际的 WAL 写入 */

    return 0;
}

/**
 * @brief 写入事务提交 WAL（含 CSN）
 */
uint64_t mvcc_wal_xact_commit(wal_t *wal, TransactionId txn_id, uint64_t csn)
{
    (void)wal;
    (void)txn_id;
    (void)csn;

    /* TODO: 实现实际的 WAL 写入 */

    /* 注册 CSN */
    register_csn(txn_id, csn);

    return 0;
}

/**
 * @brief 写入事务回滚 WAL
 */
uint64_t mvcc_wal_xact_abort(wal_t *wal, TransactionId txn_id)
{
    (void)wal;
    (void)txn_id;

    /* TODO: 实现实际的 WAL 写入 */

    return 0;
}

/**
 * @brief 写入元组冻结 WAL
 */
uint64_t mvcc_wal_tuple_freeze(wal_t *wal,
                               TransactionId txn_id,
                               uint32_t relfilenode,
                               uint32_t blocknum,
                               uint32_t offset,
                               TransactionId old_xmin)
{
    (void)wal;
    (void)txn_id;
    (void)relfilenode;
    (void)blocknum;
    (void)offset;
    (void)old_xmin;

    /* TODO: 实现实际的 WAL 写入 */

    return 0;
}

/**
 * @brief 写入 VACUUM WAL
 */
uint64_t mvcc_wal_vacuum(wal_t *wal,
                        TransactionId txn_id,
                        uint32_t relfilenode,
                        uint32_t blocknum,
                        const uint32_t *offsets,
                        size_t count)
{
    (void)wal;
    (void)txn_id;
    (void)relfilenode;
    (void)blocknum;
    (void)offsets;
    (void)count;

    /* TODO: 实现实际的 WAL 写入 */

    return 0;
}

/* ========================================================================
 * 恢复时的 MVCC 处理
 * ======================================================================== */

/**
 * @brief 初始化 MVCC 恢复上下文
 */
MvccRecoveryCtx *mvcc_recovery_init(void)
{
    MvccRecoveryCtx *ctx = (MvccRecoveryCtx *)calloc(1, sizeof(MvccRecoveryCtx));
    if (!ctx) {
        return NULL;
    }

    ctx->current_csn = 0;
    ctx->last_complete_xid = INVALID_TRANSACTION_ID;

    g_recovery_ctx = ctx;
    return ctx;
}

/**
 * @brief 清理 MVCC 恢复上下文
 */
void mvcc_recovery_cleanup(MvccRecoveryCtx *ctx)
{
    if (ctx) {
        free(ctx);
    }
    if (g_recovery_ctx == ctx) {
        g_recovery_ctx = NULL;
    }
}

/**
 * @brief 处理 MVCC WAL 记录（恢复时调用）
 */
int mvcc_recovery_apply(MvccRecoveryCtx *ctx,
                       mvcc_wal_type_t type,
                       const MvccWalHeader *header,
                       const void *data,
                       size_t data_len)
{
    if (!ctx || !header) {
        return -1;
    }

    switch (type) {
        case MVCC_WAL_HEAP_INSERT:
            /* 恢复插入：将元组恢复到页面 */
            /* TODO: 实现页面恢复逻辑 */
            break;

        case MVCC_WAL_HEAP_UPDATE:
            /* 恢复更新：标记旧元组为已删除，创建新元组 */
            /* TODO: 实现更新恢复逻辑 */
            break;

        case MVCC_WAL_HEAP_DELETE:
            /* 恢复删除：设置 xmax */
            /* TODO: 实现删除恢复逻辑 */
            break;

        case MVCC_WAL_HEAP_LOCK:
            /* 恢复锁：更新锁状态 */
            /* TODO: 实现锁恢复逻辑 */
            break;

        case MVCC_WAL_TUPLE_FREEZE:
            /* 恢复冻结：更新 xmin 为 FROZEN */
            /* TODO: 实现冻结恢复逻辑 */
            break;

        case MVCC_WAL_XACT_COMMIT:
            /* 恢复提交：注册 CSN */
            if (data_len >= sizeof(TransactionId)) {
                TransactionId xid = *(TransactionId *)data;
                uint64_t csn = mvcc_alloc_csn();
                register_csn(xid, csn);
                ctx->last_complete_xid = xid;
                ctx->current_csn = csn;
            }
            break;

        case MVCC_WAL_XACT_ABORT:
            /* 恢复回滚：不做额外处理，之前的 undo 已恢复数据 */
            /* TODO: 可能需要清理某些状态 */
            break;

        case MVCC_WAL_VACUUM:
            /* 恢复 VACUUM：清理元组 */
            /* TODO: 实现 VACUUM 恢复逻辑 */
            break;

        default:
            /* 未知类型，跳过 */
            break;
    }

    (void)data;
    (void)data_len;

    return 0;
}

/* ========================================================================
 * 可见性与 CSN
 * ======================================================================== */

/**
 * @brief 使用 CSN 判断元组是否可见
 *
 * 基于 CSN 的可见性判断是 MVCC 的另一种实现方式。
 * 通过提交序列号而非事务 ID 来确定可见性顺序。
 *
 * @param rv ReadView
 * @param tuple_fields 元组字段
 * @param current_xid 当前事务 ID
 * @param csn 元组的提交序列号（如果有）
 * @return true 可见
 */
bool mvcc_tuple_visible_by_csn(const ReadView *rv,
                               const HeapTupleFields *tuple_fields,
                               TransactionId current_xid,
                               uint64_t csn)
{
    if (!rv || !tuple_fields) {
        return false;
    }

    TransactionId xmin = tuple_fields->xmin;

    /*
     * 规则 1: 事务自己的 INSERT 始终可见
     */
    if (xmin == current_xid) {
        return true;
    }

    /*
     * 规则 2: 检查 xmin 的 CSN
     */
    uint64_t xmin_csn = mvcc_get_tuple_csn(xmin);
    if (xmin_csn == 0) {
        /* xmin 未提交（可能在活跃列表中） */
        if (is_active_in_readview(rv, xmin)) {
            return false;
        }
        /* 未注册 CSN，可能在恢复过程中 */
        return false;
    }

    /*
     * 规则 3: 检查快照
     * 元组对快照可见当且仅当：
     * - xmin 的 CSN < 快照的 CSN
     */
    if (xmin_csn > rv->snapshot_xmax) {
        /* 元组在快照之后提交，不可见 */
        return false;
    }

    /*
     * 规则 4: 检查 xmax（删除/更新）
     */
    if (mvcc_is_xid_valid(tuple_fields->xmax)) {
        uint64_t xmax_csn = mvcc_get_tuple_csn(tuple_fields->xmax);
        if (xmax_csn > 0 && xmax_csn <= rv->snapshot_xmax) {
            /* 删除事务在快照之前提交，不可恢复 */
            if (tuple_fields->xmax_status != HEAP_XMAX_LOCK_ONLY) {
                return false;
            }
        }
    }

    return true;
}
