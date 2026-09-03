/**
 * @file bgworker.c
 * @brief 后台任务调度器核心实现
 */

#if !defined(_WIN32) && defined(__linux__)
#define _GNU_SOURCE
#endif

#include "bgworker_internal.h"
#include "db/guc.h"
#include "db/log.h"

#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ============================================================
 * 静态变量
 * ============================================================ */

/** 全局调度器实例（声明在 bgworker_internal.h） */
db_bg_worker_t g_worker = {0};

/** GUC 参数缺失时使用的默认值 */
static bool g_bgworker_enabled = true;
static int g_bgworker_tick_ms = DB_BGWORKER_DEFAULT_TICK_MS;

#ifdef _WIN32
/** Windows 原生线程句柄 */
static HANDLE g_worker_thread = NULL;
#endif

/* ============================================================
 * 辅助函数实现
 * ============================================================ */

uint64_t _db_bgworker_now_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000U + (uint64_t)ts.tv_nsec / 1000000U;
#endif
}

void _db_bgworker_init_slots(db_bg_worker_t *worker) {
    if (worker == NULL) {
        return;
    }

    memset(worker->slots, 0, sizeof(worker->slots));
    worker->num_slots = DB_BGWORKER_MAX_TASKS;
}

void _db_bgworker_cleanup_slots(db_bg_worker_t *worker) {
    uint32_t i;

    if (worker == NULL) {
        return;
    }

    for (i = 0; i < worker->num_slots; i++) {
        db_bg_task_slot_t *slot = &worker->slots[i];
        db_bg_task_t *task = slot->task;

        if (!slot->in_use || task == NULL) {
            continue;
        }
        if (task->cleanup != NULL && task->ctx != NULL) {
            task->cleanup(task->ctx);
        }
        _db_bgworker_free_context(task->ctx);
        task->ctx = NULL;
        memset(slot, 0, sizeof(*slot));
    }
}

int _db_bgworker_find_slot(db_bg_worker_t *worker, const char *name) {
    uint32_t i;

    if (worker == NULL || name == NULL) {
        return -1;
    }

    for (i = 0; i < worker->num_slots; i++) {
        db_bg_task_slot_t *slot = &worker->slots[i];

        if (slot->in_use && slot->task != NULL && slot->task->name != NULL && strcmp(slot->task->name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int _db_bgworker_find_free_slot(db_bg_worker_t *worker) {
    uint32_t i;

    if (worker == NULL) {
        return -1;
    }

    for (i = 0; i < worker->num_slots; i++) {
        if (!worker->slots[i].in_use) {
            return (int)i;
        }
    }
    return -1;
}

db_bg_task_context_t *_db_bgworker_alloc_context(db_bg_task_t *task) {
    db_bg_task_context_t *ctx;

    if (task == NULL) {
        return NULL;
    }

    ctx = (db_bg_task_context_t *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    atomic_init(&ctx->status, DB_BG_TASK_STATUS_IDLE);
    return ctx;
}

void _db_bgworker_free_context(db_bg_task_context_t *ctx) {
    free(ctx);
}

static void db_bgworker_sleep(uint32_t sleep_ms) {
#ifdef _WIN32
    Sleep((DWORD)sleep_ms);
#else
    struct timespec request;
    struct timespec remaining;

    request.tv_sec = (time_t)(sleep_ms / 1000U);
    request.tv_nsec = (long)(sleep_ms % 1000U) * 1000000L;
    while (nanosleep(&request, &remaining) != 0 && errno == EINTR) {
        request = remaining;
    }
#endif
}

/* ============================================================
 * 主循环
 * ============================================================ */

void *_db_bgworker_main_loop(void *arg) {
    db_bg_worker_t *worker = (db_bg_worker_t *)arg;
    uint64_t cycle_count = 0;

    if (worker == NULL) {
        return NULL;
    }

    LOG_INFO("后台任务调度器启动，tick_ms=%u", worker->tick_ms);

    while (!atomic_load_explicit(&worker->stopping, memory_order_acquire)) {
        uint64_t now = _db_bgworker_now_ms();
        uint32_t i;

        cycle_count++;
        pthread_mutex_lock(&worker->mutex);

        for (i = 0; i < worker->num_slots; i++) {
            db_bg_task_slot_t *slot = &worker->slots[i];
            db_bg_task_t *task = slot->task;
            db_bg_task_status_t status;
            int check_result;

            if (!slot->in_use || task == NULL || task->ctx == NULL || task->check == NULL) {
                continue;
            }

            status = db_bg_task_get_status(task->ctx);
            if (status != DB_BG_TASK_STATUS_IDLE || now < slot->next_check_time) {
                continue;
            }

            db_bg_task_set_status(task->ctx, DB_BG_TASK_STATUS_CHECKING);
            task->ctx->last_check_time = now;
            check_result = task->check(task->ctx);

            if (check_result > 0) {
                int *configured_window = guc_get_int("bgworker.cceh_shrink.time_window");
                int window = configured_window != NULL && *configured_window > 0 ? *configured_window : 3;

                task->ctx->consecutive_trigger++;
                if (task->ctx->consecutive_trigger >= (uint32_t)window) {
                    int execute_result;

                    db_bg_task_set_status(task->ctx, DB_BG_TASK_STATUS_EXECUTING);
                    execute_result = task->execute != NULL ? task->execute(task->ctx) : -1;
                    task->ctx->consecutive_trigger = 0;
                    if (execute_result != 0) {
                        LOG_WARN("后台任务 '%s' 执行失败", task->name);
                    }
                }
            } else {
                if (check_result < 0) {
                    LOG_WARN("后台任务 '%s' 检查失败", task->name);
                }
                task->ctx->consecutive_trigger = 0;
            }

            db_bg_task_set_status(task->ctx, DB_BG_TASK_STATUS_IDLE);
            slot->next_check_time = now + task->interval_ms;
        }

        pthread_mutex_unlock(&worker->mutex);
        db_bgworker_sleep(worker->tick_ms);
    }

    LOG_INFO("后台任务调度器停止，共执行 %" PRIu64 " 个周期", cycle_count);
    return NULL;
}

#ifdef _WIN32
static DWORD WINAPI db_bgworker_windows_main(LPVOID arg) {
    (void)_db_bgworker_main_loop(arg);
    return 0;
}
#endif

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

int db_bgworker_start(void) {
    bool *configured_enabled;
    int *configured_tick_ms;
    int config_init_result;

    if (atomic_load_explicit(&g_worker.running, memory_order_acquire)) {
        LOG_WARN("%s", "后台任务调度器已在运行");
        return 0;
    }

    /* 注册本模块的 GUC 配置参数（幂等保护由 guc_register_param 重名检测） */
    config_init_result = db_bgworker_config_init();
    if (config_init_result != 0) {
        LOG_WARN("%s", "后台任务调度器 GUC 参数注册失败，继续以本地默认值运行");
    }

    configured_enabled = guc_get_bool("bgworker.enabled");
    if (!(configured_enabled != NULL ? *configured_enabled : g_bgworker_enabled)) {
        LOG_INFO("%s", "后台任务调度器被配置禁用");
        return 0;
    }

    memset(&g_worker, 0, sizeof(g_worker));
    _db_bgworker_init_slots(&g_worker);

    configured_tick_ms = guc_get_int("bgworker.tick_ms");
    g_worker.tick_ms = configured_tick_ms != NULL && *configured_tick_ms > 0 ? (uint32_t)*configured_tick_ms
                                                                                 : (uint32_t)g_bgworker_tick_ms;

    if (pthread_mutex_init(&g_worker.mutex, NULL) != 0) {
        LOG_ERROR("%s", "后台任务调度器互斥锁初始化失败");
        return -1;
    }

    atomic_init(&g_worker.running, true);
    atomic_init(&g_worker.stopping, false);
    g_worker.start_time_ms = _db_bgworker_now_ms();

#ifdef _WIN32
    g_worker_thread = CreateThread(NULL, 0, db_bgworker_windows_main, &g_worker, 0, NULL);
    if (g_worker_thread == NULL) {
        LOG_ERROR("后台任务调度器线程创建失败，错误码=%lu", (unsigned long)GetLastError());
        pthread_mutex_destroy(&g_worker.mutex);
        atomic_store_explicit(&g_worker.running, false, memory_order_release);
        return -1;
    }
#else
    if (pthread_create(&g_worker.thread, NULL, _db_bgworker_main_loop, &g_worker) != 0) {
        LOG_ERROR("%s", "后台任务调度器线程创建失败");
        pthread_mutex_destroy(&g_worker.mutex);
        atomic_store_explicit(&g_worker.running, false, memory_order_release);
        return -1;
    }
#endif

    LOG_INFO("%s", "后台任务调度器已启动");
    return 0;
}

int db_bgworker_stop(void) {
    int wait_result = 0;

    if (!atomic_load_explicit(&g_worker.running, memory_order_acquire)) {
        return 0;
    }

    LOG_INFO("%s", "正在停止后台任务调度器...");
    atomic_store_explicit(&g_worker.stopping, true, memory_order_release);

#ifdef _WIN32
    {
        DWORD wait_status = WaitForSingleObject(g_worker_thread, DB_BGWORKER_STOP_TIMEOUT_SEC * 1000U);

        if (wait_status == WAIT_TIMEOUT) {
            LOG_WARN("%s", "等待后台任务调度器线程停止超时，正在强制终止");
            if (!TerminateThread(g_worker_thread, 1U)) {
                LOG_ERROR("强制终止后台任务调度器线程失败，错误码=%lu", (unsigned long)GetLastError());
                return -1;
            }
            wait_status = WaitForSingleObject(g_worker_thread, INFINITE);
        }
        if (wait_status != WAIT_OBJECT_0) {
            LOG_ERROR("等待后台任务调度器线程停止失败，错误码=%lu", (unsigned long)GetLastError());
            return -1;
        }
        CloseHandle(g_worker_thread);
        g_worker_thread = NULL;
    }
#elif defined(__linux__)
    {
        struct timespec deadline;

        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
            LOG_ERROR("获取后台任务调度器停止期限失败，错误码=%d", errno);
            return -1;
        }
        deadline.tv_sec += DB_BGWORKER_STOP_TIMEOUT_SEC;
        wait_result = pthread_timedjoin_np(g_worker.thread, NULL, &deadline);
        if (wait_result == ETIMEDOUT) {
            LOG_WARN("%s", "等待后台任务调度器线程停止超时，正在取消线程");
            wait_result = pthread_cancel(g_worker.thread);
            if (wait_result == 0) {
                wait_result = pthread_join(g_worker.thread, NULL);
            }
        }
    }
#else
    wait_result = pthread_join(g_worker.thread, NULL);
#endif

    if (wait_result != 0) {
        LOG_ERROR("等待后台任务调度器线程停止失败，错误码=%d", wait_result);
        return -1;
    }

    _db_bgworker_cleanup_slots(&g_worker);
    pthread_mutex_destroy(&g_worker.mutex);

    atomic_store_explicit(&g_worker.running, false, memory_order_release);
    LOG_INFO("%s", "后台任务调度器已停止");
    return 0;
}
