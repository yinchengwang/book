/**
 * @file worker_pool.c
 * @brief Worker 线程池实现
 */

#include "db/sql/worker_pool.h"
#include "db/sql/tuple_queue.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>

/* Windows 平台线程封装 */
typedef HANDLE pthread_t;
typedef unsigned (__stdcall *ThreadStartFn)(void *);

static int pthread_create(pthread_t *thread, void *attr,
                          ThreadStartFn start_routine, void *arg) {
    (void)attr;
    *thread = (HANDLE)_beginthreadex(NULL, 0, start_routine, arg, 0, NULL);
    return *thread == NULL ? -1 : 0;
}

static int pthread_join(pthread_t thread, void **retval) {
    (void)retval;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

#else
#include <pthread.h>
#endif

/* ========================================================================
 * WorkerPool 内部结构
 * ======================================================================== */

/**
 * @brief Worker 线程参数
 */
typedef struct WorkerArg {
    int              worker_id;   /**< Worker ID */
    WorkerTaskFn     func;        /**< 任务函数 */
    void            *arg;         /**< 用户参数 */
    TupleQueue      *output_q;    /**< 输出队列 */
    WorkerPool      *pool;        /**< 所属线程池 */
} WorkerArg;

/**
 * @brief Worker 线程池结构
 */
struct WorkerPool {
    int              nworkers;    /**< Worker 数量 */
    pthread_t       *threads;     /**< 线程句柄数组 */
    WorkerArg       *worker_args; /**< Worker 参数数组 */
    volatile bool    shutdown;    /**< 是否已关闭 */
    volatile bool    started;     /**< 是否已启动 */
    volatile int     nfinished;   /**< 已完成的 worker 数量 */
};

/* ========================================================================
 * Worker 线程入口函数
 * ======================================================================== */

#ifdef _WIN32
static unsigned __stdcall worker_thread_main(void *arg) {
#else
static void *worker_thread_main(void *arg) {
#endif
    WorkerArg *warg = (WorkerArg *)arg;

    /* 执行任务函数 */
    if (warg->func) {
        warg->func(warg->worker_id, warg->arg, warg->output_q);
    }

    /* 原子递增已完成计数，最后一个 worker 负责关闭输出队列 */
    if (warg->output_q) {
        int finished;
#ifdef _WIN32
        InterlockedIncrement((volatile LONG *)&warg->pool->nfinished);
        finished = warg->pool->nfinished;
#else
        __sync_fetch_and_add(&warg->pool->nfinished, 1);
        finished = warg->pool->nfinished;
#endif
        if (finished >= warg->pool->nworkers) {
            /* 最后一个 worker 完成，关闭输出队列 */
            TupleQueueClose(warg->output_q);
        }
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ========================================================================
 * WorkerPool API 实现
 * ======================================================================== */

WorkerPool *WorkerPoolCreate(int nworkers) {
    WorkerPool *pool;

    if (nworkers <= 0) {
        return NULL;
    }

    pool = (WorkerPool *)calloc(1, sizeof(WorkerPool));
    if (!pool) {
        return NULL;
    }

    pool->nworkers = nworkers;
    pool->shutdown = false;
    pool->started = false;
    pool->nfinished = 0;

    /* 分配线程句柄数组 */
    pool->threads = (pthread_t *)calloc(nworkers, sizeof(pthread_t));
    if (!pool->threads) {
        free(pool);
        return NULL;
    }

    /* 分配 worker 参数数组 */
    pool->worker_args = (WorkerArg *)calloc(nworkers, sizeof(WorkerArg));
    if (!pool->worker_args) {
        free(pool->threads);
        free(pool);
        return NULL;
    }

    return pool;
}

void WorkerPoolDestroy(WorkerPool *pool) {
    if (!pool) {
        return;
    }

    /* 等待所有 worker 完成 */
    if (pool->started && !pool->shutdown) {
        WorkerPoolWait(pool);
    }

    /* 释放资源 */
    free(pool->threads);
    free(pool->worker_args);
    free(pool);
}

int WorkerPoolStart(WorkerPool *pool, WorkerTask *tasks) {
    int i;
    int nstarted = 0;

    if (!pool || !tasks) {
        return -1;
    }

    if (pool->started) {
        return -1;  /* 已经启动过 */
    }

    pool->started = true;
    pool->nfinished = 0;

    /* 初始化每个 worker 的参数并启动线程 */
    for (i = 0; i < pool->nworkers; i++) {
        pool->worker_args[i].worker_id = i;
        pool->worker_args[i].func = tasks[i].func;
        pool->worker_args[i].arg = tasks[i].arg;
        pool->worker_args[i].output_q = tasks[i].output_q;
        pool->worker_args[i].pool = pool;

        if (pthread_create(&pool->threads[i], NULL,
                           worker_thread_main,
                           &pool->worker_args[i]) == 0) {
            nstarted++;
        }
    }

    return nstarted;
}

void WorkerPoolWait(WorkerPool *pool) {
    int i;

    if (!pool || !pool->started) {
        return;
    }

    /* 等待所有线程完成 */
    for (i = 0; i < pool->nworkers; i++) {
        if (pool->threads[i]) {
            pthread_join(pool->threads[i], NULL);
            pool->threads[i] = 0;
        }
    }

    pool->shutdown = true;
}

int WorkerPoolGetNWorkers(WorkerPool *pool) {
    return pool ? pool->nworkers : 0;
}

bool WorkerPoolIsShutdown(WorkerPool *pool) {
    return pool ? pool->shutdown : true;
}