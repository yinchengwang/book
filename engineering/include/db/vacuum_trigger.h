/**
 * @file vacuum_trigger.h
 * @brief 自动 vacuum 阈值触发（C2-1 T8）
 */
#ifndef DB_VACUUM_TRIGGER_H
#define DB_VACUUM_TRIGGER_H

#include "db/storage/txn/vacuum.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 检查 xmin/xmax 老龄化并按需触发 vacuum
 *
 * 简化策略：累计已撤销/已提交事务数超过阈值时触发 vacuum_autovacuum。
 * 真实实现参考 PG autovacuum launcher 的 cost-based delay。
 */
void vacuum_trigger_check(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_VACUUM_TRIGGER_H */