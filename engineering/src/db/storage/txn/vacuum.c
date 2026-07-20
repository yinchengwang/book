/**
 * @file vacuum.c
 * @brief VACUUM 垃圾回收实现
 *
 * 参考 PostgreSQL: src/backend/commands/vacuum.c
 */

#include "db/storage/txn/vacuum.h"
#include "db/storage/txn/mvcc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================================================================
 * 全局状态
 * ======================================================================== */

/** 默认 VACUUM 参数 */
static VacuumParams g_vacuum_params = {
    .vacuum_cost_delay = VACUUM_DEFAULT_COST_DELAY,
    .vacuum_cost_limit = VACUUM_DEFAULT_COST_LIMIT,
    .vacuum_page_speed = 1000,
    .freeze_min_age = VACUUM_DEFAULT_FREEZE_MIN_AGE,
    .freeze_max_age = VACUUM_DEFAULT_FREEZE_MAX_AGE,
    .vacuum_scale_factor = VACUUM_DEFAULT_SCALE_FACTOR,
    .index_cleanup = 1.0,
    .is_parallel = false,
    .n_workers = 0
};

/** 全局最老 XID 阈值 */
TransactionId g_oldest_xmin = TRANSACTION_ID_OFFSET;

/** 需要 VACUUM 的关系列表（简化实现） */
#define VACUUM_PENDING_MAX 256
static uint32_t g_pending_vacuum[VACUUM_PENDING_MAX];
static int g_pending_count = 0;
static int g_pending_head = 0;

/** VACUUM 统计 */
static VacuumStats g_last_vacuum_stats;
static VacuumProgress g_vacuum_progress;

/** VACUUM 子系统是否初始化 */
static bool g_vacuum_initialized = false;

/* ========================================================================
 * 初始化
 * ======================================================================== */

/**
 * @brief 初始化 VACUUM 子系统
 */
void vacuum_init(void)
{
    if (g_vacuum_initialized) {
        return;
    }

    g_oldest_xmin = TRANSACTION_ID_OFFSET;
    g_pending_count = 0;
    g_pending_head = 0;

    memset(&g_last_vacuum_stats, 0, sizeof(g_last_vacuum_stats));
    memset(&g_vacuum_progress, 0, sizeof(g_vacuum_progress));

    g_vacuum_initialized = true;
}

/**
 * @brief 清理 VACUUM 子系统
 */
void vacuum_shutdown(void)
{
    if (!g_vacuum_initialized) {
        return;
    }

    g_pending_count = 0;
    g_pending_head = 0;
    g_vacuum_initialized = false;
}

/* ========================================================================
 * VACUUM 执行
 * ======================================================================== */

/**
 * @brief 获取当前时间（毫秒）
 */
static uint64_t get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * @brief 执行单表 VACUUM
 *
 * 简化实现：遍历所有页面，检测和清理死亡元组
 */
int vacuum_relation(uint32_t relfilenode,
                   const VacuumParams *params,
                   VacuumStats *stats)
{
    if (!g_vacuum_initialized) {
        vacuum_init();
    }

    VacuumParams p = params ? *params : g_vacuum_params;
    uint64_t start_time = get_current_time_ms();

    VacuumStats s = {0};

    /* 初始化进度 */
    g_vacuum_progress.mode = VACUUM_DEFAULT;
    g_vacuum_progress.scanned_pages = 0;
    g_vacuum_progress.total_pages = 100;  /* TODO: 从实际关系获取 */

    /* Phase 1: 扫描页面，标记死亡元组 */
    g_vacuum_progress.phase = 0;

    /* TODO: 遍历关系的每个页面
     * for (page = 0; page < num_pages; page++) {
     *     // 检查元组
     *     // 标记死亡元组
     *     // 计算是否可以冻结
     *     s.pages_visited++;
     *     s.tuples_removed += dead_tuples;
     *     s.tuples_frozen += frozen_tuples;
     * }
     */

    /* Phase 2: 清理索引 */
    g_vacuum_progress.phase = 1;

    /* TODO: 遍历相关索引，清理指向死亡元组的索引项
     * vacuum_index_cleanup(relfilenode);
     */

    /* Phase 3: 更新 FSM */
    g_vacuum_progress.phase = 2;

    /* TODO: 更新空闲空间映射
     * vacuum_update_fsm(relfilenode);
     */

    /* 结束 */
    s.elapsed_ms = get_current_time_ms() - start_time;
    s.new_relfrozenxid = 0;
    s.new_oldest_xid = vacuum_get_oldest_xmin();

    if (stats) {
        *stats = s;
    }
    g_last_vacuum_stats = s;

    return 0;
}

/**
 * @brief 执行自动 VACUUM
 */
int vacuum_autovacuum(const VacuumParams *params)
{
    uint32_t relfilenode;

    while (vacuum_get_next_relation(&relfilenode)) {
        vacuum_relation(relfilenode, params, NULL);
    }

    return 0;
}

/**
 * @brief 执行数据库级 VACUUM
 */
int vacuum_database(const VacuumParams *params)
{
    VacuumParams p = params ? *params : g_vacuum_params;

    /* TODO: 获取数据库中所有关系
     * for (each relation in database) {
     *     vacuum_relation(relfilenode, &p, NULL);
     * }
     */

    return 0;
}

/**
 * @brief 执行 Freeze
 */
int vacuum_freeze_all(uint32_t freeze_age)
{
    VacuumParams p = g_vacuum_params;
    p.freeze_min_age = freeze_age;

    /* 遍历所有关系，冻结旧元组 */
    /* TODO: 实现 freeze 逻辑 */

    return 0;
}

/**
 * @brief 获取 VACUUM 进度
 */
bool vacuum_get_progress(VacuumProgress *progress)
{
    if (!g_vacuum_initialized || g_vacuum_progress.total_pages == 0) {
        return false;
    }

    if (progress) {
        *progress = g_vacuum_progress;
    }

    return true;
}

/* ========================================================================
 * 死亡元组检测
 * ======================================================================== */

/**
 * @brief 检查元组是否死亡
 */
bool vacuum_is_tuple_dead(TransactionId xmax,
                         heap_xmax_status_t xmax_status,
                         uint64_t xmin_commit_time,
                         TransactionId cutoff_xid)
{
    if (!mvcc_is_xid_valid(xmax)) {
        return false;
    }

    if (xmax_status == HEAP_XMAX_INVALID) {
        return false;
    }

    /* xmax 已提交且不是 LOCK_ONLY */
    if (xmax_status == HEAP_XMAX_COMMITTED) {
        return true;
    }

    /* 如果 xmax 在 cutoff 之前已提交，也是死亡元组 */
    if (mvcc_xid_before(xmax, cutoff_xid)) {
        transaction_status_t status = mvcc_get_status(xmax);
        if (status == TRANSACTION_STATUS_COMMITTED) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 检查元组是否需要冻结
 */
bool vacuum_needs_freeze(TransactionId xmin,
                         uint32_t freeze_min_age,
                         TransactionId cutoff_xid)
{
    if (!mvcc_is_xid_valid(xmin)) {
        return false;
    }

    if (mvcc_is_xid_frozen(xmin)) {
        return false;
    }

    /* 计算 xmin 的年龄 */
    TransactionId current_xid = mvcc_get_current_xid();
    TransactionId xid_age;

    if (mvcc_xid_before(xmin, current_xid)) {
        xid_age = current_xid - xmin;
    } else {
        /* wraparound */
        xid_age = (MAX_TRANSACTION_ID - xmin) + current_xid;
    }

    /* 检查是否超过 freeze_min_age */
    if (xid_age < freeze_min_age) {
        return false;
    }

    /* 检查是否在 cutoff 之前 */
    if (mvcc_xid_before(xmin, cutoff_xid)) {
        return true;
    }

    return false;
}

/**
 * @brief 计算死亡元组数量（简化实现）
 */
uint64_t vacuum_count_dead_tuples(uint32_t relfilenode)
{
    (void)relfilenode;
    /* TODO: 实际遍历页面计算 */
    return 0;
}

/* ========================================================================
 * Freeze 管理
 * ======================================================================== */

/**
 * @brief 获取最老可见事务 ID
 */
TransactionId vacuum_get_oldest_xmin(void)
{
    return g_oldest_xmin;
}

/**
 * @brief 更新最老 XID 阈值
 */
void vacuum_update_oldest_xmin(TransactionId oldest_xid)
{
    if (mvcc_xid_before(oldest_xid, g_oldest_xmin)) {
        g_oldest_xmin = oldest_xid;
    }
}

/**
 * @brief 检查是否需要紧急 Freeze
 */
bool vacuum_needs_emergency_freeze(void)
{
    return vacuum_xid_wraparound_distance() < 10000000;  /* 1000万 */
}

/**
 * @brief 获取 XID wraparound 距离
 */
uint32_t vacuum_xid_wraparound_distance(void)
{
    TransactionId current = mvcc_get_current_xid();
    return (uint32_t)(MAX_TRANSACTION_ID - current);
}

/* ========================================================================
 * VACUUM 调度
 * ======================================================================== */

/**
 * @brief 检查是否需要触发 autovacuum
 */
bool vacuum_needs_autovacuum(uint32_t relfilenode,
                            uint64_t tuples_changed,
                            uint64_t total_tuples)
{
    (void)relfilenode;

    VacuumParams p = g_vacuum_params;

    /* 使用缩放因子计算阈值 */
    double threshold = total_tuples * p.vacuum_scale_factor;

    return tuples_changed > threshold;
}

/**
 * @brief 获取下一个需要 VACUUM 的关系
 */
bool vacuum_get_next_relation(uint32_t *relfilenode)
{
    if (g_pending_count <= 0) {
        return false;
    }

    *relfilenode = g_pending_vacuum[g_pending_head];
    g_pending_head = (g_pending_head + 1) % VACUUM_PENDING_MAX;
    g_pending_count--;

    return true;
}

/**
 * @brief 标记关系需要 VACUUM
 */
void vacuum_mark_relation_needs_vacuum(uint32_t relfilenode, int reason)
{
    (void)reason;

    if (g_pending_count >= VACUUM_PENDING_MAX) {
        return;  /* 队列满 */
    }

    /* 检查是否已在队列中 */
    for (int i = 0; i < g_pending_count; i++) {
        int idx = (g_pending_head + i) % VACUUM_PENDING_MAX;
        if (g_pending_vacuum[idx] == relfilenode) {
            return;
        }
    }

    int tail = (g_pending_head + g_pending_count) % VACUUM_PENDING_MAX;
    g_pending_vacuum[tail] = relfilenode;
    g_pending_count++;
}

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印 VACUUM 统计
 */
void vacuum_dump_stats(void)
{
    printf("=== VACUUM Statistics ===\n");
    printf("Pages Visited: %lu\n", g_last_vacuum_stats.pages_visited);
    printf("Pages Removed: %lu\n", g_last_vacuum_stats.pages_removed);
    printf("Tuples Removed: %lu\n", g_last_vacuum_stats.tuples_removed);
    printf("Tuples Frozen: %lu\n", g_last_vacuum_stats.tuples_frozen);
    printf("Elapsed Time: %lu ms\n", g_last_vacuum_stats.elapsed_ms);
    printf("Pending Relations: %d\n", g_pending_count);
}

/**
 * @brief 打印 Freeze 信息
 */
void vacuum_dump_freeze_info(void)
{
    printf("=== Freeze Information ===\n");
    printf("Oldest Xmin: %u\n", vacuum_get_oldest_xmin());
    printf("Current XID: %u\n", mvcc_get_current_xid());
    printf("Wraparound Distance: %u\n", vacuum_xid_wraparound_distance());
    printf("Emergency Freeze Needed: %s\n",
           vacuum_needs_emergency_freeze() ? "YES" : "NO");
}
