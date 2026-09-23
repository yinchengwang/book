/* px_scheduler.c - 进程级线程池（Gap#4） */
#include "db/executor/px_scheduler.h"
#include <pthread.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define PX_TASK_QUEUE_CAP 256

typedef struct {
    px_task_fn fn;
    void *arg;
    volatile int *cancel_flag;
} px_task_t;

struct px_scheduler {
    pthread_t *threads;
    int nworkers;
    px_task_t tasks[PX_TASK_QUEUE_CAP];
    int head, tail, count;
    int outstanding;             /* 已提交未完成（含排队中） */
    pthread_mutex_t mu;
    pthread_cond_t has_task;
    pthread_cond_t idle;
};

static px_scheduler_t g_sched;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static int px_hw_concurrency(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}

static void *px_worker_main(void *arg) {
    px_scheduler_t *s = (px_scheduler_t *)arg;
    for (;;) {
        pthread_mutex_lock(&s->mu);
        while (s->count == 0) {
            pthread_cond_wait(&s->has_task, &s->mu);
        }
        px_task_t t = s->tasks[s->head];
        s->head = (s->head + 1) % PX_TASK_QUEUE_CAP;
        s->count--;
        pthread_mutex_unlock(&s->mu);

        if (!t.cancel_flag || !__atomic_load_n(t.cancel_flag, __ATOMIC_ACQUIRE)) {
            t.fn(t.arg, t.cancel_flag);
        }

        pthread_mutex_lock(&s->mu);
        s->outstanding--;
        if (s->outstanding == 0) pthread_cond_broadcast(&s->idle);
        pthread_mutex_unlock(&s->mu);
    }
    return NULL;
}

static void px_sched_init_once(void) {
    int n = px_hw_concurrency();
    if (n > PX_SCHED_MAX_WORKERS) n = PX_SCHED_MAX_WORKERS;
    if (n < 1) n = 1;
    g_sched.nworkers = n;
    g_sched.threads = (pthread_t *)calloc((size_t)n, sizeof(pthread_t));
    pthread_mutex_init(&g_sched.mu, NULL);
    pthread_cond_init(&g_sched.has_task, NULL);
    pthread_cond_init(&g_sched.idle, NULL);
    for (int i = 0; i < n; i++) {
        pthread_create(&g_sched.threads[i], NULL, px_worker_main, &g_sched);
        pthread_detach(g_sched.threads[i]);   /* 进程级常驻 */
    }
}

px_scheduler_t *px_scheduler_default(void) {
    pthread_once(&g_once, px_sched_init_once);
    return &g_sched;
}

int px_scheduler_submit(px_scheduler_t *s, px_task_fn fn,
                        void *arg, volatile int *cancel_flag) {
    if (!s || !fn) return -1;
    pthread_mutex_lock(&s->mu);
    while (s->count == PX_TASK_QUEUE_CAP) {
        pthread_cond_wait(&s->idle, &s->mu);  /* 队列满：等一波排空 */
    }
    s->tasks[s->tail].fn = fn;
    s->tasks[s->tail].arg = arg;
    s->tasks[s->tail].cancel_flag = cancel_flag;
    s->tail = (s->tail + 1) % PX_TASK_QUEUE_CAP;
    s->count++;
    s->outstanding++;
    pthread_cond_signal(&s->has_task);
    pthread_mutex_unlock(&s->mu);
    return 0;
}

void px_scheduler_wait_idle(px_scheduler_t *s) {
    if (!s) return;
    pthread_mutex_lock(&s->mu);
    while (s->outstanding > 0) {
        pthread_cond_wait(&s->idle, &s->mu);
    }
    pthread_mutex_unlock(&s->mu);
}

int px_scheduler_num_workers(const px_scheduler_t *s) {
    return s ? s->nworkers : 0;
}
