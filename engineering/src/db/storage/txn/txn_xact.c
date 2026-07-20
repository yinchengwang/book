/**
 * @file txn_xact.c
 * @brief 事务状态管理实现
 *
 * 管理事务的生命周期：Active -> Committed/Aborted
 * 维护活跃事务列表，用于 ReadView 构建
 */

#include "db/storage/txn/mvcc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>  /* for tolower */

/* 确保 bool 类型可用 */
#include <stdbool.h>

/* ========================================================================
 * 大小写不敏感字符串比较（跨平台）
 * ======================================================================== */
static int str_equal_ignore_case(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* ========================================================================
 * 事务槽位管理
 * ======================================================================== */

/** 事务槽位 */
typedef struct XactSlot_s {
    TransactionId xid;           /**< 事务 ID */
    transaction_status_t status;   /**< 事务状态 */
    uint64_t start_time;        /**< 开始时间 */
    uint64_t csn;               /**< 提交序列号（用于 SI） */
    void *user_data;             /**< 用户数据 */
} XactSlot;

/** 事务槽位数组 */
static XactSlot *g_xact_slots = NULL;

/** 空闲槽位链表头 */
static int g_free_list_head = -1;

/** 活跃事务链表 */
static int g_active_list_head = -1;

/** 槽位数量 */
#define XACT_SLOT_COUNT 8192

/** 初始化事务槽位 */
static int init_xact_slots(void)
{
    if (g_xact_slots != NULL) {
        return 0;
    }

    g_xact_slots = (XactSlot *)calloc(XACT_SLOT_COUNT, sizeof(XactSlot));
    if (!g_xact_slots) {
        return -1;
    }

    /* 初始化空闲链表 */
    g_free_list_head = 0;
    for (int i = 0; i < XACT_SLOT_COUNT - 1; i++) {
        g_xact_slots[i].status = TRANSACTION_STATUS_IN_PROGRESS;
        /* 简单的链表：槽位 i 的下一个是 i+1 */
    }
    g_xact_slots[XACT_SLOT_COUNT - 1].status = TRANSACTION_STATUS_IN_PROGRESS;

    g_active_list_head = -1;

    return 0;
}

/* ========================================================================
 * 事务操作 API
 * ======================================================================== */

/**
 * @brief 开始一个新事务
 *
 * @param xid 输出参数，返回分配的事务 ID
 * @return 0 成功，-1 失败
 */
int txn_begin(TransactionId *xid)
{
    if (init_xact_slots() != 0) {
        return -1;
    }

    if (g_free_list_head < 0) {
        /* 没有空闲槽位，应该执行清理 */
        return -1;
    }

    /* 分配槽位 */
    int slot = g_free_list_head;
    XactSlot *slot_ptr = &g_xact_slots[slot];

    /* 分配新的事务 ID */
    TransactionId new_xid = mvcc_alloc_xid();

    /* 初始化槽位 */
    slot_ptr->xid = new_xid;
    slot_ptr->status = TRANSACTION_STATUS_IN_PROGRESS;
    slot_ptr->start_time = 0;  /* TODO: 获取当前时间 */
    slot_ptr->csn = 0;
    slot_ptr->user_data = NULL;

    /* 从空闲链表移除 */
    g_free_list_head = slot + 1;  /* 简化实现 */

    /* 添加到活跃链表 */
    slot_ptr->status = TRANSACTION_STATUS_IN_PROGRESS;

    if (xid) {
        *xid = new_xid;
    }

    return 0;
}

/**
 * @brief 提交事务
 *
 * @param xid 事务 ID
 * @return 0 成功，-1 失败
 */
int txn_commit(TransactionId xid)
{
    if (!g_xact_slots) {
        return -1;
    }

    /* 查找事务槽位（简化：线性搜索） */
    for (int i = 0; i < XACT_SLOT_COUNT; i++) {
        if (g_xact_slots[i].xid == xid) {
            g_xact_slots[i].status = TRANSACTION_STATUS_COMMITTED;
            g_xact_slots[i].csn = mvcc_get_current_xid();  /* 用 XID 作为 CSN */
            mvcc_mark_committed(xid);
            return 0;
        }
    }

    return -1;
}

/**
 * @brief 回滚事务
 *
 * @param xid 事务 ID
 * @return 0 成功，-1 失败
 */
int txn_rollback(TransactionId xid)
{
    if (!g_xact_slots) {
        return -1;
    }

    /* 查找事务槽位 */
    for (int i = 0; i < XACT_SLOT_COUNT; i++) {
        if (g_xact_slots[i].xid == xid) {
            g_xact_slots[i].status = TRANSACTION_STATUS_ABORTED;
            mvcc_mark_aborted(xid);
            return 0;
        }
    }

    return -1;
}

/**
 * @brief 获取事务状态
 *
 * @param xid 事务 ID
 * @return 事务状态，未知返回 -1
 */
transaction_status_t txn_get_status(TransactionId xid)
{
    if (!g_xact_slots) {
        return -1;
    }

    for (int i = 0; i < XACT_SLOT_COUNT; i++) {
        if (g_xact_slots[i].xid == xid) {
            return g_xact_slots[i].status;
        }
    }

    return -1;
}

/**
 * @brief 获取活跃事务数量
 *
 * @return 活跃事务数
 */
int txn_active_count(void)
{
    if (!g_xact_slots) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < XACT_SLOT_COUNT; i++) {
        if (g_xact_slots[i].xid != INVALID_TRANSACTION_ID &&
            g_xact_slots[i].status == TRANSACTION_STATUS_IN_PROGRESS) {
            count++;
        }
    }
    return count;
}

/**
 * @brief 获取所有活跃事务 ID
 *
 * @param xids 输出数组
 * @param max_count 数组最大长度
 * @return 实际写入的数量
 */
int txn_get_active_xids(TransactionId *xids, int max_count)
{
    if (!g_xact_slots || !xids) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < XACT_SLOT_COUNT && count < max_count; i++) {
        if (g_xact_slots[i].xid != INVALID_TRANSACTION_ID &&
            g_xact_slots[i].status == TRANSACTION_STATUS_IN_PROGRESS) {
            xids[count++] = g_xact_slots[i].xid;
        }
    }
    return count;
}

/**
 * @brief 检查是否有足够的事务槽位
 *
 * @return true 充足
 */
bool txn_has_free_slots(void)
{
    return txn_active_count() < XACT_SLOT_COUNT - 100;
}

/**
 * @brief 清理已结束的事务
 *
 * 释放已完成事务的槽位
 */
void txn_cleanup_finished(void)
{
    if (!g_xact_slots) {
        return;
    }

    /* TODO: 实现清理逻辑，将已结束的事务从活跃链表移除 */
    /* 简化版本：暂时不释放槽位 */
}

/* ========================================================================
 * 简化的事务上下文
 * ======================================================================== */

/** 当前线程的事务上下文 */
typedef struct TxnContext_s {
    TransactionId xid;           /**< 当前事务 ID */
    isolation_level_t isolation;  /**< 隔离级别 */
    ReadView *readview;          /**< 读视图 */
    bool in_transaction;        /**< 是否在事务中 */
} TxnContext;

/** 当前线程的事务上下文（线程本地） */
static THREAD_LOCAL TxnContext *g_txn_context = NULL;

/**
 * @brief 获取当前事务上下文
 */
TxnContext *txn_get_context(void)
{
    if (g_txn_context == NULL) {
        g_txn_context = (TxnContext *)calloc(1, sizeof(TxnContext));
    }
    return g_txn_context;
}

/**
 * @brief 开始事务
 */
int txn_start(isolation_level_t isolation)
{
    TxnContext *ctx = txn_get_context();
    if (ctx->in_transaction) {
        return -1;  /* 已在事务中 */
    }

    TransactionId xid;
    if (txn_begin(&xid) != 0) {
        return -1;
    }

    ctx->xid = xid;
    ctx->isolation = isolation;
    ctx->in_transaction = true;

    /* SI/SSI 级别：创建 ReadView */
    if (isolation != ISOLATION_READ_COMMITTED) {
        ctx->readview = mvcc_create_readview(isolation);
        mvcc_set_current_readview(ctx->readview);
    }

    return 0;
}

/**
 * @brief 提交事务
 */
int txn_end(void)
{
    TxnContext *ctx = txn_get_context();
    if (!ctx->in_transaction) {
        return -1;  /* 不在事务中 */
    }

    if (txn_commit(ctx->xid) != 0) {
        return -1;
    }

    /* 清理 ReadView */
    if (ctx->readview) {
        mvcc_destroy_readview(ctx->readview);
        ctx->readview = NULL;
    }

    ctx->in_transaction = false;

    return 0;
}

/**
 * @brief 回滚事务
 */
int txn_abort(void)
{
    TxnContext *ctx = txn_get_context();
    if (!ctx->in_transaction) {
        return -1;
    }

    if (txn_rollback(ctx->xid) != 0) {
        return -1;
    }

    /* 清理 ReadView */
    if (ctx->readview) {
        mvcc_destroy_readview(ctx->readview);
        ctx->readview = NULL;
    }

    ctx->in_transaction = false;

    return 0;
}

/**
 * @brief 获取当前事务 ID
 */
TransactionId txn_current_xid(void)
{
    TxnContext *ctx = txn_get_context();
    return ctx->xid;
}

/**
 * @brief 检查是否在事务中
 */
bool txn_in_progress(void)
{
    TxnContext *ctx = txn_get_context();
    return ctx->in_transaction;
}

/**
 * @brief 刷新 ReadView（RC 级别）
 *
 * RC 级别每个语句需要刷新 ReadView
 * RR/SSI 级别：ReadView 在事务期间保持不变，不刷新
 */
void txn_refresh_readview(void)
{
    TxnContext *ctx = txn_get_context();
    if (!ctx || !ctx->in_transaction) {
        return;
    }

    if (ctx->isolation == ISOLATION_READ_COMMITTED) {
        /* RC 级别：每个语句刷新 ReadView */
        /* 销毁旧的 ReadView */
        if (ctx->readview) {
            mvcc_destroy_readview(ctx->readview);
        }
        /* 创建新的 ReadView */
        ctx->readview = mvcc_create_readview(ctx->isolation);
        mvcc_set_current_readview(ctx->readview);
    }
    /* RR/SSI 级别：ReadView 在事务开始时创建，整个事务期间保持不变 */
    /* 不执行刷新操作 */
}

/**
 * @brief 获取当前隔离级别
 */
isolation_level_t txn_get_isolation_level(void)
{
    TxnContext *ctx = txn_get_context();
    return ctx->isolation;
}

/**
 * @brief 检查是否为 REPEATABLE READ 或更高隔离级别
 *
 * @return true 如果是 RR 或 SERIALIZABLE
 */
bool txn_is_snapshot_isolation(void)
{
    TxnContext *ctx = txn_get_context();
    return ctx->in_transaction &&
           (ctx->isolation == ISOLATION_REPEATABLE_READ ||
            ctx->isolation == ISOLATION_SERIALIZABLE);
}

/* ========================================================================
 * 隔离级别解析
 * ======================================================================== */

/**
 * @brief 从字符串解析隔离级别
 * @param level_str 隔离级别字符串
 * @return 隔离级别，失败返回 -1
 */
isolation_level_t txn_parse_isolation_level(const char *level_str)
{
    if (!level_str) return (isolation_level_t)-1;

    /* 支持的大小写不敏感匹配 */
    if (str_equal_ignore_case(level_str, "read committed") == 0 ||
        str_equal_ignore_case(level_str, "read-committed") == 0 ||
        str_equal_ignore_case(level_str, "rc") == 0) {
        return ISOLATION_READ_COMMITTED;
    }
    if (str_equal_ignore_case(level_str, "repeatable read") == 0 ||
        str_equal_ignore_case(level_str, "repeatable-read") == 0 ||
        str_equal_ignore_case(level_str, "rr") == 0) {
        return ISOLATION_REPEATABLE_READ;
    }
    if (str_equal_ignore_case(level_str, "serializable") == 0 ||
        str_equal_ignore_case(level_str, "ssi") == 0) {
        return ISOLATION_SERIALIZABLE;
    }
    return (isolation_level_t)-1;
}

/**
 * @brief 获取隔离级别的字符串表示
 * @param level 隔离级别
 * @return 字符串
 */
const char *txn_isolation_level_name(isolation_level_t level)
{
    switch (level) {
        case ISOLATION_READ_COMMITTED:  return "read committed";
        case ISOLATION_REPEATABLE_READ: return "repeatable read";
        case ISOLATION_SERIALIZABLE:    return "serializable";
        default:                        return "unknown";
    }
}

/**
 * @brief 打印事务状态（调试用）
 */
void txn_dump_status(void)
{
    printf("=== Transaction Status ===\n");
    printf("Active count: %d\n", txn_active_count());

    TxnContext *ctx = txn_get_context();
    if (ctx && ctx->in_transaction) {
        printf("Current XID: %u\n", ctx->xid);
        printf("Isolation: %d\n", ctx->isolation);
    } else {
        printf("No active transaction\n");
    }
}
