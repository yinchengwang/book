/*
 * migrate_manager.c - 迁移管理器实现
 *
 * 注意：这是框架实现，具体迁移逻辑在 Task 6-7 实现
 */

#include "db/sharding/migrate_manager.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* 迁移管理器内部状态 */
struct migrate_manager {
    shard_router_t *router;          /* 分片路由器 */
    migrate_task_t **tasks;          /* 迁移任务数组 */
    int task_count;                  /* 任务数量 */
    int task_capacity;               /* 任务容量 */
    int next_task_id;                /* 下一个任务ID */
    pthread_mutex_t mutex;           /* 互斥锁 */
    bool running;                    /* 运行状态 */
};

/* 全局迁移管理器实例（简化实现） */
static migrate_manager_t *g_migrate_manager = NULL;

/**
 * @brief 创建迁移管理器
 */
migrate_manager_t *migrate_manager_create(shard_router_t *router)
{
    migrate_manager_t *mgr = calloc(1, sizeof(migrate_manager_t));
    if (!mgr) {
        return NULL;
    }

    mgr->router = router;
    mgr->task_count = 0;
    mgr->task_capacity = 16;
    mgr->next_task_id = 1;
    mgr->running = true;

    mgr->tasks = calloc(mgr->task_capacity, sizeof(migrate_task_t *));
    if (!mgr->tasks) {
        free(mgr);
        return NULL;
    }

    pthread_mutex_init(&mgr->mutex, NULL);

    /* 设置全局实例（简化） */
    g_migrate_manager = mgr;

    return mgr;
}

/**
 * @brief 销毁迁移管理器
 */
void migrate_manager_destroy(migrate_manager_t *mgr)
{
    if (!mgr) {
        return;
    }

    pthread_mutex_lock(&mgr->mutex);

    /* 清理所有任务 */
    for (int i = 0; i < mgr->task_count; i++) {
        if (mgr->tasks[i]) {
            /* 释放 key_range 如果存在 */
            if (mgr->tasks[i]->key_range) {
                free(mgr->tasks[i]->key_range);
            }
            free(mgr->tasks[i]);
        }
    }

    free(mgr->tasks);
    mgr->running = false;

    pthread_mutex_unlock(&mgr->mutex);
    pthread_mutex_destroy(&mgr->mutex);

    if (g_migrate_manager == mgr) {
        g_migrate_manager = NULL;
    }

    free(mgr);
}

/**
 * @brief 创建增量迁移任务
 */
migrate_task_t *migrate_manager_create_incremental(migrate_manager_t *mgr,
                                                    int source_shard,
                                                    int target_shard,
                                                    const void *key_range,
                                                    size_t range_len)
{
    if (!mgr) {
        return NULL;
    }

    migrate_task_t *task = calloc(1, sizeof(migrate_task_t));
    if (!task) {
        return NULL;
    }

    task->task_id = mgr->next_task_id++;
    task->source_shard = source_shard;
    task->target_shard = target_shard;
    task->strategy = MIGRATE_INCREMENTAL;
    task->progress = 0.0;
    task->status = MIGRATE_STATUS_PENDING;

    /* 复制 key_range */
    if (key_range && range_len > 0) {
        task->key_range = malloc(range_len);
        if (task->key_range) {
            memcpy(task->key_range, key_range, range_len);
        }
    }

    pthread_mutex_lock(&mgr->mutex);

    /* 扩展任务数组如果需要 */
    if (mgr->task_count >= mgr->task_capacity) {
        mgr->task_capacity *= 2;
        migrate_task_t **new_tasks = realloc(mgr->tasks,
                                              mgr->task_capacity * sizeof(migrate_task_t *));
        if (new_tasks) {
            mgr->tasks = new_tasks;
        } else {
            pthread_mutex_unlock(&mgr->mutex);
            free(task->key_range);
            free(task);
            return NULL;
        }
    }

    mgr->tasks[mgr->task_count++] = task;

    pthread_mutex_unlock(&mgr->mutex);

    return task;
}

/**
 * @brief 创建虚拟节点迁移任务
 */
migrate_task_t *migrate_manager_create_vnode(migrate_manager_t *mgr,
                                              int source_shard,
                                              int target_shard,
                                              int vnode_id)
{
    if (!mgr) {
        return NULL;
    }

    migrate_task_t *task = calloc(1, sizeof(migrate_task_t));
    if (!task) {
        return NULL;
    }

    task->task_id = mgr->next_task_id++;
    task->source_shard = source_shard;
    task->target_shard = target_shard;
    task->strategy = MIGRATE_VIRTUAL_NODE;
    task->progress = 0.0;
    task->status = MIGRATE_STATUS_PENDING;

    pthread_mutex_lock(&mgr->mutex);

    /* 扩展任务数组如果需要 */
    if (mgr->task_count >= mgr->task_capacity) {
        mgr->task_capacity *= 2;
        migrate_task_t **new_tasks = realloc(mgr->tasks,
                                              mgr->task_capacity * sizeof(migrate_task_t *));
        if (new_tasks) {
            mgr->tasks = new_tasks;
        } else {
            pthread_mutex_unlock(&mgr->mutex);
            free(task);
            return NULL;
        }
    }

    mgr->tasks[mgr->task_count++] = task;

    pthread_mutex_unlock(&mgr->mutex);

    return task;
}

/**
 * @brief 执行迁移任务
 *
 * 注意：这是框架实现，Task 6-7 实现具体迁移逻辑
 */
int migrate_manager_execute(migrate_manager_t *mgr, migrate_task_t *task)
{
    if (!mgr || !task) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);

    /* 查找任务 */
    migrate_task_t *found = NULL;
    for (int i = 0; i < mgr->task_count; i++) {
        if (mgr->tasks[i] && mgr->tasks[i]->task_id == task->task_id) {
            found = mgr->tasks[i];
            break;
        }
    }

    if (!found) {
        pthread_mutex_unlock(&mgr->mutex);
        return -1;
    }

    /* 更新任务状态为运行中 */
    found->status = MIGRATE_STATUS_RUNNING;
    found->progress = 0.0;

    pthread_mutex_unlock(&mgr->mutex);

    /*
     * 具体迁移逻辑在 Task 6-7 实现
     * 目前框架只标记状态，实际数据迁移暂不执行
     */

    /* 模拟迁移完成（实际由 Task 6-7 更新） */
    pthread_mutex_lock(&mgr->mutex);
    found->status = MIGRATE_STATUS_COMPLETED;
    found->progress = 1.0;
    pthread_mutex_unlock(&mgr->mutex);

    return 0;
}

/**
 * @brief 获取迁移任务状态
 */
migrate_status_t migrate_manager_get_status(migrate_manager_t *mgr, int task_id)
{
    if (!mgr) {
        return MIGRATE_STATUS_FAILED;
    }

    pthread_mutex_lock(&mgr->mutex);

    for (int i = 0; i < mgr->task_count; i++) {
        if (mgr->tasks[i] && mgr->tasks[i]->task_id == task_id) {
            migrate_status_t status = mgr->tasks[i]->status;
            pthread_mutex_unlock(&mgr->mutex);
            return status;
        }
    }

    pthread_mutex_unlock(&mgr->mutex);

    return MIGRATE_STATUS_FAILED;
}

/**
 * @brief 取消迁移任务
 */
int migrate_manager_cancel(migrate_manager_t *mgr, int task_id)
{
    if (!mgr) {
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);

    for (int i = 0; i < mgr->task_count; i++) {
        if (mgr->tasks[i] && mgr->tasks[i]->task_id == task_id) {
            /* 只能取消 pending 状态的任务 */
            if (mgr->tasks[i]->status == MIGRATE_STATUS_PENDING) {
                mgr->tasks[i]->status = MIGRATE_STATUS_FAILED;
                pthread_mutex_unlock(&mgr->mutex);
                return 0;
            } else {
                pthread_mutex_unlock(&mgr->mutex);
                return -1;
            }
        }
    }

    pthread_mutex_unlock(&mgr->mutex);

    return -1;
}
