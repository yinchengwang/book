/**
 * @file mvcc.h
 * @brief MVCC 多版本并发控制头文件
 *
 * MVCC 实现参考 PostgreSQL 风格：
 * - 元组级版本链
 * - 事务 ID 管理
 * - ReadView 可见性判断
 * - 支持 RC/SI/SSI 隔离级别
 *
 * 核心数据结构：
 * - TupleHeader_MVCC: 元组头扩展
 * - TransactionId: 事务 ID
 * - ReadView: 读视图
 * - TransactionState: 事务状态
 */
#ifndef DB_MVCC_H
#define DB_MVCC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Windows 兼容性: 使用 __declspec(thread) 或 pthread TLS */
#ifdef _WIN32
    #include <windows.h>
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL __thread
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

/** 事务上下文（不透明） */
typedef struct TxnContext_s TxnContext;

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 无效事务 ID */
#define INVALID_TRANSACTION_ID 0

/** 事务 ID 最大值（用于 wraparound 检测） */
#define MAX_TRANSACTION_ID UINT32_MAX

/** 最大的活跃事务数 */
#define MAX_ACTIVE_TRANSACTIONS 1024

/** Frozen 事务 ID（表示旧元组不再需要事务信息） */
#define FROZEN_TRANSACTION_ID 2

/** TransactionId 基准偏移量 */
#define TRANSACTION_ID_OFFSET 3

/** 隔离级别枚举 */
typedef enum {
    ISOLATION_READ_COMMITTED = 0,   /**< 读已提交 */
    ISOLATION_REPEATABLE_READ = 1,   /**< 可重复读 */
    ISOLATION_SERIALIZABLE = 2      /**< 可串行化 */
} isolation_level_t;

/** 事务状态枚举 */
typedef enum {
    TRANSACTION_STATUS_IN_PROGRESS = 0,   /**< 进行中 */
    TRANSACTION_STATUS_COMMITTED = 1,      /**< 已提交 */
    TRANSACTION_STATUS_ABORTED = 2,        /**< 已回滚 */
    TRANSACTION_STATUS_PREPARED = 3        /**< 已预提交（两阶段提交） */
} transaction_status_t;

/** 元组 xmin/xmax 状态 */
typedef enum {
    HEAP_XMIN_COMMITTED = 0,      /**< xmin 已提交 */
    HEAP_XMIN_INVALID = 1,        /**< xmin 无效 */
    HEAP_XMIN_ABORTED = 2,        /**< xmin 已回滚 */
    HEAP_XMIN_FROZEN = 3          /**< xmin 已冻结 */
} heap_xmin_status_t;

typedef enum {
    HEAP_XMAX_COMMITTED = 0,      /**< xmax 已提交 */
    HEAP_XMAX_INVALID = 1,       /**< xmax 无效 */
    HEAP_XMAX_ABORTED = 2,        /**< xmax 已回滚 */
    HEAP_XMAX_LOCK_ONLY = 3      /**< xmax 仅为锁 */
} heap_xmax_status_t;

/* ========================================================================
 * TransactionId 管理
 * ======================================================================== */

/**
 * @brief 事务 ID 类型
 */
typedef uint32_t TransactionId;

/**
 * @brief 获取当前事务 ID
 * @return 当前事务 ID
 */
TransactionId mvcc_get_current_xid(void);

/**
 * @brief 分配新的事务 ID
 * @return 新事务 ID
 */
TransactionId mvcc_alloc_xid(void);

/**
 * @brief 检查事务 ID 是否有效
 * @param xid 事务 ID
 * @return true 有效
 */
bool mvcc_is_xid_valid(TransactionId xid);

/**
 * @brief 检查是否是 Frozen 事务 ID
 * @param xid 事务 ID
 * @return true 是 Frozen
 */
bool mvcc_is_xid_frozen(TransactionId xid);

/**
 * @brief 检查两个事务 ID 的顺序
 * @param xid1 事务 ID 1
 * @param xid2 事务 ID 2
 * @return true xid1 < xid2
 */
bool mvcc_xid_before(TransactionId xid1, TransactionId xid2);

/**
 * @brief 检查事务 ID 是否在某个点之后
 * @param xid 事务 ID
 * @param before_xid 参考点
 * @return true xid 在 before_xid 之后
 */
bool mvcc_xid_after(TransactionId xid, TransactionId before_xid);

/**
 * @brief 获取 XID horizon（最老的可见事务边界）
 * @return XID horizon
 */
TransactionId mvcc_get_xid_horizon(void);

/* ========================================================================
 * 事务状态管理
 * ======================================================================== */

/**
 * @brief 标记事务为已提交
 * @param xid 事务 ID
 */
void mvcc_mark_committed(TransactionId xid);

/**
 * @brief 标记事务为已回滚
 * @param xid 事务 ID
 */
void mvcc_mark_aborted(TransactionId xid);

/**
 * @brief 标记事务为已预提交（两阶段提交 PREPARE）
 * @param xid 事务 ID
 */
void mvcc_mark_prepared(TransactionId xid);

/**
 * @brief 获取事务状态
 * @param xid 事务 ID
 * @return 事务状态
 */
transaction_status_t mvcc_get_status(TransactionId xid);

/**
 * @brief 检查事务是否在指定时间点已提交
 * @param xid 事务 ID
 * @param horizon 参考时间点
 * @return true 已提交
 */
bool mvcc_is_committed_at(TransactionId xid, TransactionId horizon);

/* ========================================================================
 * ReadView（读视图）
 * ======================================================================== */

/**
 * @brief ReadView 结构
 *
 * 用于判断元组在某个时间点是否对事务可见
 */
typedef struct ReadView_s {
    TransactionId xmin;              /**< 最小活跃事务 ID */
    TransactionId xmax;               /**< 最大已分配事务 ID */
    TransactionId *active_txs;         /**< 活跃事务 ID 数组 */
    int32_t num_active;              /**< 活跃事务数 */
    TransactionId snapshot_xmax;       /**< 快照的最大 XID */
    isolation_level_t isolation;      /**< 隔离级别 */
} ReadView;

/**
 * @brief 创建 ReadView
 * @param isolation 隔离级别
 * @return ReadView 指针，失败返回 NULL
 */
ReadView *mvcc_create_readview(isolation_level_t isolation);

/**
 * @brief 释放 ReadView
 * @param rv ReadView 指针
 */
void mvcc_destroy_readview(ReadView *rv);

/**
 * @brief 检查事务 ID 是否在活跃事务列表中
 * @param rv ReadView
 * @param xid 事务 ID
 * @return true 如果在活跃列表中
 */
bool is_active_in_readview(const ReadView *rv, TransactionId xid);

/**
 * @brief 获取当前事务的 ReadView
 * @return ReadView 指针
 */
ReadView *mvcc_get_current_readview(void);

/**
 * @brief 设置当前事务的 ReadView
 * @param rv ReadView 指针
 */
void mvcc_set_current_readview(ReadView *rv);

/* ========================================================================
 * 元组可见性判断
 * ======================================================================== */

/**
 * @brief 事务 ID 组合状态
 */
typedef struct {
    TransactionId xmin;              /**< 插入事务 ID */
    TransactionId xmax;              /**< 删除/更新事务 ID */
    heap_xmin_status_t xmin_status;  /**< xmin 状态 */
    heap_xmax_status_t xmax_status;  /**< xmax 状态 */
    bool t_ctid_is_self;            /**< ctid 是否指向自己 */
    TransactionId t_ctid;            /**< 指向新版本的元组指针 */
} HeapTupleFields;

/**
 * @brief 判断元组对 ReadView 是否可见
 *
 * @param rv ReadView
 * @param tuple_fields 元组的事务字段
 * @return true 可见
 */
bool mvcc_tuple_visible(const ReadView *rv, const HeapTupleFields *tuple_fields);

/**
 * @brief 获取元组最新版本
 *
 * @param rv ReadView
 * @param tuple_fields 当前元组的事务字段
 * @return 元组最新版本的事务字段，如果当前版本就是最新则返回 NULL
 */
HeapTupleFields *mvcc_get_latest_version(const ReadView *rv,
                                         const HeapTupleFields *tuple_fields);

/* ========================================================================
 * 元组版本链操作
 * ======================================================================== */

/**
 * @brief 创建一个新版本的元组（用于 UPDATE）
 *
 * @param old_tuple_fields 旧元组的事务字段
 * @param new_xid 新版本的事务 ID
 * @return 新版本元组的事务字段
 */
HeapTupleFields *mvcc_create_new_version(const HeapTupleFields *old_tuple_fields,
                                          TransactionId new_xid);

/**
 * @brief 标记元组为已删除（设置 xmax）
 *
 * @param tuple_fields 元组的事务字段
 * @param xid 删除事务 ID
 * @param status xmax 状态
 */
void mvcc_mark_deleted(HeapTupleFields *tuple_fields,
                       TransactionId xid,
                       heap_xmax_status_t status);

/**
 * @brief 冻结元组（用于 VACUUM）
 *
 * @param tuple_fields 元组的事务字段
 */
void mvcc_freeze_tuple(HeapTupleFields *tuple_fields);

/* ========================================================================
 * REPEATABLE READ 支持
 * ======================================================================== */

/**
 * @brief REPEATABLE READ 隔离级别下检查元组是否可见
 *
 * @param rv ReadView
 * @param tuple_fields 元组的事务字段
 * @param current_xid 当前事务 ID
 * @return true 可见
 */
bool mvcc_repeatable_read_visible(const ReadView *rv,
                                   const HeapTupleFields *tuple_fields,
                                   TransactionId current_xid);

/**
 * @brief 检查 RR 模式下是否需要报告序列化冲突
 *
 * @param rv ReadView
 * @param xid 可能冲突的事务 ID
 * @return true 存在冲突
 */
bool mvcc_repeatable_read_has_conflict(const ReadView *rv, TransactionId xid);

/* ========================================================================
 * SERIALIZABLE SSI 支持
 * ======================================================================== */

/**
 * @brief SERIALIZABLE 隔离级别下检查元组是否可见
 *
 * @param rv ReadView
 * @param tuple_fields 元组的事务字段
 * @param current_xid 当前事务 ID
 * @return true 可见
 */
bool mvcc_serializable_visible(const ReadView *rv,
                              const HeapTupleFields *tuple_fields,
                              TransactionId current_xid);

/**
 * @brief 检查序列化隔离级别下是否发生序列化异常
 *
 * @return true 正常可以继续，false 发生序列化冲突需要重试
 */
bool mvcc_serializable_check_anomaly(void);

/* ========================================================================
 * MVCC 与 WAL 集成
 * ======================================================================== */

/**
 * @brief 初始化 MVCC WAL 集成
 */
void mvcc_wal_init(void);

/**
 * @brief 关闭 MVCC WAL 集成
 */
void mvcc_wal_shutdown(void);

/**
 * @brief 分配新的提交序列号
 * @return 新的 CSN
 */
uint64_t mvcc_alloc_csn(void);

/**
 * @brief 检查 CSN 是否已提交
 * @param csn 提交序列号
 * @return true 已提交
 */
bool mvcc_csn_committed(uint64_t csn);

/**
 * @brief 获取元组的提交序列号
 * @param xid 事务 ID
 * @return CSN，未知返回 0
 */
uint64_t mvcc_get_tuple_csn(TransactionId xid);

/**
 * @brief 使用 CSN 判断元组是否可见
 */
bool mvcc_tuple_visible_by_csn(const ReadView *rv,
                               const HeapTupleFields *tuple_fields,
                               TransactionId current_xid,
                               uint64_t csn);

/* ========================================================================
 * MVCC 初始化和清理
 * ======================================================================== */

/**
 * @brief 初始化 MVCC 子系统
 * @return 0 成功，-1 失败
 */
int mvcc_init(void);

/**
 * @brief 关闭 MVCC 子系统
 */
void mvcc_shutdown(void);

/**
 * @brief 重置 MVCC 状态（用于测试）
 */
void mvcc_reset(void);

/* ========================================================================
 * GUC 参数
 * ======================================================================== */

/** 当前隔离级别 */
extern isolation_level_t mvcc_current_isolation;

/** XID wraparound 安全阈值 */
extern uint32_t mvcc_xid_wraparound_threshold;

/**
 * @brief 设置隔离级别
 * @param level 隔离级别
 */
void mvcc_set_isolation_level(isolation_level_t level);

/**
 * @brief 获取当前隔离级别
 * @return 隔离级别
 */
isolation_level_t mvcc_get_isolation_level(void);

/* ========================================================================
 * VACUUM 辅助
 * ======================================================================== */

/**
 * @brief 获取需要 freeze 的最老事务 ID
 * @return freeze 阈值
 */
TransactionId mvcc_get_oldest_xid_limit(void);

/**
 * @brief 检查是否需要执行 VACUUM freeze
 * @return true 需要
 */
bool mvcc_needs_vacuum(void);

/**
 * @brief 执行 Freeze 处理
 */
void mvcc_do_freeze(void);

/* ========================================================================
 * 调试和统计
 * ======================================================================== */

/** MVCC 统计信息 */
typedef struct MvccStats_s {
    uint64_t xid_allocations;       /**< XID 分配次数 */
    uint64_t xid_wraparounds;        /**< XID wraparound 次数 */
    uint64_t readview_creates;       /**< ReadView 创建次数 */
    uint64_t visibility_checks;      /**< 可见性检查次数 */
    uint64_t version_follows;        /**< 版本链遍历次数 */
    uint32_t active_transactions;    /**< 当前活跃事务数 */
} MvccStats;

/**
 * @brief 获取 MVCC 统计信息
 * @param stats 输出统计信息
 */
void mvcc_get_stats(MvccStats *stats);

/**
 * @brief 重置 MVCC 统计
 */
void mvcc_reset_stats(void);

/**
 * @brief 打印 MVCC 状态（调试用）
 */
void mvcc_dump_status(void);

/* ========================================================================
 * 事务上下文 API（REPEATABLE READ 支持）
 * ======================================================================== */

/**
 * @brief 刷新 ReadView（RC 级别）
 *
 * RC 级别每个语句需要刷新 ReadView
 * RR/SSI 级别不执行刷新
 */
void txn_refresh_readview(void);

/**
 * @brief 获取当前隔离级别
 * @return 隔离级别
 */
isolation_level_t txn_get_isolation_level(void);

/**
 * @brief 检查是否为快照隔离级别（RR/SERIALIZABLE）
 * @return true 如果是 RR 或 SERIALIZABLE
 */
bool txn_is_snapshot_isolation(void);

/**
 * @brief 从字符串解析隔离级别
 * @param level_str 隔离级别字符串
 * @return 隔离级别，失败返回 -1
 */
isolation_level_t txn_parse_isolation_level(const char *level_str);

/**
 * @brief 获取隔离级别的字符串表示
 * @param level 隔离级别
 * @return 字符串
 */
const char *txn_isolation_level_name(isolation_level_t level);

/**
 * @brief 获取当前事务上下文
 * @return 事务上下文指针
 */
TxnContext *txn_get_context(void);

/**
 * @brief 获取当前事务 ID
 * @return 事务 ID
 */
TransactionId txn_current_xid(void);

/**
 * @brief 检查是否在事务中
 * @return true 在事务中
 */
bool txn_in_progress(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_MVCC_H */
