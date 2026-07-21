/**
 * @file txn_savepoint.h
 * @brief W5.3: 子事务/保存点接口
 *
 * 实现 SAVEPOINT / ROLLBACK TO / RELEASE 保存点机制。
 * 保存点允许在事务内创建子事务点，可以回滚到指定点而不影响整个事务。
 *
 * 使用方式：
 * @code
 * txn_start(ISOLATION_READ_COMMITTED);
 * txn_create_savepoint("sp1");
 * // ... 做一些操作 ...
 * txn_rollback_to_savepoint("sp1");  // 撤销 sp1 之后的修改
 * txn_release_savepoint("sp1");       // 释放保存点
 * txn_end();
 * @endcode
 */
#ifndef DB_STORAGE_TXN_SAVEPOINT_H
#define DB_STORAGE_TXN_SAVEPOINT_H

#include "db/storage/txn/mvcc.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 保存点 API
 * ======================================================================== */

/**
 * @brief 创建保存点
 *
 * 在事务中创建一个保存点，记录当前事务状态。
 *
 * @param name 保存点名称（可为 NULL，使用自动生成名称 "sp_0", "sp_1" ...）
 * @return 0 成功，-1 失败
 */
int txn_create_savepoint(const char *name);

/**
 * @brief 回滚到保存点
 *
 * 撤销从保存点创建以来所做的所有修改。
 * 事务状态恢复到保存点创建时的状态。
 *
 * @param name 保存点名称（NULL 表示回滚到上一个保存点）
 * @return 0 成功，-1 失败
 */
int txn_rollback_to_savepoint(const char *name);

/**
 * @brief 释放保存点
 *
 * 释放保存点，之后不能再回滚到该保存点。
 * 保存点之间的修改不会被撤销。
 *
 * @param name 保存点名称（NULL 表示释放最近创建的保存点）
 * @return 0 成功，-1 失败
 */
int txn_release_savepoint(const char *name);

/**
 * @brief 获取当前保存点深度
 *
 * @return 保存点深度（0 = 无保存点）
 */
int txn_savepoint_depth(void);

/**
 * @brief 获取保存点名称
 *
 * @param depth 保存点深度
 * @return 保存点名称，不存在返回 NULL
 */
const char *txn_savepoint_name(int depth);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_TXN_SAVEPOINT_H */