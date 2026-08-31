/**
 * @file txn_internal.h
 * @brief MVCC 事务内部 API
 *
 * 这些函数和类型用于 MVCC 快照隔离实现，
 * 不在公共 API (db/txn.h) 中暴露。
 */
#ifndef DB_STORAGE_TXN_INTERNAL_H
#define DB_STORAGE_TXN_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct txn_s txn_t;

/* ============================================================
 * 常量定义
 * ============================================================ */

#define TXN_OK          0
#define TXN_ERROR      (-1)
#define TXN_NOT_FOUND    1
#define TXN_ID_NONE     0

/** 事务状态 */
typedef enum txn_state_e {
    TXN_STATE_ACTIVE    = 0,
    TXN_STATE_COMMITTED = 1,
    TXN_STATE_ABORTED   = 2,
    TXN_STATE_PREPARED  = 3
} txn_state_t;

/* ============================================================
 * MVCC 隔离级别
 * ============================================================ */

/** 事务隔离级别 */
typedef enum txn_isolation_e {
    TXN_ISOLATION_READ_COMMITTED = 0,   /**< 读已提交 */
    TXN_ISOLATION_REPEATABLE_READ = 1, /**< 可重复读 */
    TXN_ISOLATION_SERIALIZABLE = 2     /**< 可串行化 */
} txn_isolation_t;

/** 元组头（MVCC 版本信息） */
typedef struct tuple_header_s {
    uint32_t t_xmin;       /**< 创建事务 ID */
    uint32_t t_xmax;       /**< 删除事务 ID (0 = 未删除) */
    uint64_t t_csn;        /**< 提交序列号 */
    bool     committed;    /**< 是否已提交 */
} tuple_header_t;

/* ============================================================
 * 事务基本 API
 * ============================================================ */

/**
 * @brief 开始事务
 * @param db KV 数据库
 * @return 事务句柄
 */
txn_t *txn_begin(void *db);

/**
 * @brief 提交事务
 * @param txn 事务句柄
 * @return 0 成功，非0 失败
 */
int txn_commit(txn_t *txn);

/**
 * @brief 回滚事务
 * @param txn 事务句柄
 * @return 0 成功，非0 失败
 */
int txn_rollback(txn_t *txn);

/**
 * @brief 释放事务
 * @param txn 事务句柄
 */
void txn_free(txn_t *txn);

/**
 * @brief 获取事务 ID
 * @param txn 事务句柄
 * @return 事务 ID
 */
uint32_t txn_id(const txn_t *txn);

/**
 * @brief 获取事务状态
 * @param txn 事务句柄
 * @return 事务状态
 */
txn_state_t txn_state(const txn_t *txn);

/**
 * @brief 写入键值
 * @param txn 事务句柄
 * @param key 键
 * @param key_len 键长度
 * @param value 值
 * @param value_len 值长度
 * @return 0 成功
 */
int txn_put(txn_t *txn, const void *key, size_t key_len,
            const void *value, size_t value_len);

/**
 * @brief 读取值
 * @param txn 事务句柄
 * @param key 键
 * @param key_len 键长度
 * @param out_value 输出值
 * @param out_len 输出值长度
 * @return 0 成功
 */
int txn_get(txn_t *txn, const void *key, size_t key_len,
            void **out_value, size_t *out_len);

/**
 * @brief 删除键
 * @param txn 事务句柄
 * @param key 键
 * @param key_len 键长度
 * @return 0 成功
 */
int txn_delete(txn_t *txn, const void *key, size_t key_len);

/* ============================================================
 * MVCC 事务函数
 * ============================================================ */

/**
 * @brief 开始 MVCC 事务
 * @param txn 事务句柄
 * @param level 隔离级别
 * @return 0 成功，非0 失败
 */
int txn_begin_mvcc(txn_t *txn, txn_isolation_t level);

/**
 * @brief 检查事务是否使用 MVCC
 * @param txn 事务句柄
 * @return true 如果使用 MVCC
 */
bool txn_is_mvcc(const txn_t *txn);

/**
 * @brief 获取事务隔离级别
 * @param txn 事务句柄
 * @return 隔离级别
 */
txn_isolation_t txn_get_isolation_level(const txn_t *txn);

/**
 * @brief 获取事务快照 LSN
 * @param txn 事务句柄
 * @return 快照 LSN
 */
uint64_t txn_get_snapshot_lsn(const txn_t *txn);

/**
 * @brief 检查元组对当前事务是否可见
 * @param txn 事务句柄
 * @param tuple 元组头
 * @return 1 可见，0 不可见
 */
int txn_visibility_check(txn_t *txn, const tuple_header_t *tuple);

/**
 * @brief 升级锁到排他模式
 * @param txn 事务句柄
 * @return 0 成功，非0 失败
 */
int txn_upgrade_lock_to_exclusive(txn_t *txn);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_TXN_INTERNAL_H */
