/*
 * migrate_manager.h - 迁移管理器接口
 */

#ifndef DB_SHARDING_MIGRATE_MANAGER_H
#define DB_SHARDING_MIGRATE_MANAGER_H

#include "shard_balance.h"
#include "sharding.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct migrate_manager migrate_manager_t;

/**
 * @brief 创建迁移管理器
 */
migrate_manager_t *migrate_manager_create(shard_router_t *router);

/**
 * @brief 销毁迁移管理器
 */
void migrate_manager_destroy(migrate_manager_t *mgr);

/**
 * @brief 创建增量迁移任务
 */
migrate_task_t *migrate_manager_create_incremental(migrate_manager_t *mgr,
                                                    int source_shard,
                                                    int target_shard,
                                                    const void *key_range,
                                                    size_t range_len);

/**
 * @brief 创建虚拟节点迁移任务
 */
migrate_task_t *migrate_manager_create_vnode(migrate_manager_t *mgr,
                                              int source_shard,
                                              int target_shard,
                                              int vnode_id);

/**
 * @brief 执行迁移任务
 */
int migrate_manager_execute(migrate_manager_t *mgr, migrate_task_t *task);

/**
 * @brief 获取迁移任务状态
 */
migrate_status_t migrate_manager_get_status(migrate_manager_t *mgr, int task_id);

/**
 * @brief 取消迁移任务
 */
int migrate_manager_cancel(migrate_manager_t *mgr, int task_id);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARDING_MIGRATE_MANAGER_H */
