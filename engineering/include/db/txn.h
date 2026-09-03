/**
 * @file txn.h
 * @brief 事务管理器 API - MVCC 版本
 *
 * MVCC 事务特性：
 * - XMIN/XMAX 事务标识：每行记录创建事务(xmin)和删除事务(xmax)
 * - Command ID (CID)：同一事务内的命令序号，用于可见性判断
 * - Subtransaction：支持嵌套事务/保存点
 * - Savepoint：支持部分回滚
 */
#ifndef DB_TXN_MVCC_H
#define DB_TXN_MVCC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 无效事务ID */
#define TXN_ID_NONE 0

/** 无效命令ID */
#define TXN_CID_NONE 0

/** 最大并发事务数 */
#define TXN_MAX_ACTIVE 1024

/** 最大子事务嵌套深度 */
#define TXN_MAX_SUBTXN_DEPTH 16

/** 最大保存点数量 */
#define TXN_MAX_SAVEPOINTS 64

/** 事务ID回绕阈值（达到后需要 vacuum） */
#define TXN_ID_WRAP_THRESHOLD (INT64_MAX - 1000000)

/* ============================================================
 * 事务状态
 * ============================================================ */

/** 事务状态 */
typedef enum txn_state_e {
    TXN_STATE_ACTIVE = 0,     /**< 活动状态 */
    TXN_STATE_COMMITTED = 1,  /**< 已提交 */
    TXN_STATE_ABORTED = 2,    /**< 已回滚 */
    TXN_STATE_PREPARED = 3    /**< 预提交（两阶段提交） */
} txn_state_t;

/** 事务类型 */
typedef enum txn_type_e {
    TXN_TYPE_READ_WRITE = 0,  /**< 读写事务 */
    TXN_TYPE_READ_ONLY = 1    /**< 只读事务 */
} txn_type_t;

/* ============================================================
 * MVCC 核心类型
 * ============================================================ */

/** 事务ID类型（64位，支持回绕检测） */
typedef int64_t txn_id_t;

/** 命令ID类型（同一事务内的命令序号） */
typedef uint32_t txn_cid_t;

/** 事务XIP列表项 */
typedef struct txn_xip_entry {
    txn_id_t xid;             /**< 事务ID */
    txn_cid_t top_cid;        /**< 顶层命令ID（子事务使用） */
    bool is_subxact;          /**< 是否为子事务 */
} txn_xip_entry_t;

/** 事务XIP列表（活跃事务快照） */
typedef struct txn_xip_list {
    txn_xip_entry_t *entries; /**< 条目数组 */
    int count;                /**< 条目数量 */
    int capacity;             /**< 数组容量 */
} txn_xip_list_t;

/* ============================================================
 * 保存点
 * ============================================================ */

/** 保存点结构 */
typedef struct txn_savepoint {
    char name[64];            /**< 保存点名称 */
    txn_cid_t cid;            /**< 保存点时的命令ID */
    txn_id_t subxact_id;      /**< 子事务ID（如果有） */
    void *undo_ptr;           /**< Undo 指针位置 */
} txn_savepoint_t;

/** 保存点栈 */
typedef struct txn_savepoint_stack {
    txn_savepoint_t *stack;   /**< 保存点数组 */
    int top;                  /**< 栈顶索引 */
    int capacity;             /**< 容量 */
} txn_savepoint_stack_t;

/* ============================================================
 * 子事务
 * ============================================================ */

/** 子事务状态 */
typedef struct txn_subxact {
    txn_id_t parent_xid;      /**< 父事务ID */
    txn_id_t subxid;          /**< 子事务ID */
    txn_cid_t cid;            /**< 当前命令ID */
    void *undo_head;          /**< Undo 链头 */
} txn_subxact_t;

/* ============================================================
 * 事务上下文
 * ============================================================ */

/** 事务句柄 */
typedef struct txn_context_s txn_context_t;

/* ============================================================
 * 事务生命周期
 * ============================================================ */

/**
 * @brief 开始一个新事务
 * @param db 数据库句柄
 * @param txn_type 事务类型（读写/只读）
 * @return 事务上下文，失败返回 NULL
 *
 * 示例：
 * @code
 * txn_context_t *txn = txn_begin(db, TXN_TYPE_READ_WRITE);
 * if (!txn) {
 *     printf("Failed to start transaction\n");
 *     return;
 * }
 *
 * // 在事务中执行操作
 * txn_put(txn, "key1", 4, "value1", 6);
 *
 * if (txn_commit(txn) == 0) {
 *     printf("Transaction committed\n");
 * }
 * @endcode
 */
txn_context_t *txn_begin(void *db, txn_type_t txn_type);

/**
 * @brief 提交事务
 * @param txn 事务上下文
 * @return 0 成功，非0 失败
 */
int txn_commit(txn_context_t *txn);

/**
 * @brief 回滚事务
 * @param txn 事务上下文
 * @return 0 成功，非0 失败
 */
int txn_rollback(txn_context_t *txn);

/**
 * @brief 释放事务句柄（不提交也不回滚）
 * @param txn 事务上下文
 */
void txn_free(txn_context_t *txn);

/* ============================================================
 * 命令ID管理
 * ============================================================ */

/**
 * @brief 获取下一个命令ID
 * @param txn 事务上下文
 * @return 新的命令ID
 *
 * 每个SQL语句执行前调用一次，用于实现同一事务内的命令级别隔离。
 */
txn_cid_t txn_next_cid(txn_context_t *txn);

/**
 * @brief 获取当前命令ID
 * @param txn 事务上下文
 * @return 当前命令ID
 */
txn_cid_t txn_current_cid(const txn_context_t *txn);

/* ============================================================
 * 子事务管理
 * ============================================================ */

/**
 * @brief 开始子事务
 * @param txn 父事务上下文
 * @return 子事务ID，失败返回 TXN_ID_NONE
 *
 * 子事务允许嵌套回滚，不影响父事务。
 */
txn_id_t txn_begin_subxact(txn_context_t *txn);

/**
 * @brief 提交子事务
 * @param txn 事务上下文
 * @return 0 成功，非0 失败
 */
int txn_commit_subxact(txn_context_t *txn);

/**
 * @brief 回滚子事务
 * @param txn 事务上下文
 * @param cid 回滚到的命令ID（子事务开始时的CID）
 * @return 0 成功，非0 失败
 */
int txn_rollback_subxact(txn_context_t *txn, txn_cid_t cid);

/* ============================================================
 * 保存点管理
 * ============================================================ */

/**
 * @brief 创建保存点
 * @param txn 事务上下文
 * @param name 保存点名称
 * @return 0 成功，非0 失败
 *
 * 示例：
 * @code
 * txn_savepoint(txn, "before_update");
 * // 执行一些操作
 * txn_rollback_to_savepoint(txn, "before_update");
 * @endcode
 */
int txn_savepoint(txn_context_t *txn, const char *name);

/**
 * @brief 回滚到保存点
 * @param txn 事务上下文
 * @param name 保存点名称
 * @return 0 成功，非0 失败
 */
int txn_rollback_to_savepoint(txn_context_t *txn, const char *name);

/**
 * @brief 释放保存点（删除保存点，不回滚）
 * @param txn 事务上下文
 * @param name 保存点名称
 * @return 0 成功，非0 失败
 */
int txn_release_savepoint(txn_context_t *txn, const char *name);

/* ============================================================
 * 事务查询
 * ============================================================ */

/**
 * @brief 获取事务ID（XMIN）
 * @param txn 事务上下文
 * @return 事务ID
 */
txn_id_t txn_get_id(const txn_context_t *txn);

/**
 * @brief 获取事务状态
 * @param txn 事务上下文
 * @return 事务状态
 */
txn_state_t txn_get_state(const txn_context_t *txn);

/**
 * @brief 获取事务类型
 * @param txn 事务上下文
 * @return 事务类型
 */
txn_type_t txn_get_type(const txn_context_t *txn);

/**
 * @brief 获取事务开始时间戳
 * @param txn 事务上下文
 * @return 时间戳（毫秒）
 */
uint64_t txn_get_start_time(const txn_context_t *txn);

/**
 * @brief 检查是否只读事务
 * @param txn 事务上下文
 * @return true 如果只读
 */
bool txn_is_read_only(const txn_context_t *txn);

/* ============================================================
 * 全局事务管理器
 * ============================================================ */

/**
 * @brief 初始化全局事务管理器
 * @param max_active 最大活跃事务数
 * @return 0 成功，非0 失败
 */
int txn_manager_init(size_t max_active);

/**
 * @brief 关闭全局事务管理器
 */
void txn_manager_shutdown(void);

/**
 * @brief 获取当前全局事务ID
 * @return 当前最大已分配事务ID
 */
txn_id_t txn_manager_get_current_xid(void);

/**
 * @brief 获取下一个事务ID
 * @return 下一个事务ID
 */
txn_id_t txn_manager_next_xid(void);

/**
 * @brief 获取全局XIP列表快照
 * @param out_xip_list 输出：XIP列表
 * @return 0 成功，非0 失败
 */
int txn_manager_get_snapshot(txn_xip_list_t *out_xip_list);

/**
 * @brief 释放XIP列表快照
 * @param xip_list XIP列表
 */
void txn_manager_release_snapshot(txn_xip_list_t *xip_list);

/**
 * @brief 注册活跃事务
 * @param xid 事务ID
 * @return 0 成功，非0 失败
 */
int txn_manager_register_active(txn_id_t xid);

/**
 * @brief 注销活跃事务
 * @param xid 事务ID
 */
void txn_manager_unregister_active(txn_id_t xid);

/**
 * @brief 检查事务是否活跃
 * @param xid 事务ID
 * @return true 如果活跃
 */
bool txn_manager_is_active(txn_id_t xid);

/**
 * @brief 获取活动事务数量
 * @return 活动事务数量
 */
size_t txn_manager_active_count(void);

/* ============================================================
 * MVCC 可见性判断
 * ============================================================ */

/**
 * @brief 判断版本对事务是否可见
 * @param txn 事务上下文
 * @param xmin 版本创建事务ID
 * @param xmax 版本删除事务ID
 * @param xmin_cid 版本创建命令ID（同一事务内判断）
 * @return true 如果可见
 */
bool txn_version_visible(txn_context_t *txn,
                         txn_id_t xmin,
                         txn_id_t xmax,
                         txn_cid_t xmin_cid);

/**
 * @brief 获取事务快照的XMIN值
 * @param txn 事务上下文
 * @return XMIN值
 */
txn_id_t txn_snapshot_xmin(const txn_context_t *txn);

/**
 * @brief 获取事务快照的XMAX值
 * @param txn 事务上下文
 * @return XMAX值
 */
txn_id_t txn_snapshot_xmax(const txn_context_t *txn);

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 检查事务ID是否有效
 * @param xid 事务ID
 * @return true 如果有效
 */
bool txn_id_is_valid(txn_id_t xid);

/**
 * @brief 检查是否需要VACUUM（事务ID接近回绕阈值）
 * @return true 如果需要VACUUM
 */
bool txn_needs_vacuum(void);

/**
 * @brief 打印事务信息（调试用）
 * @param txn 事务上下文
 */
void txn_print_info(const txn_context_t *txn);

/**
 * @brief 打印全局事务管理器状态（调试用）
 */
void txn_manager_print_status(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_TXN_MVCC_H */
