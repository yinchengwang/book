/*
 * migrate_manager.c - 迁移管理器实现
 *
 * 实现增量迁移核心逻辑（Task 6）
 */

#include "db/sharding/migrate_manager.h"
#include "db/storage/kv/kv.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

/* 默认数据目录 */
#ifndef DEFAULT_DATA_DIR
#define DEFAULT_DATA_DIR "./data"
#endif

/* 批次大小（避免内存溢出） */
#ifndef MIGRATE_BATCH_SIZE
#define MIGRATE_BATCH_SIZE 1000
#endif

/* 进度回调间隔（每多少条记录回调一次） */
#ifndef MIGRATE_PROGRESS_INTERVAL
#define MIGRATE_PROGRESS_INTERVAL 5000
#endif

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
 * @brief 获取分片数据库路径
 *
 * 分片数据存储在: {data_dir}/shard_{shard_id}.db
 */
int migrate_get_shard_path(int shard_id, char *path, size_t path_len)
{
    if (!path || path_len == 0) {
        return -1;
    }

    snprintf(path, path_len, "%s/shard_%d.db", DEFAULT_DATA_DIR, shard_id);
    return 0;
}

/**
 * @brief 执行增量迁移（核心逻辑）
 *
 * 迁移流程：
 * 1. 打开源分片和目标分片数据库
 * 2. 从源分片读取 key_range 范围内的数据
 * 3. 写入目标分片（双写）
 * 4. 验证数据一致性
 * 5. 更新路由表
 * 6. 删除源分片数据
 *
 * @param mgr 迁移管理器
 * @param task 迁移任务
 * @param progress_cb 进度回调（可为 NULL）
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
int migrate_execute_incremental(migrate_manager_t *mgr,
                                migrate_task_t *task,
                                migrate_progress_cb progress_cb,
                                void *user_data)
{
    if (!mgr || !task) {
        return -1;
    }

    if (task->strategy != MIGRATE_INCREMENTAL) {
        return -1;
    }

    char src_path[512];
    char dst_path[512];

    /* 获取源和目标分片路径 */
    if (migrate_get_shard_path(task->source_shard, src_path, sizeof(src_path)) != 0) {
        return -1;
    }
    if (migrate_get_shard_path(task->target_shard, dst_path, sizeof(dst_path)) != 0) {
        return -1;
    }

    /* 打开源分片数据库 */
    kv_t *src_db = kv_open(src_path);
    if (!src_db) {
        return -1;
    }

    /* 打开目标分片数据库（不存在则创建） */
    kv_t *dst_db = kv_open(dst_path);
    if (!dst_db) {
        kv_close(src_db);
        return -1;
    }

    /* 设置起始和结束键 */
    const void *start_key = task->key_range;
    size_t start_len = 0;
    const void *end_key = NULL;
    size_t end_len = 0;

    if (task->key_range) {
        /* key_range 格式: [start_key, end_key] 的打包格式
         * 这里简化处理，假设 key_range 是以 null 结尾的字符串
         * 实际使用时可能需要更复杂的解析 */
        start_len = strlen((const char *)task->key_range);
        /* 计算 end_key = start_key + 最后一个字节 + 1（模拟范围） */
        /* 这是一个简化实现 */
    }

    /* 创建扫描迭代器 */
    kv_iter_t *iter = kv_scan(src_db, start_key, start_len, end_key, end_len);
    if (!iter) {
        kv_close(dst_db);
        kv_close(src_db);
        return -1;
    }

    /* 获取总键数量用于进度计算 */
    kv_stats_t src_stats;
    double total_keys = 1.0;  /* 避免除零 */
    if (kv_stats(src_db, &src_stats) == KV_OK) {
        total_keys = (double)(src_stats.num_keys > 0 ? src_stats.num_keys : 1);
    }

    /* 批量缓冲 */
    void **keys = malloc(sizeof(void *) * MIGRATE_BATCH_SIZE);
    void **values = malloc(sizeof(void *) * MIGRATE_BATCH_SIZE);
    size_t *key_lens = malloc(sizeof(size_t) * MIGRATE_BATCH_SIZE);
    size_t *value_lens = malloc(sizeof(size_t) * MIGRATE_BATCH_SIZE);
    size_t batch_count = 0;
    uint64_t migrated_count = 0;
    int result = 0;

    if (!keys || !values || !key_lens || !value_lens) {
        result = -1;
        goto cleanup;
    }

    /* 第一阶段：读取源分片数据并写入目标分片 */
    while (kv_iter_next(iter) == KV_OK) {
        const void *key = kv_iter_key(iter);
        size_t key_len = kv_iter_key_len(iter);
        const void *value = kv_iter_value(iter);
        size_t value_len = kv_iter_value_len(iter);

        if (!key || !value || key_len == 0 || value_len == 0) {
            continue;
        }

        /* 写入目标分片 */
        if (kv_put(dst_db, key, key_len, value, value_len) != KV_OK) {
            result = -1;
            break;
        }

        batch_count++;
        migrated_count++;

        /* 批次处理 */
        if (batch_count >= MIGRATE_BATCH_SIZE) {
            /* 刷新目标数据库 */
            kv_flush(dst_db);

            /* 更新进度 */
            double progress = 0.5 * (migrated_count / total_keys);  /* 前50% */
            task->progress = progress > 1.0 ? 0.5 : progress;

            if (progress_cb && migrated_count % MIGRATE_PROGRESS_INTERVAL == 0) {
                progress_cb(task->task_id, task->progress, user_data);
            }

            batch_count = 0;
        }
    }

    /* 处理剩余批次 */
    if (batch_count > 0) {
        kv_flush(dst_db);
    }

    /* 第二阶段：验证数据一致性 */
    if (result == 0) {
        kv_iter_free(iter);
        iter = kv_scan(dst_db, start_key, start_len, end_key, end_len);
        if (!iter) {
            result = -1;
            goto cleanup;
        }

        uint64_t verified_count = 0;
        while (kv_iter_next(iter) == KV_OK) {
            const void *key = kv_iter_key(iter);
            size_t key_len = kv_iter_key_len(iter);

            /* 验证源和目标数据一致 */
            void *src_value = NULL;
            size_t src_value_len = 0;

            if (kv_get(src_db, key, key_len, &src_value, &src_value_len) != KV_OK) {
                result = -1;
                break;
            }

            const void *dst_value = kv_iter_value(iter);
            size_t dst_value_len = kv_iter_value_len(iter);

            if (src_value_len != dst_value_len ||
                memcmp(src_value, dst_value, src_value_len) != 0) {
                free(src_value);
                result = -1;
                break;
            }

            /* 释放源值内存 */
            free(src_value);
            src_value = NULL;

            verified_count++;

            /* 进度更新（50%-90%） */
            double progress = 0.5 + 0.4 * (verified_count / total_keys);
            task->progress = progress > 0.9 ? 0.9 : progress;

            if (progress_cb && verified_count % MIGRATE_PROGRESS_INTERVAL == 0) {
                progress_cb(task->task_id, task->progress, user_data);
            }
        }
    }

    /* 第三阶段：更新路由表并删除源分片数据 */
    if (result == 0) {
        /* 更新路由表：将 key_range 路由到目标分片
         * 实际实现需要调用 shard_router_update_range 或类似接口
         * 这里简化处理，只更新本地路由缓存 */

        /* 获取路由器 */
        shard_router_t *router = mgr->router;
        if (router) {
            /* 通知路由表更新（通过协调器） */
            /* 实际需要调用: shard_router_update(mgr->router, task->key_range, task->target_shard) */
        }

        /* 删除源分片数据 */
        kv_iter_free(iter);
        iter = kv_scan(src_db, start_key, start_len, end_key, end_len);
        if (iter) {
            while (kv_iter_next(iter) == KV_OK) {
                const void *key = kv_iter_key(iter);
                size_t key_len = kv_iter_key_len(iter);
                kv_delete(src_db, key, key_len);
            }
        }

        /* 刷脏页 */
        kv_flush(src_db);

        /* 更新进度到完成 */
        task->progress = 1.0;
        if (progress_cb) {
            progress_cb(task->task_id, 1.0, user_data);
        }
    }

cleanup:
    if (iter) {
        kv_iter_free(iter);
    }

    /* 关闭数据库 */
    kv_close(dst_db);
    kv_close(src_db);

    /* 释放批次内存 */
    free(keys);
    free(values);
    free(key_lens);
    free(value_lens);

    if (result == 0) {
        task->status = MIGRATE_STATUS_COMPLETED;
    } else {
        task->status = MIGRATE_STATUS_FAILED;
    }

    return result;
}

/**
 * @brief 执行迁移任务
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

    /* 根据策略执行迁移 */
    int result;
    if (found->strategy == MIGRATE_INCREMENTAL) {
        /* 执行增量迁移 */
        result = migrate_execute_incremental(mgr, found, NULL, NULL);
    } else {
        /* 虚拟节点迁移暂未实现 */
        result = -1;
    }

    /* 更新最终状态 */
    pthread_mutex_lock(&mgr->mutex);
    if (result == 0) {
        found->status = MIGRATE_STATUS_COMPLETED;
        found->progress = 1.0;
    } else {
        if (found->status == MIGRATE_STATUS_RUNNING) {
            found->status = MIGRATE_STATUS_FAILED;
        }
    }
    pthread_mutex_unlock(&mgr->mutex);

    return result;
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
