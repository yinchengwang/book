/**
 * @file concurrency-thread-pool/main.c
 * @brief 简单线程池实现演示
 *
 * 演示内容:
 * 1. 固定 N 个工作线程的任务队列模型
 * 2. 函数指针 + void* arg 提交任务
 * 3. mutex + cond_var 生产者-消费者通信
 * 4. 动态扩容演示
 * 5. 队列满拒绝策略
 * 6. 优雅关闭
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#define INITIAL_THREADS 2
#define MAX_QUEUE       8
#define TOTAL_TASKS     20

/* ============================================================================
 * 任务定义
 * ============================================================================ */

typedef struct Task {
    void (*func)(void *arg);
    void *arg;
} Task;

/* ============================================================================
 * 线程池
 * ============================================================================ */

typedef struct {
    pthread_t   *threads;
    int          num_threads;

    Task        *queue;
    int          capacity;
    int          front;
    int          rear;
    int          count;

    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;

    bool         shutdown;
} ThreadPool;

/* forward declaration — 在 tp_init 之前 */
static void *tp_worker(void *arg);

/**
 * 初始化线程池
 */
static void tp_init(ThreadPool *tp, int num_threads) {
    tp->num_threads = num_threads;
    tp->threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    tp->capacity = MAX_QUEUE;
    tp->queue = (Task *)malloc(tp->capacity * sizeof(Task));
    tp->front = 0;
    tp->rear  = 0;
    tp->count = 0;
    tp->shutdown = false;

    pthread_mutex_init(&tp->mutex, NULL);
    pthread_cond_init(&tp->not_empty, NULL);
    pthread_cond_init(&tp->not_full, NULL);

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&tp->threads[i], NULL, tp_worker, tp);
    }
}

/**
 * 工作线程：循环从队列取任务执行
 */
static void *tp_worker(void *arg) {
    ThreadPool *tp = (ThreadPool *)arg;

    while (1) {
        pthread_mutex_lock(&tp->mutex);

        /* 等待任务或关闭 */
        while (tp->count == 0 && !tp->shutdown) {
            pthread_cond_wait(&tp->not_empty, &tp->mutex);
        }

        if (tp->shutdown && tp->count == 0) {
            pthread_mutex_unlock(&tp->mutex);
            break;
        }

        /* 取任务 */
        Task task = tp->queue[tp->front];
        tp->front = (tp->front + 1) % tp->capacity;
        tp->count--;

        pthread_cond_signal(&tp->not_full);
        pthread_mutex_unlock(&tp->mutex);

        /* 执行任务（不持锁） */
        task.func(task.arg);
    }

    return NULL;
}

/**
 * 提交任务。队列满时返回 -1 拒绝
 */
static int tp_submit(ThreadPool *tp, void (*func)(void *), void *arg) {
    pthread_mutex_lock(&tp->mutex);

    if (tp->count >= tp->capacity || tp->shutdown) {
        pthread_mutex_unlock(&tp->mutex);
        return -1;  /* 拒绝 */
    }

    tp->queue[tp->rear].func = (void (*)(void *))func;
    tp->queue[tp->rear].arg  = arg;
    tp->rear = (tp->rear + 1) % tp->capacity;
    tp->count++;

    pthread_cond_signal(&tp->not_empty);
    pthread_mutex_unlock(&tp->mutex);
    return 0;
}

/**
 * 动态扩容：增加一个新工作线程
 */
static void tp_expand(ThreadPool *tp) {
    pthread_mutex_lock(&tp->mutex);
    int new_count = tp->num_threads + 1;
    tp->threads = (pthread_t *)realloc(tp->threads,
                    new_count * sizeof(pthread_t));
    tp->num_threads = new_count;
    pthread_mutex_unlock(&tp->mutex);

    pthread_create(&tp->threads[new_count - 1], NULL, tp_worker, tp);
    printf("  [扩容] 工作线程数: %d\n", new_count);
}

/**
 * 优雅关闭：等待所有已提交任务完成
 */
static void tp_shutdown(ThreadPool *tp) {
    pthread_mutex_lock(&tp->mutex);
    tp->shutdown = true;
    pthread_cond_broadcast(&tp->not_empty);
    pthread_mutex_unlock(&tp->mutex);

    for (int i = 0; i < tp->num_threads; i++) {
        pthread_join(tp->threads[i], NULL);
    }
}

static void tp_destroy(ThreadPool *tp) {
    free(tp->threads);
    free(tp->queue);
    pthread_mutex_destroy(&tp->mutex);
    pthread_cond_destroy(&tp->not_empty);
    pthread_cond_destroy(&tp->not_full);
}

/* ============================================================================
 * 任务函数
 * ============================================================================ */

static pthread_mutex_t g_print_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int id;
    int work_ms;
} TaskArg;

static void sample_task(void *arg) {
    TaskArg *ta = (TaskArg *)arg;
    /* 模拟工作 */
    for (volatile int i = 0; i < 1000000 * ta->work_ms; i++) { }

    pthread_mutex_lock(&g_print_mutex);
    printf("  任务 %d 完成 (work=%d)\n", ta->id, ta->work_ms);
    pthread_mutex_unlock(&g_print_mutex);

    free(arg);
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    ThreadPool pool;
    int accepted = 0, rejected = 0;

    printf("=== 线程池演示 ===\n\n");

    tp_init(&pool, INITIAL_THREADS);
    printf("初始线程数: %d, 队列容量: %d\n\n", INITIAL_THREADS, MAX_QUEUE);

    /* 提交一批任务，观察正常处理 */
    printf("--- 提交 %d 个任务 ---\n", TOTAL_TASKS);
    for (int i = 0; i < TOTAL_TASKS; i++) {
        TaskArg *ta = (TaskArg *)malloc(sizeof(TaskArg));
        ta->id   = i + 1;
        ta->work_ms = (i % 3) + 1;

        int rc = tp_submit(&pool, sample_task, ta);
        if (rc == 0) {
            accepted++;
        } else {
            rejected++;
            printf("  任务 %d 被拒绝 (队列满)\n", ta->id);
            free(ta);
        }
    }

    printf("\n  接受: %d, 拒绝: %d\n\n", accepted, rejected);

    /* 演示动态扩容 */
    printf("--- 动态扩容 ---\n");
    tp_expand(&pool);

    /* 再提交一批任务 */
    printf("\n--- 扩容后追加任务 ---\n");
    for (int i = 0; i < 6; i++) {
        TaskArg *ta = (TaskArg *)malloc(sizeof(TaskArg));
        ta->id = TOTAL_TASKS + i + 1;
        ta->work_ms = 1;
        tp_submit(&pool, sample_task, ta);
    }

    /* 优雅关闭 */
    printf("\n--- 优雅关闭 ---\n");
    tp_shutdown(&pool);
    tp_destroy(&pool);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
