/**
 * @file checkpoint_manager.c
 * @brief Vector WAL 后台 CheckpointManager 实现（P1-B Task 7）
 *
 * pthread 后台线程，固定周期执行：
 *     vector_wal_flush → vector_wal_checkpoint → vector_wal_truncate
 *
 * 关键不变式：
 *  - mgr->wal 在 destroy 期间必须仍有效，故 vector_api_destroy 必须先停 mgr 再关 wal
 *  - cond_timedwait 使用绝对时间（CLOCK_REALTIME），避免相对时间漂移
 *  - stop 时 broadcast cond 并 join，线程保证退出后再 free
 */
#include <db/storage/vector/checkpoint_manager.h>
#include <db/core/log.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* ============================================================
 * 内部结构
 * ============================================================ */

struct CheckpointManager_s {
    vector_wal_t   *wal;                 /**< 目标 WAL（不由 mgr 拥有） */
    uint32_t        interval_ms;         /**< 周期（毫秒） */
    struct timespec interval_timespec;   /**< 周期（timespec 形式，给 cond_timedwait 用） */

    pthread_t       thread;              /**< 后台线程句柄 */
    pthread_mutex_t mutex;               /**< 保护 cond 唤醒 */
    pthread_cond_t  cond;                /**< 唤醒条件变量 */

    atomic_bool     running;             /**< 运行标志 */
    atomic_bool     started;             /**< 已启动标志 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 把 interval_ms 转为 timespec（pthread_cond_timedwait 用） */
static void ms_to_timespec(uint32_t ms, struct timespec *ts) {
    ts->tv_sec  = (time_t)(ms / 1000);
    ts->tv_nsec = (long)(ms % 1000) * 1000000L;
}

/* ============================================================
 * 后台线程主体
 * ============================================================ */

static void *checkpoint_thread_main(void *arg) {
    CheckpointManager *mgr = (CheckpointManager *)arg;
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);

    while (atomic_load(&mgr->running)) {
        /* 计算下一次唤醒的绝对时间：deadline += interval */
        deadline.tv_sec  += mgr->interval_timespec.tv_sec;
        deadline.tv_nsec += mgr->interval_timespec.tv_nsec;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec  += 1;
            deadline.tv_nsec -= 1000000000L;
        }

        pthread_mutex_lock(&mgr->mutex);
        /* 等待至 deadline，或被 stop 的 broadcast 唤醒 */
        int rc = pthread_cond_timedwait(&mgr->cond, &mgr->mutex, &deadline);
        if (!atomic_load(&mgr->running)) {
            pthread_mutex_unlock(&mgr->mutex);
            break;
        }

        /* flush + checkpoint + truncate（三步合一） */
        if (mgr->wal) {
            if (vector_wal_flush(mgr->wal) != 0) {
                LOG_WARN("CheckpointManager: flush 失败");
            }
            if (vector_wal_checkpoint(mgr->wal, VECTOR_CHECKPOINT_TIMED) != 0) {
                LOG_WARN("CheckpointManager: checkpoint 失败");
            }
            if (vector_wal_truncate(mgr->wal) != 0) {
                LOG_WARN("CheckpointManager: truncate 失败");
            }
        }
        pthread_mutex_unlock(&mgr->mutex);

        /* rc 可为 ETIMEDOUT（正常唤醒）或 0（被 stop 唤醒），不需处理 */
        (void)rc;
    }
    return NULL;
}

/* ============================================================
 * 公开 API
 * ============================================================ */

CheckpointManager *checkpoint_manager_create(vector_wal_t *wal, uint32_t interval_ms) {
    if (!wal) return NULL;

    CheckpointManager *mgr = (CheckpointManager *)calloc(1, sizeof(CheckpointManager));
    if (!mgr) {
        LOG_ERROR("CheckpointManager: 分配失败");
        return NULL;
    }

    mgr->wal = wal;
    mgr->interval_ms = interval_ms;
    ms_to_timespec(interval_ms, &mgr->interval_timespec);

    if (pthread_mutex_init(&mgr->mutex, NULL) != 0) {
        LOG_ERROR("CheckpointManager: mutex init 失败");
        free(mgr);
        return NULL;
    }
    if (pthread_cond_init(&mgr->cond, NULL) != 0) {
        LOG_ERROR("CheckpointManager: cond init 失败");
        pthread_mutex_destroy(&mgr->mutex);
        free(mgr);
        return NULL;
    }

    atomic_store(&mgr->running, false);
    atomic_store(&mgr->started, false);
    return mgr;
}

void checkpoint_manager_destroy(CheckpointManager *mgr) {
    if (!mgr) return;
    if (atomic_load(&mgr->started)) {
        checkpoint_manager_stop(mgr);
    }
    pthread_mutex_destroy(&mgr->mutex);
    pthread_cond_destroy(&mgr->cond);
    free(mgr);
}

int checkpoint_manager_start(CheckpointManager *mgr) {
    if (!mgr || atomic_load(&mgr->started)) return -1;

    atomic_store(&mgr->running, true);
    if (pthread_create(&mgr->thread, NULL, checkpoint_thread_main, mgr) != 0) {
        LOG_ERROR("CheckpointManager: pthread_create 失败");
        atomic_store(&mgr->running, false);
        return -1;
    }
    atomic_store(&mgr->started, true);
    LOG_INFO("CheckpointManager: 后台线程已启动 (interval=%u ms)", mgr->interval_ms);
    return 0;
}

int checkpoint_manager_stop(CheckpointManager *mgr) {
    if (!mgr || !atomic_load(&mgr->started)) return 0;

    atomic_store(&mgr->running, false);

    /* 唤醒可能阻塞在 cond_timedwait 上的线程 */
    pthread_mutex_lock(&mgr->mutex);
    pthread_cond_broadcast(&mgr->cond);
    pthread_mutex_unlock(&mgr->mutex);

    pthread_join(mgr->thread, NULL);
    atomic_store(&mgr->started, false);
    LOG_INFO("CheckpointManager: 后台线程已停止");
    return 0;
}

uint64_t checkpoint_manager_get_lsn(CheckpointManager *mgr) {
    if (!mgr || !mgr->wal) return 0;
    /* vector_wal 内部已 mutex 化 last_checkpoint 访问（write/read 都在 wal->mutex
     * 下），这里直接读即可。无需额外加锁，避免与后台线程的 checkpoint 写竞争。 */
    return mgr->wal->last_checkpoint;
}
