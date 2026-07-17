/**
 * @file merge_scheduler.c
 * @brief 后台合并调度器实现
 */

#include "merge_scheduler.h"
#include "vector_write_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/**
 * @brief 任务队列节点
 */
typedef struct merge_task_node {
    merge_task_t task;
    struct merge_task_node *next;
} merge_task_node_t;

/**
 * @brief 任务队列
 */
typedef struct merge_task_queue {
    merge_task_node_t *head;
    merge_task_node_t *tail;
    int32_t size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} merge_task_queue_t;

/**
 * @brief 合并调度器内部结构
 */
struct merge_scheduler {
    vector_index_t *index;         /* 主索引 */
    merge_scheduler_config_t config;

    /* 任务队列 */
    merge_task_queue_t task_queue;

    /* 后台线程 */
    pthread_t thread;
    bool thread_running;
    bool thread_should_stop;

    /* 同步 */
    pthread_mutex_t state_mutex;
    pthread_cond_t done_cond;

    /* 统计 */
    int32_t completed_tasks;
    int64_t total_vectors_merged;

    /* 当前任务进度 */
    merge_task_type_t current_task_type;
    int32_t current_progress;
    int32_t current_total;

    /* 回调 */
    merge_progress_callback_t callback;
    void *callback_ctx;

    /* 定时器 */
    time_t last_flush_time;
};

/* ========================================================================
 * 任务队列操作
 * ======================================================================== */

static merge_task_queue_t *task_queue_create(void) {
    merge_task_queue_t *queue = (merge_task_queue_t *)calloc(1, sizeof(merge_task_queue_t));
    if (queue == NULL) {
        return NULL;
    }

    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    return queue;
}

static void task_queue_destroy(merge_task_queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    pthread_mutex_lock(&queue->mutex);

    /* 释放所有节点 */
    merge_task_node_t *node = queue->head;
    while (node != NULL) {
        merge_task_node_t *next = node->next;
        free(node);
        node = next;
    }

    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);

    free(queue);
}

static int32_t task_queue_push(merge_task_queue_t *queue, const merge_task_t *task) {
    if (queue == NULL || task == NULL) {
        return -1;
    }

    merge_task_node_t *node = (merge_task_node_t *)malloc(sizeof(merge_task_node_t));
    if (node == NULL) {
        return -1;
    }

    node->task = *task;
    node->next = NULL;

    pthread_mutex_lock(&queue->mutex);

    if (queue->tail == NULL) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }

    queue->size++;
    pthread_cond_signal(&queue->cond);

    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

static int32_t task_queue_pop(merge_task_queue_t *queue, merge_task_t *task) {
    if (queue == NULL || task == NULL) {
        return -1;
    }

    pthread_mutex_lock(&queue->mutex);

    while (queue->head == NULL && !pthread_cond_wait(&queue->cond, &queue->mutex) == 0) {
        /* 如果队列为空，等待信号 */
    }

    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    merge_task_node_t *node = queue->head;
    *task = node->task;
    queue->head = node->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    queue->size--;
    free(node);

    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

static int32_t task_queue_size(const merge_task_queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }

    pthread_mutex_lock((pthread_mutex_t *)&queue->mutex);
    int32_t size = queue->size;
    pthread_mutex_unlock((pthread_mutex_t *)&queue->mutex);

    return size;
}

static void task_queue_clear(merge_task_queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    pthread_mutex_lock(&queue->mutex);

    merge_task_node_t *node = queue->head;
    while (node != NULL) {
        merge_task_node_t *next = node->next;
        free(node);
        node = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    pthread_mutex_unlock(&queue->mutex);
}

/* ========================================================================
 * 配置
 * ======================================================================== */

merge_scheduler_config_t merge_scheduler_config_default(void) {
    merge_scheduler_config_t config = {
        .flush_interval_ms = MERGE_SCHEDULER_DEFAULT_INTERVAL_MS,
        .max_pending_tasks = MERGE_SCHEDULER_DEFAULT_MAX_PENDING,
        .merge_batch_size = MERGE_SCHEDULER_DEFAULT_BATCH_SIZE,
        .auto_flush = true
    };
    return config;
}

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

merge_scheduler_t *merge_scheduler_create(
    vector_index_t *index,
    const merge_scheduler_config_t *config) {

    if (index == NULL) {
        return NULL;
    }

    merge_scheduler_t *sched = (merge_scheduler_t *)calloc(1, sizeof(merge_scheduler_t));
    if (sched == NULL) {
        return NULL;
    }

    sched->index = index;

    if (config != NULL) {
        sched->config = *config;
    } else {
        sched->config = merge_scheduler_config_default();
    }

    /* 初始化任务队列 */
    sched->task_queue.head = NULL;
    sched->task_queue.tail = NULL;
    sched->task_queue.size = 0;
    pthread_mutex_init(&sched->task_queue.mutex, NULL);
    pthread_cond_init(&sched->task_queue.cond, NULL);

    /* 初始化同步原语 */
    pthread_mutex_init(&sched->state_mutex, NULL);
    pthread_cond_init(&sched->done_cond, NULL);

    sched->thread_running = false;
    sched->thread_should_stop = false;
    sched->completed_tasks = 0;
    sched->total_vectors_merged = 0;
    sched->last_flush_time = time(NULL);

    return sched;
}

void merge_scheduler_destroy(merge_scheduler_t *sched) {
    if (sched == NULL) {
        return;
    }

    /* 停止后台线程 */
    if (sched->thread_running) {
        merge_scheduler_stop(sched);
    }

    /* 清理任务队列 */
    task_queue_clear(&sched->task_queue);
    pthread_mutex_destroy(&sched->task_queue.mutex);
    pthread_cond_destroy(&sched->task_queue.cond);

    pthread_mutex_destroy(&sched->state_mutex);
    pthread_cond_destroy(&sched->done_cond);

    free(sched);
}

/* ========================================================================
 * 后台线程
 * ======================================================================== */

/**
 * @brief 后台合并线程主函数
 */
static void *merge_thread_main(void *arg) {
    merge_scheduler_t *sched = (merge_scheduler_t *)arg;

    pthread_mutex_lock(&sched->state_mutex);
    sched->thread_running = true;
    pthread_mutex_unlock(&sched->state_mutex);

    while (!sched->thread_should_stop) {
        merge_task_t task;
        int32_t result = task_queue_pop(&sched->task_queue, &task);

        if (result == 0) {
            /* 执行合并任务 */
            switch (task.type) {
            case MERGE_TASK_FLUSH:
                /* 刷新缓冲区到主索引 */
                if (task.data != NULL && task.vector_count > 0) {
                    /* TODO: 调用 vector_index_add 或类似接口 */
                    /* 这里简化处理，实际需要调用 HNSW/DiskANN 的插入接口 */
                    sched->total_vectors_merged += task.vector_count;
                }
                break;

            case MERGE_TASK_REBUILD:
                /* 重建索引 */
                /* TODO: 调用索引重建接口 */
                break;

            case MERGE_TASK_OPTIMIZE:
                /* 图优化 */
                /* TODO: 调用图优化接口 */
                break;
            }

            sched->completed_tasks++;

            /* 触发回调 */
            if (sched->callback != NULL) {
                sched->callback(task.vector_count, task.vector_count, task.type, sched->callback_ctx);
            }
        }

        /* 检查定时刷新 */
        if (sched->config.auto_flush) {
            time_t now = time(NULL);
            if (now - sched->last_flush_time >= sched->config.flush_interval_ms / 1000) {
                /* 时间到了但队列为空，可以休眠一会儿 */
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = 100000000;  /* 100ms */
                nanosleep(&ts, NULL);
            }
        }
    }

    pthread_mutex_lock(&sched->state_mutex);
    sched->thread_running = false;
    pthread_mutex_unlock(&sched->state_mutex);

    return NULL;
}

int32_t merge_scheduler_start(merge_scheduler_t *sched) {
    if (sched == NULL) {
        return -1;
    }

    pthread_mutex_lock(&sched->state_mutex);

    if (sched->thread_running) {
        pthread_mutex_unlock(&sched->state_mutex);
        return 0;  /* 已经运行 */
    }

    sched->thread_should_stop = false;

    if (pthread_create(&sched->thread, NULL, merge_thread_main, sched) != 0) {
        pthread_mutex_unlock(&sched->state_mutex);
        return -1;
    }

    pthread_mutex_unlock(&sched->state_mutex);

    return 0;
}

int32_t merge_scheduler_stop(merge_scheduler_t *sched) {
    if (sched == NULL) {
        return -1;
    }

    pthread_mutex_lock(&sched->state_mutex);

    if (!sched->thread_running) {
        pthread_mutex_unlock(&sched->state_mutex);
        return 0;  /* 已经停止 */
    }

    sched->thread_should_stop = true;
    pthread_cond_signal(&sched->task_queue.cond);  /* 唤醒线程以便退出 */

    pthread_mutex_unlock(&sched->state_mutex);

    /* 等待线程结束 */
    pthread_join(sched->thread, NULL);

    return 0;
}

/* ========================================================================
 * 任务提交
 * ======================================================================== */

int32_t merge_scheduler_trigger(merge_scheduler_t *sched, const merge_task_t *task) {
    if (sched == NULL || task == NULL) {
        return -1;
    }

    /* 检查队列是否已满 */
    if (task_queue_size(&sched->task_queue) >= sched->config.max_pending_tasks) {
        return -1;  /* 队列满 */
    }

    return task_queue_push(&sched->task_queue, task);
}

int32_t merge_scheduler_flush(merge_scheduler_t *sched,
                               void *buffer,
                               float *vectors,
                               int32_t max_n) {
    if (sched == NULL || buffer == NULL || vectors == NULL) {
        return -1;
    }

    /* 从缓冲区获取数据 */
    int32_t flushed = vector_buffer_flush((vector_write_buffer_t *)buffer, vectors, max_n);
    if (flushed <= 0) {
        return 0;
    }

    /* 创建合并任务 */
    merge_task_t task = {
        .type = MERGE_TASK_FLUSH,
        .data = vectors,
        .vector_count = flushed
    };

    /* 更新最后刷新时间 */
    sched->last_flush_time = time(NULL);

    return merge_scheduler_trigger(sched, &task);
}

int32_t merge_scheduler_wait(merge_scheduler_t *sched) {
    if (sched == NULL) {
        return -1;
    }

    pthread_mutex_lock(&sched->state_mutex);

    /* 等待队列为空 */
    while (task_queue_size(&sched->task_queue) > 0) {
        pthread_cond_wait(&sched->done_cond, &sched->state_mutex);
    }

    pthread_mutex_unlock(&sched->state_mutex);

    return 0;
}

void merge_scheduler_cancel_pending(merge_scheduler_t *sched) {
    if (sched == NULL) {
        return;
    }

    task_queue_clear(&sched->task_queue);
}

/* ========================================================================
 * 配置更新
 * ======================================================================== */

int32_t merge_scheduler_set_interval(merge_scheduler_t *sched, int32_t interval_ms) {
    if (sched == NULL || interval_ms <= 0) {
        return -1;
    }

    sched->config.flush_interval_ms = interval_ms;
    return 0;
}

void merge_scheduler_set_callback(merge_scheduler_t *sched,
                                   merge_progress_callback_t callback,
                                   void *ctx) {
    if (sched == NULL) {
        return;
    }

    sched->callback = callback;
    sched->callback_ctx = ctx;
}

/* ========================================================================
 * 状态查询
 * ======================================================================== */

void merge_scheduler_get_stats(const merge_scheduler_t *sched,
                                merge_scheduler_stats_t *stats) {
    if (sched == NULL || stats == NULL) {
        return;
    }

    pthread_mutex_lock((pthread_mutex_t *)&sched->state_mutex);
    stats->running = sched->thread_running;
    pthread_mutex_unlock((pthread_mutex_t *)&sched->state_mutex);

    stats->pending_tasks = task_queue_size(&sched->task_queue);
    stats->completed_tasks = sched->completed_tasks;
    stats->total_vectors_merged = sched->total_vectors_merged;
    stats->current_task_type = sched->current_task_type;
    stats->current_progress = sched->current_progress;
    stats->current_total = sched->current_total;
}
