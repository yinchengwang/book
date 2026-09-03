/*
 * pthread scaffold — POSIX 线程 + mutex + cond 三件套最小演示
 *
 * 用法: ./pthread_demo  (无须参数)
 *
 * 演示场景：
 *   - 2 个 worker 线程并发累加同一个全局计数器
 *   - mutex 保证计数原子性
 *   - 主线程等待子线程完成后打印结果
 *
 * 编译运行：
 *   $ make
 *   $ ./pthread_demo
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define WORKER_COUNT 2
#define INCREMENTS_PER_WORKER 100000

static long long g_counter = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int id;
    int increments;
} worker_arg_t;

static void *worker(void *arg) {
    worker_arg_t *wa = (worker_arg_t *)arg;
    for (int i = 0; i < wa->increments; i++) {
        pthread_mutex_lock(&g_mutex);
        g_counter++;
        pthread_mutex_unlock(&g_mutex);
    }
    printf("worker %d done\n", wa->id);
    return NULL;
}

int main(void) {
    pthread_t threads[WORKER_COUNT];
    worker_arg_t args[WORKER_COUNT];
    int rc;

    /* 创建 worker */
    for (int i = 0; i < WORKER_COUNT; i++) {
        args[i].id = i;
        args[i].increments = INCREMENTS_PER_WORKER;
        rc = pthread_create(&threads[i], NULL, worker, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
            return 1;
        }
    }

    /* 主线程 join（隐式等待） */
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    long long expected = (long long)WORKER_COUNT * INCREMENTS_PER_WORKER;
    printf("counter = %lld (expected %lld)\n", g_counter, expected);
    if (g_counter != expected) {
        fprintf(stderr, "FAIL: counter mismatch\n");
        return 1;
    }
    printf("OK\n");
    return 0;
}
