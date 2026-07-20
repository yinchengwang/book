/**
 * @file txn.h
 * @brief 事务管理器 API
 *
 * 事务特性：
 * - 支持 ACID 特性（通过 WAL 保证持久性，通过 undo 保证原子性）
 * - 写操作先写 WAL，再写数据页
 * - 支持回滚未提交事务的修改
 * - 简化实现：单写者模型，不支持并发事务
 */
#ifndef DB_TXN_H
#define DB_TXN_H

#include "db/storage/wal/wal.h"
#include "db/kv.h"
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

/** 最大并发事务数 */
#define TXN_MAX_ACTIVE 1024

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

/* ============================================================
 * 事务上下文
 * ============================================================ */

/** 事务句柄 */
typedef struct txn_s txn_t;

/* ============================================================
 * 事务生命周期
 * ============================================================ */

/**
 * @brief 开始一个新事务
 * @param db KV 数据库句柄
 * @return 事务句柄，失败返回 NULL
 *
 * 示例：
 * @code
 * txn_t *txn = txn_begin(db);
 * if (!txn) {
 *     printf("Failed to start transaction\n");
 *     return;
 * }
 *
 * txn_put(txn, "key1", 4, "value1", 6);
 * txn_put(txn, "key2", 4, "value2", 6);
 *
 * if (txn_commit(txn) == TXN_OK) {
 *     printf("Transaction committed\n");
 * } else {
 *     printf("Transaction failed: %s\n", txn_errmsg(txn));
 * }
 * @endcode
 */
txn_t *txn_begin(kv_t *db);

/**
 * @brief 提交事务
 * @param txn 事务句柄
 * @return TXN_OK 成功，TXN_ERROR 失败
 */
int txn_commit(txn_t *txn);

/**
 * @brief 回滚事务
 * @param txn 事务句柄
 * @return TXN_OK 成功，TXN_ERROR 失败
 */
int txn_rollback(txn_t *txn);

/**
 * @brief 释放事务句柄（不提交也不回滚）
 * @param txn 事务句柄
 */
void txn_free(txn_t *txn);

/* ============================================================
 * 事务操作（类似于 KV API，但通过事务包装）
 * ============================================================ */

/**
 * @brief 在事务中插入键值对
 * @param txn 事务句柄
 * @param key 键
 * @param key_len 键长度
 * @param value 值
 * @param value_len 值长度
 * @return TXN_OK 成功，TXN_ERROR 失败
 */
int txn_put(txn_t *txn,
            const void *key, size_t key_len,
            const void *value, size_t value_len);

/**
 * @brief 在事务中获取值
 * @param txn 事务句柄
 * @param key 键
 * @param key_len 键长度
 * @param out_value 输出值（调用者负责释放）
 * @param out_len 输出值长度
 * @return TXN_OK 成功，TXN_NOT_FOUND 键不存在，TXN_ERROR 失败
 */
int txn_get(txn_t *txn,
            const void *key, size_t key_len,
            void **out_value, size_t *out_len);

/**
 * @brief 在事务中删除键值对
 * @param txn 事务句柄
 * @param key 键
 * @param key_len 键长度
 * @return TXN_OK 成功，TXN_NOT_FOUND 键不存在，TXN_ERROR 失败
 */
int txn_delete(txn_t *txn, const void *key, size_t key_len);

/**
 * @brief 检查键是否存在
 * @param txn 事务句柄
 * @param key 键
 * @param key_len 键长度
 * @return true 存在，false 不存在
 */
bool txn_exists(txn_t *txn, const void *key, size_t key_len);

/* ============================================================
 * 事务查询
 * ============================================================ */

/**
 * @brief 获取事务ID
 * @param txn 事务句柄
 * @return 事务ID
 */
uint32_t txn_id(const txn_t *txn);

/**
 * @brief 获取事务状态
 * @param txn 事务句柄
 * @return 事务状态
 */
txn_state_t txn_state(const txn_t *txn);

/**
 * @brief 获取错误信息
 * @param txn 事务句柄
 * @return 错误信息字符串
 */
const char *txn_errmsg(const txn_t *txn);

/**
 * @brief 获取事务开始时间戳
 * @param txn 事务句柄
 * @return 时间戳（毫秒）
 */
uint64_t txn_start_time(const txn_t *txn);

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 事务结果码 */
typedef enum txn_result_e {
    TXN_OK = 0,           /**< 成功 */
    TXN_NOT_FOUND = 1,    /**< 键不存在 */
    TXN_ERROR = 2,        /**< 一般错误 */
    TXN_CORRUPT = 3,      /**< 数据库损坏 */
    TXN_NOMEM = 4,        /**< 内存不足 */
    TXN_INVALID = 5       /**< 无效参数 */
} txn_result_t;

/**
 * @brief 设置事务错误信息
 * @param txn 事务句柄
 * @param msg 错误信息
 */
void txn_set_error(txn_t *txn, const char *msg);

/**
 * @brief 获取活动事务数量
 * @return 活动事务数量
 */
size_t txn_active_count(void);

/**
 * @brief 打印所有活动事务信息（调试用）
 */
void txn_print_active(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_TXN_H */