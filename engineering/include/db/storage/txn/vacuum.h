/**
 * @file vacuum.h
 * @brief VACUUM 垃圾回收头文件
 *
 * 实现 PostgreSQL 风格的 VACUUM 机制：
 * 1. 清理死亡元组（dead tuples）
 * 2. 冻结旧事务 ID（防止 wraparound）
 * 3. 更新 FSM（空闲空间映射）
 * 4. 清理不必要的索引项
 *
 * 参考：src/backend/commands/vacuum.c
 */
#ifndef DB_VACUUM_H
#define DB_VACUUM_H

#include <stdint.h>
#include <stdbool.h>
#include "db/storage/txn/mvcc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * VACUUM 配置
 * ======================================================================== */

/** VACUUM 模式 */
typedef enum VacuumMode_e {
    VACUUM_DEFAULT = 0,      /**< 普通 VACUUM */
    VACUUM_FULL = 1,        /**< 完整 VACUUM（排他锁） */
    VACUUM_FREEZE = 2,      /**< 冻结模式 */
    VACUUM_ANALYZE = 3      /**< VACUUM + ANALYZE */
} VacuumMode;

/** VACUUM 参数 */
typedef struct VacuumParams_s {
    uint32_t vacuum_cost_delay;       /**< VACUUM 成本延迟（毫秒） */
    uint32_t vacuum_cost_limit;       /**< VACUUM 成本限制 */
    uint32_t vacuum_page_speed;       /**< 页面处理速度（页/秒） */
    uint32_t freeze_min_age;          /**< 冻结最小年龄 */
    uint32_t freeze_max_age;          /**< 冻结最大年龄（触发强制 freeze） */
    uint32_t vacuum_scale_factor;     /**< VACUUM 缩放因子 */
    double   index_cleanup;           /**< 索引清理比例 */
    bool     is_parallel;             /**< 是否并行 */
    int      n_workers;              /**< 并行工作进程数 */
} VacuumParams;

/** 默认 VACUUM 参数 */
#define VACUUM_DEFAULT_COST_DELAY 0
#define VACUUM_DEFAULT_COST_LIMIT 200
#define VACUUM_DEFAULT_FREEZE_MIN_AGE 100
#define VACUUM_DEFAULT_FREEZE_MAX_AGE 200000000
#define VACUUM_DEFAULT_SCALE_FACTOR 0.2

/* ========================================================================
 * VACUUM 结果
 * ======================================================================== */

/** VACUUM 统计信息 */
typedef struct VacuumStats_s {
    uint64_t pages_visited;     /**< 访问的页面数 */
    uint64_t pages_removed;     /**< 移除的页面数 */
    uint64_t tuples_removed;    /**< 移除的元组数 */
    uint64_t tuples_frozen;     /**< 冻结的元组数 */
    uint32_t new_relfrozenxid;  /**< 新的 relfrozenxid */
    TransactionId new_oldest_xid; /**< 新的最老 XID */
    uint64_t elapsed_ms;         /**< 耗时（毫秒） */
} VacuumStats;

/** VACUUM 进度信息 */
typedef struct VacuumProgress_s {
    uint64_t total_pages;       /**< 总页面数 */
    uint64_t scanned_pages;     /**< 已扫描页面数 */
    uint32_t current_block;     /**< 当前处理的块号 */
    VacuumMode mode;            /**< VACUUM 模式 */
    bool phase;                 /**< 0=扫描，1=清理索引，2=更新FSM */
} VacuumProgress;

/* ========================================================================
 * VACUUM API
 * ======================================================================== */

/**
 * @brief 执行 VACUUM
 *
 * @param relfilenode 关系文件节点
 * @param params VACUUM 参数
 * @param stats 输出统计信息
 * @return 0 成功
 */
int vacuum_relation(uint32_t relfilenode,
                    const VacuumParams *params,
                    VacuumStats *stats);

/**
 * @brief 执行自动 VACUUM
 *
 * @param params VACUUM 参数
 * @return 0 成功
 */
int vacuum_autovacuum(const VacuumParams *params);

/**
 * @brief 执行数据库级 VACUUM
 *
 * @param params VACUUM 参数
 * @return 0 成功
 */
int vacuum_database(const VacuumParams *params);

/**
 * @brief 执行 Freeze（冻结所有表的旧元组）
 *
 * 当 XID 接近 wraparound 时调用
 *
 * @param freeze_age 冻结年龄阈值
 * @return 0 成功
 */
int vacuum_freeze_all(uint32_t freeze_age);

/**
 * @brief 获取 VACUUM 进度
 *
 * @param progress 输出进度信息
 * @return true 有进度信息
 */
bool vacuum_get_progress(VacuumProgress *progress);

/* ========================================================================
 * 死亡元组检测
 * ======================================================================== */

/**
 * @brief 检查元组是否死亡
 *
 * @param xmax 删除事务 ID
 * @param xmax_status xmax 状态
 * @param xmin_commit_time xmin 提交时间
 * @param cutoff_xid 截止 XID
 * @return true 元组已死亡
 */
bool vacuum_is_tuple_dead(TransactionId xmax,
                         heap_xmax_status_t xmax_status,
                         uint64_t xmin_commit_time,
                         TransactionId cutoff_xid);

/**
 * @brief 检查元组是否需要冻结
 *
 * @param xmin 插入事务 ID
 * @param freeze_min_age 冻结最小年龄
 * @param cutoff_xid 截止 XID
 * @return true 需要冻结
 */
bool vacuum_needs_freeze(TransactionId xmin,
                         uint32_t freeze_min_age,
                         TransactionId cutoff_xid);

/**
 * @brief 计算死亡元组数量
 *
 * @param relfilenode 关系文件节点
 * @return 死亡元组数
 */
uint64_t vacuum_count_dead_tuples(uint32_t relfilenode);

/* ========================================================================
 * Freeze 管理
 * ======================================================================== */

/** 全局最老 XID 阈值 */
extern TransactionId g_oldest_xmin;

/**
 * @brief 获取最老可见事务 ID
 *
 * 用于可见性判断和 VACUUM
 */
TransactionId vacuum_get_oldest_xmin(void);

/**
 * @brief 更新最老 XID 阈值
 *
 * @param oldest_xid 新的最老 XID
 */
void vacuum_update_oldest_xmin(TransactionId oldest_xid);

/**
 * @brief 检查是否需要执行紧急 Freeze
 *
 * @return true 需要紧急 Freeze
 */
bool vacuum_needs_emergency_freeze(void);

/**
 * @brief 获取 XID wraparound 距离
 *
 * @return 距离 wraparound 的事务数
 */
uint32_t vacuum_xid_wraparound_distance(void);

/* ========================================================================
 * VACUUM 调度
 * ======================================================================== */

/**
 * @brief 检查是否需要触发 autovacuum
 *
 * @param relfilenode 关系文件节点
 * @param tuples_changed 变化的元组数
 * @param total_tuples 总元组数
 * @return true 需要执行 VACUUM
 */
bool vacuum_needs_autovacuum(uint32_t relfilenode,
                            uint64_t tuples_changed,
                            uint64_t total_tuples);

/**
 * @brief 获取下一个需要 VACUUM 的关系
 *
 * @param relfilenode 输出关系文件节点
 * @return true 有待处理的关系
 */
bool vacuum_get_next_relation(uint32_t *relfilenode);

/**
 * @brief 标记关系需要 VACUUM
 *
 * @param relfilenode 关系文件节点
 * @param reason 原因（1=元组变化，2=死亡元组过多，3=freeze 警告）
 */
void vacuum_mark_relation_needs_vacuum(uint32_t relfilenode, int reason);

/**
 * @brief 初始化 VACUUM 子系统
 */
void vacuum_init(void);

/**
 * @brief 清理 VACUUM 子系统
 */
void vacuum_shutdown(void);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印 VACUUM 统计
 */
void vacuum_dump_stats(void);

/**
 * @brief 打印 Freeze 信息
 */
void vacuum_dump_freeze_info(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_VACUUM_H */
