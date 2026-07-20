/**
 * @file mvcc.c
 * @brief MVCC 多版本并发控制实现
 *
 * 参考 PostgreSQL 实现：
 * - src/backend/access/transam/xact.c
 * - src/backend/access/heap/heapam_visibility.c
 * - src/backend/storage/lmgr/predicate.c
 */

#include "db/storage/txn/mvcc.h"
#include "db/storage/txn/predicate.h"
#include "db/storage/txn/vacuum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/* ========================================================================
 * 全局状态
 * ======================================================================== */

/** 当前事务 ID */
static atomic_uint_fast32_t g_current_xid = TRANSACTION_ID_OFFSET;

/** 事务状态表（简化为固定大小数组） */
static atomic_uint_fast32_t g_txn_status[MAX_ACTIVE_TRANSACTIONS];
static atomic_uint_fast32_t g_txn_xid[MAX_ACTIVE_TRANSACTIONS];

/** 活跃事务计数 */
static atomic_uint_fast32_t g_active_txn_count = 0;

/** XID horizon（最老的活跃事务边界） */
static atomic_uint_fast32_t g_xid_horizon = TRANSACTION_ID_OFFSET;

/** 当前线程的 ReadView */
static THREAD_LOCAL ReadView *g_current_readview = NULL;

/** 统计信息 */
static atomic_uint_fast64_t g_xid_alloc_count = 0;
static atomic_uint_fast64_t g_readview_create_count = 0;
static atomic_uint_fast64_t g_visibility_check_count = 0;
static atomic_uint_fast64_t g_version_follow_count = 0;

/** GUC 参数默认值 */
isolation_level_t mvcc_current_isolation = ISOLATION_READ_COMMITTED;
uint32_t mvcc_xid_wraparound_threshold = (MAX_TRANSACTION_ID - 100000);

/* ========================================================================
 * TransactionId 管理
 * ======================================================================== */

/**
 * @brief 分配新的事务 ID
 *
 * 线程安全地递增分配事务 ID
 */
TransactionId mvcc_alloc_xid(void)
{
    uint32_t old_xid, new_xid;

    do {
        old_xid = atomic_load(&g_current_xid);
        new_xid = old_xid + 1;

        /* 检查 wraparound */
        if (new_xid >= MAX_TRANSACTION_ID - 100) {
            /* 接近最大值，应该触发 freeze */
            atomic_store(&g_xid_horizon, FROZEN_TRANSACTION_ID);
        }

    } while (!atomic_compare_exchange_weak(&g_current_xid, &old_xid, new_xid));

    atomic_fetch_add(&g_xid_alloc_count, 1);

    return new_xid;
}

/**
 * @brief 获取当前事务 ID
 */
TransactionId mvcc_get_current_xid(void)
{
    return atomic_load(&g_current_xid);
}

/**
 * @brief 检查事务 ID 是否有效
 */
bool mvcc_is_xid_valid(TransactionId xid)
{
    return xid != INVALID_TRANSACTION_ID &&
           xid != FROZEN_TRANSACTION_ID &&
           xid >= TRANSACTION_ID_OFFSET;
}

/**
 * @brief 检查是否是 Frozen 事务 ID
 */
bool mvcc_is_xid_frozen(TransactionId xid)
{
    return xid == FROZEN_TRANSACTION_ID;
}

/**
 * @brief 检查两个事务 ID 的顺序
 *
 * 使用 mod 2^31 算法处理 wraparound
 */
bool mvcc_xid_before(TransactionId xid1, TransactionId xid2)
{
    /* 处理 Frozen */
    if (xid1 == FROZEN_TRANSACTION_ID) return true;
    if (xid2 == FROZEN_TRANSACTION_ID) return false;

    int32_t diff = (int32_t)(xid1 - xid2);
    return diff < 0;
}

/**
 * @brief 检查事务 ID 是否在某个点之后
 */
bool mvcc_xid_after(TransactionId xid, TransactionId before_xid)
{
    return mvcc_xid_before(before_xid, xid);
}

/**
 * @brief 获取 XID horizon
 */
TransactionId mvcc_get_xid_horizon(void)
{
    return atomic_load(&g_xid_horizon);
}

/* ========================================================================
 * 事务状态管理
 * ======================================================================== */

/**
 * @brief 标记事务为已提交
 */
void mvcc_mark_committed(TransactionId xid)
{
    /* 简化实现：直接更新状态 */
    /* 实际实现应该用更复杂的状态表 */
    (void)xid;
    /* 在真实实现中，这里需要写入持久化存储 */
}

/**
 * @brief 标记事务为已回滚
 */
void mvcc_mark_aborted(TransactionId xid)
{
    (void)xid;
    /* 在真实实现中，这里需要写入持久化存储 */
}

/**
 * @brief 获取事务状态
 */
transaction_status_t mvcc_get_status(TransactionId xid)
{
    /* 简化实现：假设所有未知事务都是进行中 */
    /* 实际实现需要查询事务状态表 */
    (void)xid;
    return TRANSACTION_STATUS_IN_PROGRESS;
}

/**
 * @brief 检查事务是否在指定时间点已提交
 */
bool mvcc_is_committed_at(TransactionId xid, TransactionId horizon)
{
    if (!mvcc_is_xid_valid(xid)) {
        return false;
    }

    if (mvcc_is_xid_frozen(xid)) {
        return true;
    }

    /* 简化：如果 xid < horizon，认为已提交 */
    if (mvcc_xid_before(xid, horizon)) {
        return true;
    }

    /* 检查事务状态表 */
    transaction_status_t status = mvcc_get_status(xid);
    return status == TRANSACTION_STATUS_COMMITTED;
}

/* ========================================================================
 * ReadView 管理
 * ======================================================================== */

/**
 * @brief 创建 ReadView
 */
ReadView *mvcc_create_readview(isolation_level_t isolation)
{
    ReadView *rv = (ReadView *)malloc(sizeof(ReadView));
    if (!rv) {
        return NULL;
    }

    memset(rv, 0, sizeof(ReadView));

    rv->isolation = isolation;
    rv->xmin = mvcc_get_xid_horizon();

    /* xmax 是当前最大已分配事务 ID */
    rv->xmax = mvcc_get_current_xid();

    /* 为活跃事务分配数组 */
    rv->num_active = 0;
    rv->active_txs = NULL;

    /* snapshot_xmax 初始化为 xmax */
    rv->snapshot_xmax = rv->xmax;

    /* 在 RC 模式下，ReadView 会频繁创建/销毁 */
    /* 在 SI/SSI 模式下，ReadView 会在事务期间保持 */

    atomic_fetch_add(&g_readview_create_count, 1);

    return rv;
}

/**
 * @brief 释放 ReadView
 */
void mvcc_destroy_readview(ReadView *rv)
{
    if (!rv) {
        return;
    }

    if (rv->active_txs) {
        free(rv->active_txs);
    }

    free(rv);
}

/**
 * @brief 获取当前事务的 ReadView
 */
ReadView *mvcc_get_current_readview(void)
{
    return g_current_readview;
}

/**
 * @brief 设置当前事务的 ReadView
 */
void mvcc_set_current_readview(ReadView *rv)
{
    g_current_readview = rv;
}

/* ========================================================================
 * 元组可见性判断
 * ======================================================================== */

/**
 * @brief 检查事务 ID 是否在活跃事务列表中
 */
bool is_active_in_readview(const ReadView *rv, TransactionId xid)
{
    if (!rv || !rv->active_txs) {
        return false;
    }

    for (int32_t i = 0; i < rv->num_active; i++) {
        if (rv->active_txs[i] == xid) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 判断元组对 ReadView 是否可见
 *
 * 核心可见性判断逻辑，参考 PostgreSQL 的 HeapTupleSatisfiesMVCC
 */
bool mvcc_tuple_visible(const ReadView *rv, const HeapTupleFields *tuple_fields)
{
    if (!rv || !tuple_fields) {
        return false;
    }

    atomic_fetch_add(&g_visibility_check_count, 1);

    TransactionId xmin = tuple_fields->xmin;
    TransactionId xmax = tuple_fields->xmax;

    /*
     * 规则 1: xmin = 0 表示元组从未被提交插入
     */
    if (!mvcc_is_xid_valid(xmin)) {
        return false;
    }

    /*
     * 规则 2: 如果 xmin 在 ReadView 中活跃，则不可见
     */
    if (is_active_in_readview(rv, xmin)) {
        return false;
    }

    /*
     * 规则 3: 如果 xmin 未提交，则不可见
     */
    if (!mvcc_is_committed_at(xmin, rv->xmin)) {
        /* xmin 未提交 */
        if (tuple_fields->xmin_status == HEAP_XMIN_INVALID) {
            return false;
        }
        /* 如果 xmin 状态是 COMMITTED（但 xid > xmin），需要重新判断 */
    }

    /*
     * 规则 4: 如果 xmin 在 ReadView 的 xmax 之后，且事务状态是 IN_PROGRESS，
     *         则不可见（该事务在快照之后才提交）
     */
    if (mvcc_xid_after(xmin, rv->snapshot_xmax) &&
        !mvcc_is_committed_at(xmin, rv->snapshot_xmax)) {
        return false;
    }

    /*
     * 规则 5: 如果 xmax = 0 或 xmax_INVALID，表示未被删除
     */
    if (!mvcc_is_xid_valid(xmax) ||
        tuple_fields->xmax_status == HEAP_XMAX_INVALID) {
        return true;  /* 可见 */
    }

    /*
     * 规则 6: 如果 xmax 在 ReadView 中活跃，则可能可见也可能不可见
     */
    if (is_active_in_readview(rv, xmax)) {
        /* 如果是 LOCK_ONLY，则仍可见 */
        if (tuple_fields->xmax_status == HEAP_XMAX_LOCK_ONLY) {
            return true;
        }
        return false;
    }

    /*
     * 规则 7: 如果 xmax 未提交，则仍可见
     */
    if (!mvcc_is_committed_at(xmax, rv->xmin)) {
        return true;
    }

    /*
     * 规则 8: 如果 xmax 已提交，且不是 LOCK_ONLY，则不可见
     */
    if (tuple_fields->xmax_status != HEAP_XMAX_LOCK_ONLY) {
        return false;
    }

    return true;
}

/**
 * @brief 获取元组最新版本
 *
 * 如果当前元组已被更新，需要沿着版本链找到最新可见版本
 */
HeapTupleFields *mvcc_get_latest_version(const ReadView *rv,
                                        const HeapTupleFields *tuple_fields)
{
    if (!rv || !tuple_fields) {
        return NULL;
    }

    (void)tuple_fields;
    atomic_fetch_add(&g_version_follow_count, 1);

    /*
     * 简化实现：假设 ctid 指向新版本
     * 实际实现需要：
     * 1. 读取 ctid 指向的新版本
     * 2. 递归检查新版本的可见性
     * 3. 如果仍不可见，继续向下
     */

    /* TODO: 实现版本链遍历 */

    return NULL;  /* 简化版本：不做版本链遍历 */
}

/* ========================================================================
 * 元组版本链操作
 * ======================================================================== */

/**
 * @brief 创建一个新版本的元组
 */
HeapTupleFields *mvcc_create_new_version(const HeapTupleFields *old_tuple_fields,
                                         TransactionId new_xid)
{
    /* 简化实现 */
    (void)old_tuple_fields;
    (void)new_xid;
    return NULL;  /* 实际应该在元组存储中创建新版本 */
}

/**
 * @brief 标记元组为已删除
 */
void mvcc_mark_deleted(HeapTupleFields *tuple_fields,
                       TransactionId xid,
                       heap_xmax_status_t status)
{
    if (!tuple_fields) {
        return;
    }

    tuple_fields->xmax = xid;
    tuple_fields->xmax_status = status;
    tuple_fields->t_ctid_is_self = false;
}

/**
 * @brief 冻结元组
 */
void mvcc_freeze_tuple(HeapTupleFields *tuple_fields)
{
    if (!tuple_fields) {
        return;
    }

    tuple_fields->xmin = FROZEN_TRANSACTION_ID;
    tuple_fields->xmin_status = HEAP_XMIN_FROZEN;
}

/* ========================================================================
 * REPEATABLE READ 支持
 * ======================================================================== */

/**
 * @brief REPEATABLE READ 隔离级别下检查元组是否可见
 *
 * 在 REPEATABLE READ 模式下，ReadView 在事务开始时创建，
 * 整个事务期间保持不变。这意味着：
 * 1. 事务开始时提交的数据可见
 * 2. 事务开始后其他事务提交的修改不可见
 * 3. 当前事务的修改始终可见（即使未提交）
 *
 * @param rv ReadView
 * @param tuple_fields 元组的事务字段
 * @param current_xid 当前事务 ID
 * @return true 可见
 */
bool mvcc_repeatable_read_visible(const ReadView *rv,
                                   const HeapTupleFields *tuple_fields,
                                   TransactionId current_xid)
{
    if (!rv || !tuple_fields) {
        return false;
    }

    TransactionId xmin = tuple_fields->xmin;
    TransactionId xmax = tuple_fields->xmax;

    /*
     * 规则 1: 事务自己的 INSERT 始终可见
     */
    if (xmin == current_xid) {
        return true;
    }

    /*
     * 规则 2: xmin 必须对事务开始时的快照可见
     * xmin < rv->xmax 且 xmin 不在活跃列表中
     */
    if (mvcc_xid_before(xmin, rv->xmax)) {
        /* xmin 在快照之前，检查是否已提交 */
        if (!is_active_in_readview(rv, xmin)) {
            return true;
        }
    }

    /*
     * 规则 3: 在快照之后开始的插入不可见
     */
    if (!mvcc_xid_before(xmin, rv->xmax)) {
        /* xmin >= rv->xmax，在快照之后 */
        return false;
    }

    /*
     * 规则 4: 检查 xmax（删除/更新）
     */
    if (mvcc_is_xid_valid(xmax) && xmax != current_xid) {
        /* 如果删除事务已提交且不是当前事务，不可见 */
        if (!is_active_in_readview(rv, xmax)) {
            transaction_status_t status = mvcc_get_status(xmax);
            if (status == TRANSACTION_STATUS_COMMITTED &&
                tuple_fields->xmax_status != HEAP_XMAX_LOCK_ONLY) {
                return false;  /* 已被其他事务删除 */
            }
        }
    }

    return true;
}

/**
 * @brief 检查 RR 模式下是否需要报告序列化冲突
 *
 * 当其他事务提交的修改与当前事务读取的数据重叠时，
 * 可能发生序列化冲突。
 *
 * @param rv ReadView
 * @param xid 可能冲突的事务 ID
 * @return true 存在冲突
 */
bool mvcc_repeatable_read_has_conflict(const ReadView *rv, TransactionId xid)
{
    if (!rv) {
        return false;
    }

    /* 如果 xid 在快照之后提交，且与当前事务读取的数据重叠，则有冲突 */
    return mvcc_xid_after(xid, rv->snapshot_xmax);
}

/* ========================================================================
 * SERIALIZABLE SSI 支持
 * ======================================================================== */

/**
 * @brief SERIALIZABLE 隔离级别下检查元组是否可见
 *
 * 与 REPEATABLE READ 类似，但需要额外的 SSI 检测：
 * 1. 记录读写依赖和写读依赖
 * 2. 检测可能导致序列化异常的依赖环
 */
bool mvcc_serializable_visible(const ReadView *rv,
                              const HeapTupleFields *tuple_fields,
                              TransactionId current_xid)
{
    if (!rv || !tuple_fields) {
        return false;
    }

    TransactionId xmin = tuple_fields->xmin;
    TransactionId xmax = tuple_fields->xmax;

    /*
     * 规则 1: 事务自己的 INSERT 始终可见
     */
    if (xmin == current_xid) {
        return true;
    }

    /*
     * 规则 2: xmin 在快照范围内，且不在活跃列表中
     */
    if (mvcc_xid_before(xmin, rv->xmax)) {
        if (!is_active_in_readview(rv, xmin)) {
            /* 记录读-写依赖：当前事务读取了其他事务写入的数据 */
            if (xmin != current_xid && mvcc_get_status(xmin) == TRANSACTION_STATUS_COMMITTED) {
                ssi_record_rw_depend(xmin, current_xid);
            }
            return true;
        }
    }

    /*
     * 规则 3: 在快照之后开始的插入不可见
     */
    if (!mvcc_xid_before(xmin, rv->xmax)) {
        return false;
    }

    /*
     * 规则 4: 检查 xmax（删除/更新）
     */
    if (mvcc_is_xid_valid(xmax) && xmax != current_xid) {
        if (!is_active_in_readview(rv, xmax)) {
            transaction_status_t status = mvcc_get_status(xmax);
            if (status == TRANSACTION_STATUS_COMMITTED &&
                tuple_fields->xmax_status != HEAP_XMAX_LOCK_ONLY) {
                /* 记录写-读依赖：其他事务删除了当前事务读取的数据 */
                ssi_record_wr_depend(current_xid, xmax);
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief 检查序列化隔离级别下是否发生序列化异常
 *
 * @return true 正常可以继续，false 发生序列化冲突需要重试
 */
bool mvcc_serializable_check_anomaly(void)
{
    return ssi_can_continue();
}

/* ========================================================================
 * MVCC 初始化和清理
 * ======================================================================== */

/**
 * @brief 初始化 MVCC 子系统
 */
int mvcc_init(void)
{
    /* 初始化事务状态表 */
    for (int i = 0; i < MAX_ACTIVE_TRANSACTIONS; i++) {
        atomic_store(&g_txn_status[i], TRANSACTION_STATUS_IN_PROGRESS);
        atomic_store(&g_txn_xid[i], INVALID_TRANSACTION_ID);
    }

    atomic_store(&g_current_xid, TRANSACTION_ID_OFFSET);
    atomic_store(&g_active_txn_count, 0);
    atomic_store(&g_xid_horizon, TRANSACTION_ID_OFFSET);

    /* 初始化谓词锁子系统（SSI 支持） */
    predicate_lock_init();
    ssi_reset();

    /* 初始化 MVCC WAL 集成 */
    mvcc_wal_init();

    /* 初始化 VACUUM 子系统 */
    vacuum_init();

    return 0;
}

/**
 * @brief 关闭 MVCC 子系统
 */
void mvcc_shutdown(void)
{
    /* 清理 ReadView */
    if (g_current_readview) {
        mvcc_destroy_readview(g_current_readview);
        g_current_readview = NULL;
    }

    /* 清理谓词锁子系统 */
    predicate_lock_shutdown();

    /* 关闭 MVCC WAL 集成 */
    mvcc_wal_shutdown();

    /* 清理 VACUUM 子系统 */
    vacuum_shutdown();
}

/**
 * @brief 重置 MVCC 状态
 */
void mvcc_reset(void)
{
    mvcc_shutdown();
    mvcc_init();

    atomic_store(&g_xid_alloc_count, 0);
    atomic_store(&g_readview_create_count, 0);
    atomic_store(&g_visibility_check_count, 0);
    atomic_store(&g_version_follow_count, 0);
}

/* ========================================================================
 * GUC 参数
 * ======================================================================== */

/**
 * @brief 设置隔离级别
 */
void mvcc_set_isolation_level(isolation_level_t level)
{
    mvcc_current_isolation = level;
}

/**
 * @brief 获取当前隔离级别
 */
isolation_level_t mvcc_get_isolation_level(void)
{
    return mvcc_current_isolation;
}

/* ========================================================================
 * VACUUM 辅助
 * ======================================================================== */

/**
 * @brief 获取需要 freeze 的最老事务 ID
 */
TransactionId mvcc_get_oldest_xid_limit(void)
{
    return mvcc_get_xid_horizon();
}

/**
 * @brief 检查是否需要执行 VACUUM freeze
 */
bool mvcc_needs_vacuum(void)
{
    TransactionId current = mvcc_get_current_xid();
    return (current > MAX_TRANSACTION_ID - 100000);
}

/**
 * @brief 执行 Freeze 处理
 */
void mvcc_do_freeze(void)
{
    /* TODO: 实现 freeze 算法 */
    /* 遍历所有页面，将老旧 xid 替换为 FROZEN_TRANSACTION_ID */
}

/* ========================================================================
 * 调试和统计
 * ======================================================================== */

/**
 * @brief 获取 MVCC 统计信息
 */
void mvcc_get_stats(MvccStats *stats)
{
    if (!stats) {
        return;
    }

    stats->xid_allocations = atomic_load(&g_xid_alloc_count);
    stats->readview_creates = atomic_load(&g_readview_create_count);
    stats->visibility_checks = atomic_load(&g_visibility_check_count);
    stats->version_follows = atomic_load(&g_version_follow_count);
    stats->active_transactions = atomic_load(&g_active_txn_count);
}

/**
 * @brief 重置 MVCC 统计
 */
void mvcc_reset_stats(void)
{
    atomic_store(&g_xid_alloc_count, 0);
    atomic_store(&g_readview_create_count, 0);
    atomic_store(&g_visibility_check_count, 0);
    atomic_store(&g_version_follow_count, 0);
}

/**
 * @brief 打印 MVCC 状态
 */
void mvcc_dump_status(void)
{
    printf("=== MVCC Status ===\n");
    printf("Current XID: %u\n", mvcc_get_current_xid());
    printf("XID Horizon: %u\n", mvcc_get_xid_horizon());
    printf("Active Transactions: %u\n", atomic_load(&g_active_txn_count));
    printf("Isolation Level: %d\n", mvcc_current_isolation);
    printf("\n");
    printf("Statistics:\n");
    printf("  XID Allocations: %lu\n", atomic_load(&g_xid_alloc_count));
    printf("  ReadView Creates: %lu\n", atomic_load(&g_readview_create_count));
    printf("  Visibility Checks: %lu\n", atomic_load(&g_visibility_check_count));
    printf("  Version Follows: %lu\n", atomic_load(&g_version_follow_count));
}
