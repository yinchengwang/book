/**
 * @file mmdb_txn.h
 * @brief ACID 事务 API
 *
 * 基于 SQLite BEGIN TRANSACTION 实现，支持只读/读写事务，
 * 写锁升级，以及嵌套事务（Savepoint）。
 */
#ifndef SDK_MMDB_TXN_H
#define SDK_MMDB_TXN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct mmdb_s mmdb_t;

/* 事务类型 */
typedef enum {
    MMDB_TXN_READONLY,          /* 只读事务 */
    MMDB_TXN_READWRITE,         /* 读写事务 */
} mmdb_txn_mode_t;

/* 事务状态 */
typedef enum {
    MMDB_TXN_ACTIVE,            /* 活跃状态 */
    MMDB_TXN_COMMITTED,         /* 已提交 */
    MMDB_TXN_ABORTED,           /* 已回滚 */
} mmdb_txn_state_t;

/* 事务句柄（不透明类型） */
typedef struct mmdb_txn_s mmdb_txn_t;

/**
 * @brief 开始事务
 * @param db        数据库句柄
 * @param mode      事务模式（只读/读写）
 * @param txn       输出事务句柄
 * @return 0 成功，非 0 错误码
 */
int mmdb_txn_begin(mmdb_t* db, mmdb_txn_mode_t mode, mmdb_txn_t** txn);

/**
 * @brief 提交事务
 * @param txn       事务句柄
 * @return 0 成功，非 0 错误码
 */
int mmdb_txn_commit(mmdb_txn_t* txn);

/**
 * @brief 回滚事务
 * @param txn       事务句柄
 * @return 0 成功，非 0 错误码
 */
int mmdb_txn_abort(mmdb_txn_t* txn);

/**
 * @brief 获取事务状态
 * @param txn       事务句柄
 * @return 事务状态
 */
mmdb_txn_state_t mmdb_txn_get_state(const mmdb_txn_t* txn);

/**
 * @brief 检查事务是否为只读
 * @param txn       事务句柄
 * @return true 只读，false 读写
 */
bool mmdb_txn_is_readonly(const mmdb_txn_t* txn);

/**
 * @brief 释放事务句柄（仅在已提交/已回滚后调用）
 * @param txn       事务句柄
 */
void mmdb_txn_free(mmdb_txn_t* txn);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_TXN_H */
