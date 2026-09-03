/**
 * @file txn.c
 * @brief MVCC 事务管理器实现
 *
 * 核心组件：
 * - XMIN/XMAX 事务标识：每行记录创建事务(xmin)和删除事务(xmax)
 * - Command ID (CID)：同一事务内的命令序号
 * - 子事务：支持嵌套事务
 * - 保存点：支持部分回滚
 *
 * ## 可见性判断规则
 *
 * 版本可见性由以下条件决定：
 * 1. xmin 事务已提交且不在当前快照的活跃列表中
 * 2. xmax = 0（未删除）或 xmax 事务未提交或 xmax 在活跃列表中
 * 3. 同一事务内：当前事务创建的版本可见（xmin == 当前事务ID）
 * 4. CID 判断：同一事务内早期命令创建的版本可能被后期命令看到
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <db/txn.h>
#include <db/concurrency/mvcc.h>

/* ============================================================
 * 全局事务管理器
 * ============================================================ */

/** 全局事务管理器状态 */
static struct txn_manager {
    txn_id_t current_xid;         /**< 当前最大事务ID */
    txn_id_t *active_xids;        /**< 活跃事务ID数组 */
    int active_count;             /**< 活跃事务数量 */
    int active_capacity;          /**< 数组容量 */
    void *lock;                   /**< 简单自旋锁（简化实现） */
} g_txn_manager = {
    .current_xid = 0,
    .active_xids = NULL,
    .active_count = 0,
    .active_capacity = 0,
    .lock = NULL
};

/* ============================================================
 * 事务上下文
 * ============================================================ */

/** 事务上下文结构 */
struct txn_context_s {
    txn_id_t xid;                 /**< 事务ID（XMIN） */
    txn_state_t state;            /**< 事务状态 */
    txn_type_t type;              /**< 事务类型 */
    txn_cid_t current_cid;        /**< 当前命令ID */
    txn_cid_t top_cid;            /**< 顶层命令ID（子事务用） */

    /** MVCC 快照信息（事务开始时获取） */
    txn_id_t snapshot_xmin;       /**< 快照XMIN */
    txn_id_t snapshot_xmax;       /**< 快照XMAX */
    txn_xip_list_t snapshot_xip;  /**< 快照XIP列表 */

    /** 子事务信息 */
    txn_id_t parent_xid;          /**< 父事务ID（0表示顶层事务） */
    int subxact_depth;            /**< 子事务嵌套深度 */

    /** 保存点栈 */
    txn_savepoint_stack_t savepoints;

    /** 时间戳 */
    uint64_t start_time;          /**< 事务开始时间 */

    /** Undo 链 */
    void *undo_head;              /**< Undo 链头指针 */

    /** 错误信息 */
    char error_msg[256];
};

/* ============================================================
 * 工具函数
 * ============================================================ */

static const char *txn_state_name(txn_state_t state)
{
    switch (state) {
        case TXN_STATE_ACTIVE:    return "ACTIVE";
        case TXN_STATE_COMMITTED: return "COMMITTED";
        case TXN_STATE_ABORTED:   return "ABORTED";
        case TXN_STATE_PREPARED:  return "PREPARED";
        default:                  return "UNKNOWN";
    }
}

/* ============================================================
 * 全局事务管理器实现
 * ============================================================ */

int txn_manager_init(size_t max_active)
{
    g_txn_manager.active_capacity = (int)max_active;
    g_txn_manager.active_xids = (txn_id_t *)calloc(
        max_active, sizeof(txn_id_t));
    if (g_txn_manager.active_xids == NULL) {
        return -1;
    }
    g_txn_manager.current_xid = TXN_ID_NONE;
    g_txn_manager.active_count = 0;
    return 0;
}

void txn_manager_shutdown(void)
{
    if (g_txn_manager.active_xids != NULL) {
        free(g_txn_manager.active_xids);
        g_txn_manager.active_xids = NULL;
    }
    g_txn_manager.active_count = 0;
    g_txn_manager.active_capacity = 0;
}

txn_id_t txn_manager_get_current_xid(void)
{
    return g_txn_manager.current_xid;
}

txn_id_t txn_manager_next_xid(void)
{
    g_txn_manager.current_xid++;
    if (g_txn_manager.current_xid <= 0) {
        /* 事务ID回绕，需要触发VACUUM */
        g_txn_manager.current_xid = TXN_ID_NONE + 1;
    }
    return g_txn_manager.current_xid;
}

int txn_manager_register_active(txn_id_t xid)
{
    if (g_txn_manager.active_count >= g_txn_manager.active_capacity) {
        return -1;  /* 活跃事务已满 */
    }

    /* 检查是否已存在 */
    for (int i = 0; i < g_txn_manager.active_count; i++) {
        if (g_txn_manager.active_xids[i] == xid) {
            return 0;  /* 已存在 */
        }
    }

    g_txn_manager.active_xids[g_txn_manager.active_count++] = xid;
    return 0;
}

void txn_manager_unregister_active(txn_id_t xid)
{
    for (int i = 0; i < g_txn_manager.active_count; i++) {
        if (g_txn_manager.active_xids[i] == xid) {
            /* 移除：将最后一个移到当前位置 */
            g_txn_manager.active_xids[i] =
                g_txn_manager.active_xids[g_txn_manager.active_count - 1];
            g_txn_manager.active_count--;
            return;
        }
    }
}

bool txn_manager_is_active(txn_id_t xid)
{
    for (int i = 0; i < g_txn_manager.active_count; i++) {
        if (g_txn_manager.active_xids[i] == xid) {
            return true;
        }
    }
    return false;
}

size_t txn_manager_active_count(void)
{
    return (size_t)g_txn_manager.active_count;
}

int txn_manager_get_snapshot(txn_xip_list_t *out_xip_list)
{
    if (out_xip_list == NULL) {
        return -1;
    }

    out_xip_list->count = g_txn_manager.active_count;
    out_xip_list->capacity = g_txn_manager.active_capacity;
    out_xip_list->entries = (txn_xip_entry_t *)malloc(
        sizeof(txn_xip_entry_t) * g_txn_manager.active_count);

    if (out_xip_list->entries == NULL) {
        return -1;
    }

    for (int i = 0; i < g_txn_manager.active_count; i++) {
        out_xip_list->entries[i].xid = g_txn_manager.active_xids[i];
        out_xip_list->entries[i].top_cid = TXN_CID_NONE;
        out_xip_list->entries[i].is_subxact = false;
    }

    return 0;
}

void txn_manager_release_snapshot(txn_xip_list_t *xip_list)
{
    if (xip_list != NULL && xip_list->entries != NULL) {
        free(xip_list->entries);
        xip_list->entries = NULL;
    }
}

/* ============================================================
 * 保存点栈实现
 * ============================================================ */

static int savepoint_stack_init(txn_savepoint_stack_t *stack)
{
    stack->capacity = TXN_MAX_SAVEPOINTS;
    stack->top = -1;
    stack->stack = (txn_savepoint_t *)calloc(
        stack->capacity, sizeof(txn_savepoint_t));
    return (stack->stack == NULL) ? -1 : 0;
}

static void savepoint_stack_destroy(txn_savepoint_stack_t *stack)
{
    if (stack->stack != NULL) {
        free(stack->stack);
        stack->stack = NULL;
    }
    stack->top = -1;
}

static int savepoint_push(txn_savepoint_stack_t *stack,
                          const char *name,
                          txn_cid_t cid,
                          txn_id_t subxact_id,
                          void *undo_ptr)
{
    if (stack->top >= stack->capacity - 1) {
        return -1;  /* 栈满 */
    }

    stack->top++;
    txn_savepoint_t *sp = &stack->stack[stack->top];
    strncpy(sp->name, name, sizeof(sp->name) - 1);
    sp->name[sizeof(sp->name) - 1] = '\0';
    sp->cid = cid;
    sp->subxact_id = subxact_id;
    sp->undo_ptr = undo_ptr;

    return 0;
}

static txn_savepoint_t *savepoint_find(txn_savepoint_stack_t *stack,
                                        const char *name)
{
    for (int i = 0; i <= stack->top; i++) {
        if (strcmp(stack->stack[i].name, name) == 0) {
            return &stack->stack[i];
        }
    }
    return NULL;
}

static int savepoint_release(txn_savepoint_stack_t *stack, const char *name)
{
    int found_idx = -1;
    for (int i = 0; i <= stack->top; i++) {
        if (strcmp(stack->stack[i].name, name) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx < 0) {
        return -1;  /* 未找到 */
    }

    /* 移除保存点：将之后的保存点前移 */
    for (int i = found_idx; i < stack->top; i++) {
        stack->stack[i] = stack->stack[i + 1];
    }
    stack->top--;

    return 0;
}

/* ============================================================
 * 事务生命周期实现
 * ============================================================ */

txn_context_t *txn_begin(void *db, txn_type_t txn_type)
{
    (void)db;  /* 未使用，简化实现 */

    txn_context_t *txn = (txn_context_t *)calloc(1, sizeof(txn_context_t));
    if (txn == NULL) {
        return NULL;
    }

    /* 分配事务ID */
    txn->xid = txn_manager_next_xid();
    txn->state = TXN_STATE_ACTIVE;
    txn->type = txn_type;
    txn->current_cid = TXN_CID_NONE;
    txn->top_cid = TXN_CID_NONE;
    txn->parent_xid = TXN_ID_NONE;
    txn->subxact_depth = 0;
    txn->undo_head = NULL;
    txn->error_msg[0] = '\0';

    /* 获取开始时间 */
    txn->start_time = (uint64_t)time(NULL) * 1000;

    /* 初始化保存点栈 */
    if (savepoint_stack_init(&txn->savepoints) != 0) {
        free(txn);
        return NULL;
    }

    /* 获取MVCC快照（XMIN/XMAX/XIP） */
    txn->snapshot_xmin = g_txn_manager.current_xid;
    txn->snapshot_xmax = g_txn_manager.current_xid;
    if (txn_manager_get_snapshot(&txn->snapshot_xip) != 0) {
        savepoint_stack_destroy(&txn->savepoints);
        free(txn);
        return NULL;
    }

    /* 注册为活跃事务 */
    if (txn_manager_register_active(txn->xid) != 0) {
        txn_manager_release_snapshot(&txn->snapshot_xip);
        savepoint_stack_destroy(&txn->savepoints);
        free(txn);
        return NULL;
    }

    return txn;
}

int txn_commit(txn_context_t *txn)
{
    if (txn == NULL || txn->state != TXN_STATE_ACTIVE) {
        return -1;
    }

    /* 检查是否有未释放的保存点 */
    if (txn->savepoints.top >= 0) {
        snprintf(txn->error_msg, sizeof(txn->error_msg),
                 "cannot commit with active savepoints");
        return -1;
    }

    /* 标记为已提交 */
    txn->state = TXN_STATE_COMMITTED;

    /* 从活跃列表移除 */
    txn_manager_unregister_active(txn->xid);

    /* 清理资源 */
    txn_manager_release_snapshot(&txn->snapshot_xip);
    savepoint_stack_destroy(&txn->savepoints);

    return 0;
}

int txn_rollback(txn_context_t *txn)
{
    if (txn == NULL || txn->state != TXN_STATE_ACTIVE) {
        return -1;
    }

    /* 标记为已回滚 */
    txn->state = TXN_STATE_ABORTED;

    /* 从活跃列表移除 */
    txn_manager_unregister_active(txn->xid);

    /* TODO: 应用Undo链回滚修改 */

    /* 清理资源 */
    txn_manager_release_snapshot(&txn->snapshot_xip);
    savepoint_stack_destroy(&txn->savepoints);

    return 0;
}

void txn_free(txn_context_t *txn)
{
    if (txn == NULL) {
        return;
    }

    if (txn->state == TXN_STATE_ACTIVE) {
        txn_manager_unregister_active(txn->xid);
    }

    txn_manager_release_snapshot(&txn->snapshot_xip);
    savepoint_stack_destroy(&txn->savepoints);
    free(txn);
}

/* ============================================================
 * 命令ID管理实现
 * ============================================================ */

txn_cid_t txn_next_cid(txn_context_t *txn)
{
    if (txn == NULL) {
        return TXN_CID_NONE;
    }
    txn->current_cid++;
    if (txn->current_cid == TXN_CID_NONE) {
        txn->current_cid = 1;  /* 避免0值 */
    }
    return txn->current_cid;
}

txn_cid_t txn_current_cid(const txn_context_t *txn)
{
    return (txn != NULL) ? txn->current_cid : TXN_CID_NONE;
}

/* ============================================================
 * 子事务实现
 * ============================================================ */

txn_id_t txn_begin_subxact(txn_context_t *txn)
{
    if (txn == NULL || txn->subxact_depth >= TXN_MAX_SUBTXN_DEPTH) {
        return TXN_ID_NONE;
    }

    /* 子事务使用新的事务ID */
    txn_id_t subxid = txn_manager_next_xid();

    /* 注册为活跃事务 */
    if (txn_manager_register_active(subxid) != 0) {
        return TXN_ID_NONE;
    }

    /* 保存当前状态 */
    txn->parent_xid = txn->xid;
    txn->xid = subxid;
    txn->subxact_depth++;
    txn->current_cid = TXN_CID_NONE;

    return subxid;
}

int txn_commit_subxact(txn_context_t *txn)
{
    if (txn == NULL || txn->subxact_depth <= 0) {
        return -1;
    }

    /* 注销子事务 */
    txn_manager_unregister_active(txn->xid);

    /* 恢复父事务 */
    txn->xid = txn->parent_xid;
    txn->subxact_depth--;
    txn->parent_xid = TXN_ID_NONE;

    return 0;
}

int txn_rollback_subxact(txn_context_t *txn, txn_cid_t cid)
{
    if (txn == NULL || txn->subxact_depth <= 0) {
        return -1;
    }

    /* TODO: 应用Undo链回滚到指定CID */

    /* 注销子事务 */
    txn_manager_unregister_active(txn->xid);

    /* 恢复父事务 */
    txn->xid = txn->parent_xid;
    txn->subxact_depth--;
    txn->parent_xid = TXN_ID_NONE;
    txn->current_cid = cid;

    return 0;
}

/* ============================================================
 * 保存点实现
 * ============================================================ */

int txn_savepoint(txn_context_t *txn, const char *name)
{
    if (txn == NULL || name == NULL) {
        return -1;
    }

    return savepoint_push(&txn->savepoints,
                          name,
                          txn->current_cid,
                          txn->subxact_depth > 0 ? txn->xid : TXN_ID_NONE,
                          txn->undo_head);
}

int txn_rollback_to_savepoint(txn_context_t *txn, const char *name)
{
    if (txn == NULL || name == NULL) {
        return -1;
    }

    txn_savepoint_t *sp = savepoint_find(&txn->savepoints, name);
    if (sp == NULL) {
        return -1;
    }

    /* TODO: 应用Undo链回滚到保存点 */

    /* 恢复CID */
    txn->current_cid = sp->cid;

    return 0;
}

int txn_release_savepoint(txn_context_t *txn, const char *name)
{
    if (txn == NULL || name == NULL) {
        return -1;
    }

    return savepoint_release(&txn->savepoints, name);
}

/* ============================================================
 * 事务查询实现
 * ============================================================ */

txn_id_t txn_get_id(const txn_context_t *txn)
{
    return (txn != NULL) ? txn->xid : TXN_ID_NONE;
}

txn_state_t txn_get_state(const txn_context_t *txn)
{
    return (txn != NULL) ? txn->state : TXN_STATE_ABORTED;
}

txn_type_t txn_get_type(const txn_context_t *txn)
{
    return (txn != NULL) ? txn->type : TXN_TYPE_READ_WRITE;
}

uint64_t txn_get_start_time(const txn_context_t *txn)
{
    return (txn != NULL) ? txn->start_time : 0;
}

bool txn_is_read_only(const txn_context_t *txn)
{
    return (txn != NULL) && (txn->type == TXN_TYPE_READ_ONLY);
}

txn_id_t txn_snapshot_xmin(const txn_context_t *txn)
{
    return (txn != NULL) ? txn->snapshot_xmin : TXN_ID_NONE;
}

txn_id_t txn_snapshot_xmax(const txn_context_t *txn)
{
    return (txn != NULL) ? txn->snapshot_xmax : TXN_ID_NONE;
}

/* ============================================================
 * MVCC 可见性判断
 * ============================================================ */

bool txn_version_visible(txn_context_t *txn,
                         txn_id_t xmin,
                         txn_id_t xmax,
                         txn_cid_t xmin_cid)
{
    if (txn == NULL) {
        return false;
    }

    /* 规则1: 自身事务创建的版本始终可见 */
    if (xmin == txn->xid) {
        return true;
    }

    /* 规则2: xmin 事务必须已提交 */
    if (xmin >= txn->snapshot_xmax) {
        /* 事务在快照创建后开始，未提交 */
        return false;
    }

    /* 检查xmin是否在活跃列表中 */
    for (int i = 0; i < txn->snapshot_xip.count; i++) {
        if (txn->snapshot_xip.entries[i].xid == xmin) {
            return false;  /* xmin事务仍在运行 */
        }
    }

    /* 规则3: 检查删除事务 */
    if (xmax != TXN_ID_NONE) {
        /* xmax 在活跃列表中，删除事务未提交 */
        for (int i = 0; i < txn->snapshot_xip.count; i++) {
            if (txn->snapshot_xip.entries[i].xid == xmax) {
                return true;  /* 删除事务未提交，版本仍可见 */
            }
        }
        /* xmax < snapshot_xmax 表示删除事务已提交 */
        if (xmax < txn->snapshot_xmax) {
            return false;  /* 已被删除 */
        }
    }

    return true;
}

/* ============================================================
 * 工具函数实现
 * ============================================================ */

bool txn_id_is_valid(txn_id_t xid)
{
    return xid != TXN_ID_NONE;
}

bool txn_needs_vacuum(void)
{
    return g_txn_manager.current_xid > TXN_ID_WRAP_THRESHOLD;
}

void txn_print_info(const txn_context_t *txn)
{
    if (txn == NULL) {
        printf("Transaction: NULL\n");
        return;
    }

    printf("Transaction %ld:\n", (long)txn->xid);
    printf("  State: %s\n", txn_state_name(txn->state));
    printf("  Type: %s\n", txn->type == TXN_TYPE_READ_ONLY ? "READ_ONLY" : "READ_WRITE");
    printf("  CID: %u\n", txn->current_cid);
    printf("  Snapshot: xmin=%ld, xmax=%ld, xip_count=%d\n",
           (long)txn->snapshot_xmin, (long)txn->snapshot_xmax,
           txn->snapshot_xip.count);
    printf("  Savepoints: %d\n", txn->savepoints.top + 1);
    printf("  Start time: %lu\n", (unsigned long)txn->start_time);
}

void txn_manager_print_status(void)
{
    printf("Transaction Manager Status:\n");
    printf("  Current XID: %ld\n", (long)g_txn_manager.current_xid);
    printf("  Active transactions: %d/%d\n",
           g_txn_manager.active_count,
           g_txn_manager.active_capacity);
    printf("  Active XIDs:");
    for (int i = 0; i < g_txn_manager.active_count; i++) {
        printf(" %ld", (long)g_txn_manager.active_xids[i]);
    }
    printf("\n");
}
