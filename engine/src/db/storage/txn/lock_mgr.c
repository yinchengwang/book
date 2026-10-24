/**
 * @file lock_mgr.c
 * @brief 锁管理器实现
 *
 * 参考 PostgreSQL: src/backend/storage/lmgr/lock.c
 */

#include "db/storage/txn/lock_mgr.h"
#include "db/storage/txn/mvcc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/* ========================================================================
 * 前向声明
 * ======================================================================== */

static void deadlock_graph_add_waiter(TransactionId waiter, TransactionId holder, const LockTag *tag);

/* ========================================================================
 * 常量
 * ======================================================================== */

/** 锁表大小 */
#define LOCK_TABLE_SIZE 4096

/** 锁方法 ID */
#define DEFAULT_LOCKMETHOD 1
#define USER_LOCKMETHOD 2

/* ========================================================================
 * 锁兼容矩阵
 * ======================================================================== */

/**
 * 锁模式兼容矩阵
 * 行 = 已持有模式，列 = 请求模式
 * 1 = 兼容，0 = 不兼容
 */
static const uint8_t g_lock_compatibility[LOCKMODE_NUM][LOCKMODE_NUM] = {
    /*        No  AS  RS  RE  SE  SH  SRX EX  AX */
    /* NoLock */  {1,  1,  1,  1,  1,  1,  1,  1,  1},
    /* AccessShare */ {1,  1,  1,  1,  1,  1,  1,  1,  0},
    /* RowShare */    {1,  1,  1,  1,  1,  1,  1,  0,  0},
    /* RowExclusive */{1,  1,  1,  1,  1,  0,  0,  0,  0},
    /* ShareUpdateExcl*/{1, 1,  1,  1,  1,  0,  0,  0,  0},
    /* Share */        {1,  1,  1,  0,  0,  1,  0,  0,  0},
    /* ShareRowExcl */ {1,  1,  1,  0,  0,  0,  0,  0,  0},
    /* Exclusive */    {1,  1,  0,  0,  0,  0,  0,  0,  0},
    /* AccessExcl */   {1,  0,  0,  0,  0,  0,  0,  0,  0}
};

/** 锁级别权重 */
static const int g_lock_level[LOCKMODE_NUM] = {
    0,  /* NoLock */
    1,  /* AccessShareLock */
    2,  /* RowShareLock */
    3,  /* RowExclusiveLock */
    4,  /* ShareUpdateExclusiveLock */
    5,  /* ShareLock */
    6,  /* ShareRowExclusiveLock */
    7,  /* ExclusiveLock */
    8   /* AccessExclusiveLock */
};

/* ========================================================================
 * 全局状态
 * ======================================================================== */

/** 锁哈希表 */
typedef struct LockHashEntry_s {
    LockTag tag;
    LockMember holder;      /**< 当前持有者 */
    LockWaiter *waiters;  /**< 等待队列 */
    int waiter_count;
    struct LockHashEntry_s *next;
} LockHashEntry;

static LockHashEntry *g_lock_table[LOCK_TABLE_SIZE];
static atomic_uint_fast64_t g_lock_acquires = 0;
static atomic_uint_fast64_t g_lock_releases = 0;
static atomic_uint_fast64_t g_lock_waits = 0;
static atomic_uint_fast64_t g_lock_deadlocks = 0;
static atomic_uint_fast64_t g_lock_timeouts = 0;

/** GUC 参数 */
uint32_t g_lock_timeout = DEFAULT_LOCK_TIMEOUT;
uint32_t g_deadlock_timeout = DEFAULT_DEADLOCK_TIMEOUT;

/** 死锁图 */
static WfgNode g_wfg_nodes[256];
static int g_wfg_node_count = 0;
static WfgEdge g_wfg_edges[512];
static int g_wfg_edge_count = 0;

/** 是否初始化 */
static bool g_lock_mgr_initialized = false;

/* ========================================================================
 * 初始化
 * ======================================================================== */

/**
 * @brief 初始化锁管理器
 */
void lock_mgr_init(void)
{
    if (g_lock_mgr_initialized) {
        return;
    }

    memset(g_lock_table, 0, sizeof(g_lock_table));
    memset(g_wfg_nodes, 0, sizeof(g_wfg_nodes));
    memset(g_wfg_edges, 0, sizeof(g_wfg_edges));
    g_wfg_node_count = 0;
    g_wfg_edge_count = 0;

    g_lock_mgr_initialized = true;
}

/**
 * @brief 关闭锁管理器
 */
void lock_mgr_shutdown(void)
{
    if (!g_lock_mgr_initialized) {
        return;
    }

    /* 清理锁表 */
    for (int i = 0; i < LOCK_TABLE_SIZE; i++) {
        LockHashEntry *entry = g_lock_table[i];
        while (entry) {
            LockHashEntry *next = entry->next;
            /* 清理等待者 */
            LockWaiter *w = entry->waiters;
            while (w) {
                LockWaiter *w_next = w->next;
                free(w);
                w = w_next;
            }
            free(entry);
            entry = next;
        }
        g_lock_table[i] = NULL;
    }

    g_lock_mgr_initialized = false;
}

/* ========================================================================
 * 哈希计算
 * ======================================================================== */

/**
 * @brief 计算 LockTag 的哈希值
 */
static LOCKTAG_HASH compute_locktag_hash(const LockTag *tag)
{
    uint64_t hash = 0;
    hash ^= ((uint64_t)tag->locktag_type) << 0;
    hash ^= ((uint64_t)tag->locktag_lockmethodid) << 8;
    hash ^= ((uint64_t)tag->locktag_field1) << 16;
    hash ^= ((uint64_t)tag->locktag_field2) << 32;
    hash ^= ((uint64_t)tag->locktag_field3) << 48;
    hash ^= ((uint64_t)tag->locktag_field4) << 56;
    return hash;
}

/**
 * @brief 比较两个 LockTag 是否相等
 */
static bool locktag_equals(const LockTag *a, const LockTag *b)
{
    return a->locktag_type == b->locktag_type &&
           a->locktag_lockmethodid == b->locktag_lockmethodid &&
           a->locktag_field1 == b->locktag_field1 &&
           a->locktag_field2 == b->locktag_field2 &&
           a->locktag_field3 == b->locktag_field3 &&
           a->locktag_field4 == b->locktag_field4;
}

/**
 * @brief 查找或创建锁条目
 */
static LockHashEntry *find_or_create_lock(const LockTag *tag)
{
    LOCKTAG_HASH hash = compute_locktag_hash(tag);
    int index = hash % LOCK_TABLE_SIZE;

    LockHashEntry *entry = g_lock_table[index];
    while (entry) {
        if (locktag_equals(&entry->tag, tag)) {
            return entry;
        }
        entry = entry->next;
    }

    /* 创建新条目 */
    entry = (LockHashEntry *)calloc(1, sizeof(LockHashEntry));
    if (!entry) {
        return NULL;
    }
    entry->tag = *tag;
    entry->holder.xid = INVALID_TRANSACTION_ID;
    entry->holder.granted = false;
    entry->waiters = NULL;
    entry->waiter_count = 0;
    entry->next = g_lock_table[index];
    g_lock_table[index] = entry;

    return entry;
}

/* ========================================================================
 * 锁操作
 * ======================================================================== */

/**
 * @brief 检查锁模式是否兼容
 */
bool lock_mode_compatible(LockMode held, LockMode requested)
{
    if (held >= LOCKMODE_NUM || requested >= LOCKMODE_NUM) {
        return false;
    }
    return g_lock_compatibility[held][requested] == 1;
}

/**
 * @brief 获取锁级别
 */
int lock_mode_to_locklevel(LockMode mode)
{
    if (mode >= LOCKMODE_NUM) {
        return 0;
    }
    return g_lock_level[mode];
}

/**
 * @brief 获取锁
 */
bool lock_acquire(const LockTag *tag, TransactionId xid, LockMode mode, bool progress)
{
    if (!g_lock_mgr_initialized) {
        lock_mgr_init();
    }

    LockHashEntry *entry = find_or_create_lock(tag);
    if (!entry) {
        return false;
    }

    /* 检查是否可以立即获取 */
    if (entry->holder.xid == INVALID_TRANSACTION_ID) {
        /* 没有持有者，直接获取 */
        entry->holder.xid = xid;
        entry->holder.mode = mode;
        entry->holder.granted = true;
        atomic_fetch_add(&g_lock_acquires, 1);
        return true;
    }

    /* 检查兼容性 */
    if (entry->holder.xid == xid) {
        /* 同一事务，已持有锁，升级模式 */
        if (lock_mode_compatible(entry->holder.mode, mode)) {
            /* 兼容，直接返回（隐式持有） */
            return true;
        }
        /* 需要升级：暂时释放再获取更强的锁 */
        /* 简化：直接更新为更强的模式 */
        entry->holder.mode = mode;
        return true;
    }

    if (lock_mode_compatible(entry->holder.mode, mode)) {
        /* 兼容，添加到等待队列尾部 */
        /* 简化实现 */
        atomic_fetch_add(&g_lock_waits, 1);
        return true;  /* 简化：实际应阻塞等待 */
    }

    /* 不兼容，添加到等待队列 */
    LockWaiter *waiter = (LockWaiter *)calloc(1, sizeof(LockWaiter));
    if (!waiter) {
        return false;
    }
    waiter->xid = xid;
    waiter->mode = mode;
    waiter->waiting = true;
    waiter->next = entry->waiters;
    entry->waiters = waiter;
    entry->waiter_count++;
    atomic_fetch_add(&g_lock_waits, 1);

    /* 记录等待关系用于死锁检测 */
    deadlock_graph_add_waiter(xid, entry->holder.xid, tag);

    return true;  /* 简化：实际应阻塞等待 */
}

/**
 * @brief 释放锁
 */
bool lock_release(const LockTag *tag, TransactionId xid, LockMode mode)
{
    if (!g_lock_mgr_initialized) {
        return false;
    }

    LOCKTAG_HASH hash = compute_locktag_hash(tag);
    int index = hash % LOCK_TABLE_SIZE;

    LockHashEntry *entry = g_lock_table[index];
    while (entry) {
        if (locktag_equals(&entry->tag, tag)) {
            if (entry->holder.xid == xid) {
                entry->holder.xid = INVALID_TRANSACTION_ID;
                entry->holder.granted = false;

                /* 授予第一个等待者 */
                LockWaiter *waiter = entry->waiters;
                if (waiter) {
                    entry->waiters = waiter->next;
                    entry->holder.xid = waiter->xid;
                    entry->holder.mode = waiter->mode;
                    entry->holder.granted = true;
                    free(waiter);
                }
                atomic_fetch_add(&g_lock_releases, 1);
                return true;
            }
            return false;
        }
        entry = entry->next;
    }

    return false;
}

/**
 * @brief 尝试获取锁（非阻塞）
 */
bool lock_acquire_nowait(const LockTag *tag, TransactionId xid, LockMode mode)
{
    if (!g_lock_mgr_initialized) {
        lock_mgr_init();
    }

    LockHashEntry *entry = find_or_create_lock(tag);
    if (!entry) {
        return false;
    }

    if (entry->holder.xid == INVALID_TRANSACTION_ID) {
        entry->holder.xid = xid;
        entry->holder.mode = mode;
        entry->holder.granted = true;
        atomic_fetch_add(&g_lock_acquires, 1);
        return true;
    }

    if (entry->holder.xid == xid) {
        return true;  /* 已持有 */
    }

    if (lock_mode_compatible(entry->holder.mode, mode)) {
        return false;  /* 等待中，但返回 true 表示不阻塞 */
    }

    return false;
}

/**
 * @brief 检查锁是否被持有
 */
bool lock_is_held(const LockTag *tag, LockMode mode)
{
    if (!g_lock_mgr_initialized) {
        return false;
    }

    LOCKTAG_HASH hash = compute_locktag_hash(tag);
    int index = hash % LOCK_TABLE_SIZE;

    LockHashEntry *entry = g_lock_table[index];
    while (entry) {
        if (locktag_equals(&entry->tag, tag)) {
            if (entry->holder.xid != INVALID_TRANSACTION_ID) {
                return lock_mode_compatible(entry->holder.mode, mode);
            }
            return false;
        }
        entry = entry->next;
    }

    return false;
}

/**
 * @brief 检查锁是否被指定事务持有
 */
bool lock_is_held_by_xid(const LockTag *tag, TransactionId xid, LockMode mode)
{
    if (!g_lock_mgr_initialized) {
        return false;
    }

    LOCKTAG_HASH hash = compute_locktag_hash(tag);
    int index = hash % LOCK_TABLE_SIZE;

    LockHashEntry *entry = g_lock_table[index];
    while (entry) {
        if (locktag_equals(&entry->tag, tag)) {
            return entry->holder.xid == xid;
        }
        entry = entry->next;
    }

    return false;
}

/**
 * @brief 释放事务的所有锁
 */
int lock_release_all_for_xid(TransactionId xid)
{
    int released = 0;

    for (int i = 0; i < LOCK_TABLE_SIZE; i++) {
        LockHashEntry *prev = NULL;
        LockHashEntry *entry = g_lock_table[i];

        while (entry) {
            if (entry->holder.xid == xid) {
                entry->holder.xid = INVALID_TRANSACTION_ID;
                released++;

                /* 授予第一个等待者 */
                LockWaiter *waiter = entry->waiters;
                if (waiter) {
                    entry->waiters = waiter->next;
                    entry->holder.xid = waiter->xid;
                    entry->holder.mode = waiter->mode;
                    free(waiter);
                }

                atomic_fetch_add(&g_lock_releases, 1);
            }

            /* 清理空条目 */
            if (entry->holder.xid == INVALID_TRANSACTION_ID && entry->waiters == NULL) {
                if (prev) {
                    prev->next = entry->next;
                    free(entry);
                    entry = prev->next;
                } else {
                    g_lock_table[i] = entry->next;
                    free(entry);
                    entry = g_lock_table[i];
                }
                continue;
            }

            prev = entry;
            entry = entry->next;
        }
    }

    return released;
}

/**
 * @brief 更新锁超时
 */
void lock_set_timeout(uint32_t timeout_ms)
{
    g_lock_timeout = timeout_ms;
}

/* ========================================================================
 * 死锁检测
 * ======================================================================== */

/**
 * @brief 记录锁等待关系
 */
static void deadlock_graph_add_waiter(TransactionId waiter, TransactionId holder, const LockTag *tag)
{
    if (waiter == holder) {
        return;  /* 自己等待自己不可能 */
    }

    /* 添加节点 */
    bool waiter_found = false;
    bool holder_found = false;
    for (int i = 0; i < g_wfg_node_count; i++) {
        if (g_wfg_nodes[i].xid == waiter) waiter_found = true;
        if (g_wfg_nodes[i].xid == holder) holder_found = true;
    }

    if (!waiter_found && g_wfg_node_count < 256) {
        g_wfg_nodes[g_wfg_node_count++].xid = waiter;
    }
    if (!holder_found && g_wfg_node_count < 256) {
        g_wfg_nodes[g_wfg_node_count++].xid = holder;
    }

    /* 添加边：waiter -> holder */
    if (g_wfg_edge_count < 512) {
        g_wfg_edges[g_wfg_edge_count].waiter = waiter;
        g_wfg_edges[g_wfg_edge_count].holder = holder;
        g_wfg_edges[g_wfg_edge_count].tag = *tag;
        g_wfg_edge_count++;
    }
}

/**
 * @brief 清理死锁图
 */
void deadlock_graph_clear(void)
{
    memset(g_wfg_nodes, 0, sizeof(g_wfg_nodes));
    memset(g_wfg_edges, 0, sizeof(g_wfg_edges));
    g_wfg_node_count = 0;
    g_wfg_edge_count = 0;
}

/**
 * @brief 检测死锁（DFS 检测环）
 *
 * 简化实现：检测 Wait-For Graph 中是否存在环
 */
static bool detect_cycle(TransactionId start, TransactionId current,
                        bool *visited, TransactionId *on_stack)
{
    int idx;
    for (idx = 0; idx < g_wfg_node_count; idx++) {
        if (g_wfg_nodes[idx].xid == current) break;
    }
    if (idx >= g_wfg_node_count) return false;

    if (on_stack[idx]) {
        /* 发现环 */
        return true;
    }

    if (visited[idx]) {
        return false;
    }

    visited[idx] = true;
    on_stack[idx] = true;

    /* 遍历所有从 current 出发的边 */
    for (int i = 0; i < g_wfg_edge_count; i++) {
        if (g_wfg_edges[i].waiter == current) {
            if (detect_cycle(start, g_wfg_edges[i].holder, visited, on_stack)) {
                return true;
            }
        }
    }

    on_stack[idx] = false;
    return false;
}

/**
 * @brief 执行死锁检测
 */
bool deadlock_check(void)
{
    if (g_wfg_edge_count == 0) {
        return false;
    }

    /* 对每个节点执行 DFS 检测环 */
    for (int i = 0; i < g_wfg_node_count; i++) {
        bool visited[256] = {0};
        TransactionId on_stack[256] = {0};

        if (detect_cycle(g_wfg_nodes[i].xid, g_wfg_nodes[i].xid, visited, on_stack)) {
            atomic_fetch_add(&g_lock_deadlocks, 1);
            return true;
        }
    }

    return false;
}

/**
 * @brief 选择死锁中的牺牲者
 *
 * 选择最年轻的事务作为回滚对象
 */
TransactionId deadlock_select_victim(void)
{
    TransactionId youngest = 0;

    for (int i = 0; i < g_wfg_edge_count; i++) {
        if (g_wfg_edges[i].waiter > youngest) {
            youngest = g_wfg_edges[i].waiter;
        }
    }

    return youngest;
}

/* ========================================================================
 * 锁等待超时处理
 * ======================================================================== */

/**
 * @brief 检查锁等待是否超时
 */
bool lock_wait_timeout(const LockWaiter *waiter)
{
    (void)waiter;
    /* TODO: 记录等待开始时间并检查 */
    return false;
}

/**
 * @brief 获取锁等待的等待时间
 */
uint64_t lock_wait_elapsed(const LockWaiter *waiter)
{
    (void)waiter;
    return 0;
}

/* ========================================================================
 * 锁信息查询
 * ======================================================================== */

/**
 * @brief 获取指定标签的锁信息
 */
LockEntry *lock_get_entry(const LockTag *tag)
{
    LOCKTAG_HASH hash = compute_locktag_hash(tag);
    int index = hash % LOCK_TABLE_SIZE;

    LockHashEntry *entry = g_lock_table[index];
    while (entry) {
        if (locktag_equals(&entry->tag, tag)) {
            return (LockEntry *)entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * @brief 获取事务持有的锁数量
 */
int lock_count_for_xid(TransactionId xid)
{
    int count = 0;

    for (int i = 0; i < LOCK_TABLE_SIZE; i++) {
        LockHashEntry *entry = g_lock_table[i];
        while (entry) {
            if (entry->holder.xid == xid) {
                count++;
            }
            entry = entry->next;
        }
    }

    return count;
}

/* ========================================================================
 * 锁统计
 * ======================================================================== */

/**
 * @brief 获取锁统计
 */
void lock_get_stats(LockStats *stats)
{
    if (!stats) return;

    stats->lock_acquires = atomic_load(&g_lock_acquires);
    stats->lock_releases = atomic_load(&g_lock_releases);
    stats->lock_waits = atomic_load(&g_lock_waits);
    stats->lock_deadlocks = atomic_load(&g_lock_deadlocks);
    stats->lock_timeouts = atomic_load(&g_lock_timeouts);
}

/**
 * @brief 重置锁统计
 */
void lock_reset_stats(void)
{
    atomic_store(&g_lock_acquires, 0);
    atomic_store(&g_lock_releases, 0);
    atomic_store(&g_lock_waits, 0);
    atomic_store(&g_lock_deadlocks, 0);
    atomic_store(&g_lock_timeouts, 0);
}

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印锁状态
 */
void lock_dump(void)
{
    printf("=== Lock Manager Status ===\n");
    printf("Lock Timeout: %u ms\n", g_lock_timeout);
    printf("Deadlock Timeout: %u ms\n", g_deadlock_timeout);

    int total_locks = 0;
    int total_waiters = 0;

    for (int i = 0; i < LOCK_TABLE_SIZE; i++) {
        LockHashEntry *entry = g_lock_table[i];
        while (entry) {
            if (entry->holder.xid != INVALID_TRANSACTION_ID) {
                total_locks++;
                printf("Lock: rel=%u, block=%u, offset=%u, holder=%u, mode=%d\n",
                       entry->tag.locktag_field1,
                       entry->tag.locktag_field2,
                       entry->tag.locktag_field3,
                       entry->holder.xid,
                       entry->holder.mode);
            }

            LockWaiter *w = entry->waiters;
            while (w) {
                total_waiters++;
                printf("  Waiter: xid=%u, mode=%d\n", w->xid, w->mode);
                w = w->next;
            }

            entry = entry->next;
        }
    }

    printf("Total Locks: %d\n", total_locks);
    printf("Total Waiters: %d\n", total_waiters);
}

/**
 * @brief 打印死锁图
 */
void deadlock_graph_dump(void)
{
    printf("=== Deadlock Graph ===\n");
    printf("Nodes: %d\n", g_wfg_node_count);
    for (int i = 0; i < g_wfg_node_count; i++) {
        printf("  [%d] xid=%u\n", i, g_wfg_nodes[i].xid);
    }

    printf("Edges: %d\n", g_wfg_edge_count);
    for (int i = 0; i < g_wfg_edge_count; i++) {
        printf("  [%d] waiter=%u -> holder=%u\n",
               i, g_wfg_edges[i].waiter, g_wfg_edges[i].holder);
    }
}
