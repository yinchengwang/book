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
 * @brief 迁移进度回调
 * @param task_id 任务ID
 * @param progress 进度 (0.0-1.0)
 * @param user_data 用户数据
 */
typedef void (*migrate_progress_cb)(int task_id, double progress, void *user_data);

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
 * @brief 执行增量迁移（核心逻辑）
 * @param mgr 迁移管理器
 * @param task 迁移任务
 * @param progress_cb 进度回调（可为 NULL）
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
int migrate_execute_incremental(migrate_manager_t *mgr,
                                migrate_task_t *task,
                                migrate_progress_cb progress_cb,
                                void *user_data);

/**
 * @brief 执行虚拟节点迁移（核心逻辑）
 * @param mgr 迁移管理器
 * @param task 迁移任务
 * @param progress_cb 进度回调（可为 NULL）
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
int migrate_execute_vnode(migrate_manager_t *mgr,
                          migrate_task_t *task,
                          migrate_progress_cb progress_cb,
                          void *user_data);

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

/**
 * @brief 获取分片数据库路径
 * @param shard_id 分片ID
 * @param path 输出路径缓冲区
 * @param path_len 缓冲区长度
 * @return 0 成功
 */
int migrate_get_shard_path(int shard_id, char *path, size_t path_len);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARDING_MIGRATE_MANAGER_H */
