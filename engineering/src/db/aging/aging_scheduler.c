/**
 * @file aging_scheduler.c
 * @brief 老化调度器实现
 *
 * 实现定时任务调度，执行数据分层和清理策略。
 */
#include "db/aging/aging.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(seconds) Sleep((seconds) * 1000)
#else
#include <unistd.h>
#endif

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/**
 * @brief 调度任务
 */
typedef struct scheduler_task_s {
    aging_action_t action;      /**< 执行动作 */
    void *data_id;              /**< 数据标识符 */
    int32_t tier;               /**< 当前层级 */
    int64_t data_size;          /**< 数据大小 */
    float temperature;          /**< 热度评分 */
    uint64_t scheduled_time;    /**< 计划执行时间 */
    struct scheduler_task_s *next;
} scheduler_task_t;

/**
 * @brief 老化调度器
 */
typedef struct aging_scheduler_s {
    aging_manager_t *manager;   /**< 老化管理器 */
    pthread_t thread;           /**< 调度线程 */
    bool running;               /**< 运行状态 */
    bool stopped;               /**< 停止标志 */
    uint64_t interval_seconds;  /**< 调度间隔（秒） */

    /* 任务队列 */
    scheduler_task_t *task_queue;
    int32_t task_count;
    pthread_mutex_t queue_mutex;
} aging_scheduler_t;

/* ========================================================================
 * 内部函数
 * ======================================================================== */

/**
 * @brief 评估数据热度并生成动作
 */
static aging_action_t evaluate_data(aging_scheduler_t *scheduler,
                                     void *data_id,
                                     data_temperature_t *temp) {
    if (scheduler == NULL || data_id == NULL || temp == NULL) {
        return AGING_ACTION_KEEP;
    }

    /* 评估热度 */
    int32_t target_tier = aging_evaluate_tier(scheduler->manager, temp);
    aging_action_t action = aging_recommend_action(scheduler->manager, temp);

    /* 检查容量约束 */
    if (action != AGING_ACTION_KEEP && action != AGING_ACTION_DELETE) {
        if (aging_needs_eviction(scheduler->manager, target_tier, temp->current_tier > 0 ? 1024 : 0)) {
            /* 容量不足时优先删除 */
            if (temp->temperature < 0.1f) {
                action = AGING_ACTION_DELETE;
            } else if (temp->temperature < 0.3f) {
                action = AGING_ACTION_ARCHIVE;
            }
        }
    }

    return action;
}

/**
 * @brief 添加任务到队列
 */
static int add_task_to_queue(aging_scheduler_t *scheduler,
                             const scheduler_task_t *task) {
    if (scheduler == NULL || task == NULL) return -1;

    scheduler_task_t *new_task = (scheduler_task_t *)malloc(sizeof(scheduler_task_t));
    if (new_task == NULL) return -1;

    *new_task = *task;
    new_task->next = NULL;

    pthread_mutex_lock(&scheduler->queue_mutex);

    /* 添加到队列尾部 */
    if (scheduler->task_queue == NULL) {
        scheduler->task_queue = new_task;
    } else {
        scheduler_task_t *p = scheduler->task_queue;
        while (p->next != NULL) p = p->next;
        p->next = new_task;
    }
    scheduler->task_count++;

    pthread_mutex_unlock(&scheduler->queue_mutex);

    return 0;
}

/**
 * @brief 从队列获取任务
 */
static scheduler_task_t *pop_task_from_queue(aging_scheduler_t *scheduler) {
    if (scheduler == NULL) return NULL;

    pthread_mutex_lock(&scheduler->queue_mutex);

    if (scheduler->task_queue == NULL) {
        pthread_mutex_unlock(&scheduler->queue_mutex);
        return NULL;
    }

    scheduler_task_t *task = scheduler->task_queue;
    scheduler->task_queue = task->next;
    scheduler->task_count--;

    pthread_mutex_unlock(&scheduler->queue_mutex);

    return task;
}

/**
 * @brief 执行老化任务
 */
static int execute_task(aging_scheduler_t *scheduler, const scheduler_task_t *task) {
    if (scheduler == NULL || task == NULL) return -1;

    switch (task->action) {
        case AGING_ACTION_DELETE:
            aging_delete(scheduler->manager, task->data_id, NULL, NULL);
            LOG_DEBUG("调度删除: data_id=%p", task->data_id);
            break;

        case AGING_ACTION_ARCHIVE:
            aging_archive(scheduler->manager, task->data_id, NULL, NULL);
            LOG_DEBUG("调度归档: data_id=%p", task->data_id);
            break;

        case AGING_ACTION_MOVE_WARM:
            /* TODO: 实现数据移动 */
            LOG_DEBUG("调度移动到温层: data_id=%p", task->data_id);
            break;

        case AGING_ACTION_MOVE_COLD:
            /* TODO: 实现数据移动 */
            LOG_DEBUG("调度移动到冷层: data_id=%p", task->data_id);
            break;

        case AGING_ACTION_MOVE_HOT:
            /* TODO: 实现数据移动 */
            LOG_DEBUG("调度移动到热层: data_id=%p", task->data_id);
            break;

        case AGING_ACTION_KEEP:
        default:
            break;
    }

    return 0;
}

/**
 * @brief 调度线程主函数
 */
static void *scheduler_thread_main(void *arg) {
    aging_scheduler_t *scheduler = (aging_scheduler_t *)arg;

    LOG_INFO("老化调度线程启动: interval=%lu秒", scheduler->interval_seconds);

    while (!scheduler->stopped) {
        /* 等待下一个调度周期 */
        sleep((int)scheduler->interval_seconds);

        if (scheduler->stopped) break;

        LOG_DEBUG("老化调度开始...");

        /* 执行扫描并生成任务 */
        aging_action_t actions[1024];
        int32_t count = aging_schedule(scheduler->manager, actions, 1024);

        LOG_DEBUG("老化调度完成: 生成 %d 个动作", count);

        /* 更新运行时间 */
        scheduler->manager->last_run_time = (uint64_t)time(NULL);
    }

    LOG_INFO("老化调度线程退出");
    return NULL;
}

/* ========================================================================
 * 调度器 API 实现
 * ======================================================================== */

aging_scheduler_t *aging_scheduler_create(aging_manager_t *mgr,
                                           uint64_t interval_seconds) {
    if (mgr == NULL) {
        LOG_ERROR("调度器创建失败: manager 为空");
        return NULL;
    }

    aging_scheduler_t *scheduler = (aging_scheduler_t *)calloc(1, sizeof(aging_scheduler_t));
    if (scheduler == NULL) {
        LOG_ERROR("调度器分配失败");
        return NULL;
    }

    scheduler->manager = mgr;
    scheduler->interval_seconds = interval_seconds > 0 ? interval_seconds : AGING_DEFAULT_SCHEDULE_INTERVAL;
    scheduler->running = false;
    scheduler->stopped = false;
    scheduler->task_queue = NULL;
    scheduler->task_count = 0;

    pthread_mutex_init(&scheduler->queue_mutex, NULL);

    LOG_INFO("老化调度器创建成功: interval=%lu秒", scheduler->interval_seconds);
    return scheduler;
}

void aging_scheduler_destroy(aging_scheduler_t *scheduler) {
    if (scheduler == NULL) return;

    /* 停止调度线程 */
    if (scheduler->running) {
        aging_scheduler_stop(scheduler);
    }

    /* 清理任务队列 */
    pthread_mutex_lock(&scheduler->queue_mutex);
    scheduler_task_t *p = scheduler->task_queue;
    while (p != NULL) {
        scheduler_task_t *next = p->next;
        free(p);
        p = next;
    }
    pthread_mutex_unlock(&scheduler->queue_mutex);

    pthread_mutex_destroy(&scheduler->queue_mutex);
    free(scheduler);

    LOG_INFO("老化调度器已销毁");
}

int aging_scheduler_start(aging_scheduler_t *scheduler) {
    if (scheduler == NULL) return -1;

    if (scheduler->running) {
        LOG_WARN("调度器已在运行");
        return 0;
    }

    scheduler->stopped = false;

    int ret = pthread_create(&scheduler->thread, NULL, scheduler_thread_main, scheduler);
    if (ret != 0) {
        LOG_ERROR("调度器线程创建失败: ret=%d", ret);
        return -1;
    }

    scheduler->running = true;
    LOG_INFO("老化调度器已启动");
    return 0;
}

int aging_scheduler_stop(aging_scheduler_t *scheduler) {
    if (scheduler == NULL) return -1;

    if (!scheduler->running) {
        return 0;
    }

    scheduler->stopped = true;
    pthread_join(scheduler->thread, NULL);

    scheduler->running = false;
    LOG_INFO("老化调度器已停止");
    return 0;
}

bool aging_scheduler_is_running(const aging_scheduler_t *scheduler) {
    if (scheduler == NULL) return false;
    return scheduler->running;
}

uint64_t aging_scheduler_get_interval(const aging_scheduler_t *scheduler) {
    if (scheduler == NULL) return 0;
    return scheduler->interval_seconds;
}

int aging_scheduler_set_interval(aging_scheduler_t *scheduler, uint64_t interval_seconds) {
    if (scheduler == NULL) return -1;

    if (interval_seconds <= 0) {
        interval_seconds = AGING_DEFAULT_SCHEDULE_INTERVAL;
    }

    scheduler->interval_seconds = interval_seconds;
    LOG_INFO("调度间隔已更新: %lu秒", interval_seconds);
    return 0;
}

/* ========================================================================
 * 批量处理 API 实现
 * ======================================================================== */

/**
 * @brief 批量评估数据
 *
 * @param scheduler 调度器
 * @param data_ids 数据 ID 数组
 * @param temps 热度信息数组
 * @param count 数据数量
 * @param out_actions 输出动作数组
 * @param max_actions 最大动作数
 * @return 实际动作数
 */
int32_t aging_scheduler_batch_evaluate(aging_scheduler_t *scheduler,
                                       void **data_ids,
                                       data_temperature_t *temps,
                                       int32_t count,
                                       aging_action_t *out_actions,
                                       int32_t max_actions) {
    if (scheduler == NULL || data_ids == NULL || temps == NULL ||
        out_actions == NULL || max_actions <= 0) {
        return 0;
    }

    int32_t action_count = 0;

    for (int32_t i = 0; i < count && action_count < max_actions; i++) {
        aging_action_t action = evaluate_data(scheduler, data_ids[i], &temps[i]);

        if (action != AGING_ACTION_KEEP) {
            out_actions[action_count++] = action;

            /* 添加到任务队列 */
            scheduler_task_t task = {
                .action = action,
                .data_id = data_ids[i],
                .tier = temps[i].current_tier,
                .data_size = 0,
                .temperature = temps[i].temperature,
                .scheduled_time = (uint64_t)time(NULL),
                .next = NULL
            };
            add_task_to_queue(scheduler, &task);
        }
    }

    return action_count;
}

/**
 * @brief 执行所有待处理任务
 *
 * @param scheduler 调度器
 * @param max_tasks 最大执行任务数
 * @return 实际执行数
 */
int32_t aging_scheduler_process_queue(aging_scheduler_t *scheduler, int32_t max_tasks) {
    if (scheduler == NULL || max_tasks <= 0) return 0;

    int32_t processed = 0;

    while (processed < max_tasks) {
        scheduler_task_t *task = pop_task_from_queue(scheduler);
        if (task == NULL) break;

        execute_task(scheduler, task);
        free(task);
        processed++;
    }

    return processed;
}

/**
 * @brief 获取待处理任务数
 *
 * @param scheduler 调度器
 * @return 待处理任务数
 */
int32_t aging_scheduler_get_queue_size(const aging_scheduler_t *scheduler) {
    if (scheduler == NULL) return 0;
    return scheduler->task_count;
}

/**
 * @brief 强制立即执行一次调度
 *
 * @param scheduler 调度器
 * @return 0 成功，-1 失败
 */
int aging_scheduler_run_once(aging_scheduler_t *scheduler) {
    if (scheduler == NULL) return -1;

    LOG_INFO("强制执行老化调度...");

    /* 执行调度 */
    aging_action_t actions[1024];
    int32_t count = aging_schedule(scheduler->manager, actions, 1024);

    LOG_INFO("老化调度完成: 生成 %d 个动作", count);

    /* 更新运行时间 */
    scheduler->manager->last_run_time = (uint64_t)time(NULL);

    return 0;
}

/* ========================================================================
 * 统计 API 实现
 * ======================================================================== */

/**
 * @brief 获取调度器统计信息
 *
 * @param scheduler 调度器
 * @param stats 输出统计
 */
void aging_scheduler_get_stats(const aging_scheduler_t *scheduler,
                               aging_scheduler_stats_t *stats) {
    if (scheduler == NULL || stats == NULL) return;

    stats->running = scheduler->running;
    stats->interval_seconds = scheduler->interval_seconds;
    stats->queue_size = scheduler->task_count;
    stats->last_run_time = scheduler->manager->last_run_time;
}
