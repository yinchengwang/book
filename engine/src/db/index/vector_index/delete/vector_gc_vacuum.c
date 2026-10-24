/*
 * 向量索引垃圾回收器实现
 *
 * 后台线程扫描已删除向量，执行物理删除。
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <db/index/vector_index/delete/vector_gc_vacuum.h>
#include <db/index/vector_index/delete/vector_delete_bitmap.h>

/* ============================================================================
 * 内部结构
 * ============================================================================ */

struct gc_vacuum {
    /* 依赖 */
    void                *delete_bitmap;     /* vector_delete_bitmap_t* */
    void                (*remove_callback)(void *ctx, int32_t id);  /* 物理删除回调 */
    void                *remove_ctx;

    /* 配置 */
    gc_config_t         config;

    /* 状态 */
    gc_state_t          state;
    pthread_mutex_t     state_lock;
    pthread_t           thread;
    bool                stop_flag;

    /* 统计 */
    gc_stats_t          stats;
    pthread_mutex_t     stats_lock;

    /* 上次扫描位置（用于增量扫描） */
    int32_t             last_scan_pos;
};

/* ============================================================================
 * 默认配置
 * ============================================================================ */

void gc_config_init_default(gc_config_t *config)
{
    if (!config) return;

    config->batch_size           = 100;
    config->min_deleted_ratio   = 10;   /* 10% */
    config->max_concurrency     = 4;
    config->auto_vacuum         = true;
    config->vacuum_interval_sec = 60;   /* 1 分钟 */
}

/* ============================================================================
 * 创建/销毁
 * ============================================================================ */

gc_vacuum_t *gc_vacuum_create(void *delete_bitmap, const gc_config_t *config)
{
    if (!delete_bitmap) {
        return NULL;
    }

    gc_vacuum_t *vacuum = (gc_vacuum_t *)malloc(sizeof(gc_vacuum_t));
    if (!vacuum) {
        return NULL;
    }

    memset(vacuum, 0, sizeof(gc_vacuum_t));

    vacuum->delete_bitmap = delete_bitmap;

    /* 初始化配置 */
    if (config) {
        vacuum->config = *config;
    } else {
        gc_config_init_default(&vacuum->config);
    }

    /* 初始化锁 */
    pthread_mutex_init(&vacuum->state_lock, NULL);
    pthread_mutex_init(&vacuum->stats_lock, NULL);

    vacuum->state         = GC_STATE_IDLE;
    vacuum->last_scan_pos = 0;

    return vacuum;
}

void gc_vacuum_destroy(gc_vacuum_t *vacuum)
{
    if (!vacuum) {
        return;
    }

    /* 确保线程已停止 */
    gc_vacuum_stop(vacuum);

    pthread_mutex_destroy(&vacuum->state_lock);
    pthread_mutex_destroy(&vacuum->stats_lock);

    free(vacuum);
}

/* ============================================================================
 * 状态管理
 * ============================================================================ */

static void gc_set_state(gc_vacuum_t *vacuum, gc_state_t new_state)
{
    pthread_mutex_lock(&vacuum->state_lock);
    vacuum->state = new_state;
    pthread_mutex_unlock(&vacuum->state_lock);
}

gc_state_t gc_vacuum_get_state(const gc_vacuum_t *vacuum)
{
    if (!vacuum) {
        return GC_STATE_IDLE;
    }

    pthread_mutex_lock((pthread_mutex_t *)&vacuum->state_lock);
    gc_state_t state = vacuum->state;
    pthread_mutex_unlock((pthread_mutex_t *)&vacuum->state_lock);

    return state;
}

/* ============================================================================
 * 统计管理
 * ============================================================================ */

void gc_vacuum_get_stats(const gc_vacuum_t *vacuum, gc_stats_t *stats)
{
    if (!vacuum || !stats) {
        return;
    }

    pthread_mutex_lock((pthread_mutex_t *)&vacuum->stats_lock);
    *stats = vacuum->stats;
    pthread_mutex_unlock((pthread_mutex_t *)&vacuum->stats_lock);
}

static void gc_update_stats(gc_vacuum_t *vacuum, int32_t vacuumed, int32_t scanned, int64_t time_us)
{
    pthread_mutex_lock(&vacuum->stats_lock);
    vacuum->stats.total_vacuumed    += vacuumed;
    vacuum->stats.total_scanned     += scanned;
    vacuum->stats.total_time_us     += time_us;
    vacuum->stats.last_vacuumed     = vacuumed;
    vacuum->stats.last_scan_time_us = (int32_t)time_us;
    pthread_mutex_unlock(&vacuum->stats_lock);
}

/* ============================================================================
 * 后台 GC 线程
 * ============================================================================ */

static int64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void *gc_background_thread(void *arg)
{
    gc_vacuum_t *vacuum = (gc_vacuum_t *)arg;

    while (!vacuum->stop_flag) {
        gc_set_state(vacuum, GC_STATE_IDLE);

        /* 休眠间隔 */
        struct timespec ts;
        ts.tv_sec  = vacuum->config.vacuum_interval_sec;
        ts.tv_nsec = 0;
        nanosleep(&ts, NULL);

        if (vacuum->stop_flag) {
            break;
        }

        /* 检查是否满足自动 GC 条件 */
        vector_delete_stats_t del_stats;
        if (vector_delete_get_stats(vacuum->delete_bitmap, &del_stats) != 0) {
            continue;
        }

        /* 删除比例低于阈值，跳过 */
        if (del_stats.deleted_ratio * 100 < vacuum->config.min_deleted_ratio) {
            continue;
        }

        /* 执行 GC */
        gc_set_state(vacuum, GC_STATE_SCANNING);
        gc_vacuum_execute(vacuum, GC_SCAN_MODE_INCREMENTAL, NULL, NULL);
    }

    gc_set_state(vacuum, GC_STATE_IDLE);
    return NULL;
}

int32_t gc_vacuum_start(gc_vacuum_t *vacuum)
{
    if (!vacuum) {
        return -1;
    }

    pthread_mutex_lock(&vacuum->state_lock);
    if (vacuum->state != GC_STATE_IDLE) {
        pthread_mutex_unlock(&vacuum->state_lock);
        return 0;  /* 已经在运行 */
    }

    vacuum->stop_flag = false;

    if (pthread_create(&vacuum->thread, NULL, gc_background_thread, vacuum) != 0) {
        pthread_mutex_unlock(&vacuum->state_lock);
        return -1;
    }

    pthread_mutex_unlock(&vacuum->state_lock);
    return 0;
}

int32_t gc_vacuum_stop(gc_vacuum_t *vacuum)
{
    if (!vacuum) {
        return -1;
    }

    pthread_mutex_lock(&vacuum->state_lock);
    if (vacuum->state == GC_STATE_IDLE) {
        pthread_mutex_unlock(&vacuum->state_lock);
        return 0;
    }

    vacuum->stop_flag = true;
    gc_set_state(vacuum, GC_STATE_STOPPING);
    pthread_mutex_unlock(&vacuum->state_lock);

    pthread_join(vacuum->thread, NULL);

    return 0;
}

/* ============================================================================
 * GC 执行
 * ============================================================================ */

int32_t gc_vacuum_execute(gc_vacuum_t *vacuum,
                          gc_scan_mode_t mode,
                          gc_progress_callback_t callback,
                          void *ctx)
{
    if (!vacuum) {
        return -1;
    }

    int64_t start_time = get_time_us();

    /* 获取已删除向量列表 */
    vector_delete_stats_t stats;
    if (vector_delete_get_stats(vacuum->delete_bitmap, &stats) != 0) {
        return -1;
    }

    if (stats.deleted_count == 0) {
        return 0;
    }

    /* 分配临时缓冲区 */
    int32_t *deleted_ids = (int32_t *)malloc(sizeof(int32_t) * stats.deleted_count);
    if (!deleted_ids) {
        return -1;
    }

    int32_t num_deleted = vector_delete_get_deleted_ids(
        vacuum->delete_bitmap, deleted_ids, stats.deleted_count);

    if (num_deleted == 0) {
        free(deleted_ids);
        return 0;
    }

    /* 批量处理 */
    int32_t processed = 0;
    int32_t batch_size = vacuum->config.batch_size > 0 ? vacuum->config.batch_size : 100;

    gc_set_state(vacuum, GC_STATE_SCANNING);

    for (int32_t i = 0; i < num_deleted && !vacuum->stop_flag; i += batch_size) {
        int32_t end = (i + batch_size < num_deleted) ? i + batch_size : num_deleted;

        /* 执行物理删除回调 */
        if (vacuum->remove_callback) {
            for (int32_t j = i; j < end; j++) {
                vacuum->remove_callback(vacuum->remove_ctx, deleted_ids[j]);
            }
        }

        processed += (end - i);

        /* 回调进度 */
        if (callback) {
            callback(processed, num_deleted, ctx);
        }
    }

    gc_set_state(vacuum, GC_STATE_REPAIRING);

    /* 清空删除位图（所有已删除向量已被物理删除） */
    vector_delete_clear(vacuum->delete_bitmap);

    /* 重置增量扫描位置 */
    vacuum->last_scan_pos = 0;

    /* 更新统计 */
    int64_t elapsed = get_time_us() - start_time;
    gc_update_stats(vacuum, processed, num_deleted, elapsed);

    free(deleted_ids);

    gc_set_state(vacuum, GC_STATE_IDLE);

    return processed;
}

/* ============================================================================
 * 配置更新
 * ============================================================================ */

int32_t gc_vacuum_update_config(gc_vacuum_t *vacuum, const gc_config_t *config)
{
    if (!vacuum || !config) {
        return -1;
    }

    pthread_mutex_lock(&vacuum->state_lock);
    vacuum->config = *config;
    pthread_mutex_unlock(&vacuum->state_lock);

    return 0;
}
