/**
 * @file bgworker_api.c
 * @brief 后台任务调度器公共 API 实现
 */

#include "bgworker_internal.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * @brief 注册后台任务
 * @param task 任务定义
 * @param slot_out 输出分配的任务槽位
 * @return 0 成功，-1 失败
 */
int db_bgworker_register(db_bg_task_t *task, int *slot_out) {
    if (!task || !task->name || !task->check || !task->execute) {
        LOG_ERROR("%s", "无效的任务参数");
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    /* 检查是否已存在同名任务 */
    if (_db_bgworker_find_slot(&g_worker, task->name) >= 0) {
        LOG_ERROR("任务 '%s' 已存在", task->name);
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    /* 查找空闲槽位 */
    int slot = _db_bgworker_find_free_slot(&g_worker);
    if (slot < 0) {
        LOG_ERROR("%s", "无可用任务槽位");
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    /* 分配上下文 */
    db_bg_task_context_t *ctx = _db_bgworker_alloc_context(task);
    if (!ctx) {
        LOG_ERROR("%s", "分配任务上下文失败");
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    /* 调用初始化回调 */
    if (task->init) {
        if (task->init(ctx) != 0) {
            LOG_ERROR("任务 '%s' 初始化失败", task->name);
            _db_bgworker_free_context(ctx);
            pthread_mutex_unlock(&g_worker.mutex);
            return -1;
        }
    }

    /* 注册任务 */
    task->ctx = ctx;
    g_worker.slots[slot].task = task;
    g_worker.slots[slot].in_use = true;
    g_worker.slots[slot].next_check_time = _db_bgworker_now_ms();

    if (slot_out) {
        *slot_out = slot;
    }

    LOG_INFO("任务 '%s' 已注册到槽位 %d", task->name, slot);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 注销后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_unregister(const char *name) {
    if (!name) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    int slot = _db_bgworker_find_slot(&g_worker, name);
    if (slot < 0) {
        LOG_ERROR("任务 '%s' 未找到", name);
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    db_bg_task_t *task = g_worker.slots[slot].task;

    /* 调用清理回调 */
    if (task->cleanup && task->ctx) {
        task->cleanup(task->ctx);
    }

    /* 释放上下文 */
    if (task->ctx) {
        _db_bgworker_free_context(task->ctx);
        task->ctx = NULL;
    }

    /* 释放槽位 */
    memset(&g_worker.slots[slot], 0, sizeof(db_bg_task_slot_t));

    LOG_INFO("任务 '%s' 已注销", name);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 暂停后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_pause(const char *name) {
    if (!name) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    int slot = _db_bgworker_find_slot(&g_worker, name);
    if (slot < 0) {
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    db_bg_task_set_status(g_worker.slots[slot].task->ctx, DB_BG_TASK_STATUS_PAUSED);
    LOG_INFO("任务 '%s' 已暂停", name);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 恢复后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_resume(const char *name) {
    if (!name) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    int slot = _db_bgworker_find_slot(&g_worker, name);
    if (slot < 0) {
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    db_bg_task_set_status(g_worker.slots[slot].task->ctx, DB_BG_TASK_STATUS_IDLE);
    g_worker.slots[slot].next_check_time = _db_bgworker_now_ms();
    LOG_INFO("任务 '%s' 已恢复", name);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 获取任务状态
 * @param name 任务名称
 * @param status_out 输出状态
 * @return 0 成功，-1 未找到
 */
int db_bgworker_get_status(const char *name, unsigned int *status_out) {
    if (!name || !status_out) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    int slot = _db_bgworker_find_slot(&g_worker, name);
    if (slot < 0) {
        pthread_mutex_unlock(&g_worker.mutex);
        return -1;
    }

    *status_out = db_bg_task_get_status(g_worker.slots[slot].task->ctx);
    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}

/**
 * @brief 获取调度器统计信息
 * @param stats_out 输出统计信息
 * @return 0 成功
 */
int db_bgworker_get_stats(db_bgworker_stats_t *stats_out) {
    if (!stats_out) {
        return -1;
    }

    pthread_mutex_lock(&g_worker.mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < g_worker.num_slots; i++) {
        if (g_worker.slots[i].in_use) {
            count++;
        }
    }

    stats_out->task_count = count;
    stats_out->running_time_ms = atomic_load_explicit(&g_worker.running, memory_order_acquire)
        ? (_db_bgworker_now_ms() - g_worker.start_time_ms)
        : 0;
    stats_out->total_cycles = 0;
    stats_out->total_executions = 0;

    pthread_mutex_unlock(&g_worker.mutex);
    return 0;
}
