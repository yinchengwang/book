/**
 * @file concurrency-barrier/main.c
 * @brief pthread_barrier_t 分阶段同步演示
 *
 * 演示内容:
 * 1. N 个线程分两阶段计算，barrier 同步
 * 2. 多个 barrier 实现流水线阶段
 * 3. barrier 销毁
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 4
#define PHASE1_WORK 3
#define PHASE2_WORK 5

/* 全局 barrier */
static pthread_barrier_t g_barrier;

/* 共享累计值（各线程写入） */
static int g_phase1_sum = 0;
static int g_phase2_sum = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * 模拟线程工作：睡眠指定秒数
 */
static void do_work(int id, int phase, int seconds) {
    printf("  线程 %d 开始阶段 %d (工作 %ds)...\n", id, phase, seconds);
    sleep(seconds);
    printf("  线程 %d 完成阶段 %d\n", id, phase);
}

/**
 * 线程函数：两阶段计算，barrier 同步
 */
static void *worker(void *arg) {
    int id = *(int *)arg;
    int local1, local2;

    /* ---------- 阶段 1: 独立计算 ---------- */
    do_work(id, 1, (id % PHASE1_WORK) + 1);
    local1 = id * 10 + 1;

    pthread_mutex_lock(&g_mutex);
    g_phase1_sum += local1;
    pthread_mutex_unlock(&g_mutex);

    /* 所有线程在此等待，全部到达后才进入阶段 2 */
    int rc = pthread_barrier_wait(&g_barrier);
    if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
        printf("  [主线程 %d 触发阶段切换]\n", id);
    }

    /* ---------- 阶段 2: 基于阶段 1 结果继续计算 ---------- */
    do_work(id, 2, (id % PHASE2_WORK) + 1);

    pthread_mutex_lock(&g_mutex);
    local2 = g_phase1_sum + id;
    g_phase2_sum += local2;
    pthread_mutex_unlock(&g_mutex);

    /* 第二个 barrier，等待所有线程完成阶段 2 */
    pthread_barrier_wait(&g_barrier);

    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];

    printf("=== Barrier 分阶段同步演示 ===\n");
    printf("线程数: %d\n\n", NUM_THREADS);

    /* 初始化 barrier：需要 NUM_THREADS 个线程都到达才放行 */
    pthread_barrier_init(&g_barrier, NULL, NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i + 1;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&g_barrier);

    printf("\n--- 结果 ---\n");
    printf("阶段 1 累计: %d\n", g_phase1_sum);
    printf("阶段 2 累计: %d\n", g_phase2_sum);
    printf("=== 演示完成 ===\n");
    return 0;
}
