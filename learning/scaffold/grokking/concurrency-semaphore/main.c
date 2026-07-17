/**
 * @file concurrency-semaphore/main.c
 * @brief POSIX 信号量：互斥、资源池与生产者-消费者
 *
 * 演示内容:
 * 1. 二元信号量做互斥锁（保护共享计数器）
 * 2. 计数信号量控制资源池并发数
 * 3. 信号量实现生产者-消费者（替代条件变量方案）
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * 1. 二元信号量做互斥锁
 * ============================================================================ */

#define SEM_INCR_LOOPS 500000

static int sem_counter = 0;
static sem_t sem_mutex;  /* 作为互斥锁使用的二元信号量 */

static void *sem_safe_incr(void *arg) {
    (void)arg;
    for (int i = 0; i < SEM_INCR_LOOPS; i++) {
        sem_wait(&sem_mutex);
        sem_counter++;
        sem_post(&sem_mutex);
    }
    return NULL;
}

/* ============================================================================
 * 2. 计数信号量控制资源池
 * ============================================================================ */

#define POOL_SIZE 3       /* 池中最多 3 个资源 */
#define NUM_WORKERS 6     /* 6 个线程竞争资源 */

static sem_t resource_pool;

/** 使用资源的线程 */
static void *worker_thread(void *arg) {
    int id = *(int *)arg;
    printf("  工作线程%d: 等待资源...\n", id);
    sem_wait(&resource_pool);  /* P 操作：占用一个资源 */

    printf("  工作线程%d: 拿到资源，工作中...\n", id);
    usleep(200000);  /* 模拟工作 */

    printf("  工作线程%d: 释放资源\n", id);
    sem_post(&resource_pool);  /* V 操作：释放一个资源 */
    return NULL;
}

/* ============================================================================
 * 3. 信号量实现生产者-消费者
 * ============================================================================ */

#define SEM_BUF_SIZE 5
static int sem_buf[SEM_BUF_SIZE];
static int sem_buf_head = 0, sem_buf_tail = 0;

static sem_t empty_slots;  /* 空槽位计数 */
static sem_t filled_slots; /* 已填槽位计数 */
static sem_t buf_mutex;    /* 保护缓冲区的互斥 */

static void *sem_producer(void *arg) {
    (void)arg;
    for (int i = 0; i < 8; i++) {
        sem_wait(&empty_slots);       /* 等空槽位 */
        sem_wait(&buf_mutex);         /* 加锁 */

        sem_buf[sem_buf_tail] = i * 10;
        printf("  生产者: 放入 %d\n", i * 10);
        sem_buf_tail = (sem_buf_tail + 1) % SEM_BUF_SIZE;

        sem_post(&buf_mutex);         /* 解锁 */
        sem_post(&filled_slots);      /* 增加已填槽位 */
        usleep(30000);
    }
    return NULL;
}

static void *sem_consumer(void *arg) {
    (void)arg;
    for (int i = 0; i < 8; i++) {
        sem_wait(&filled_slots);      /* 等已填槽位 */
        sem_wait(&buf_mutex);         /* 加锁 */

        int val = sem_buf[sem_buf_head];
        printf("  消费者: 取出 %d\n", val);
        sem_buf_head = (sem_buf_head + 1) % SEM_BUF_SIZE;

        sem_post(&buf_mutex);         /* 解锁 */
        sem_post(&empty_slots);       /* 增加空槽位 */
        usleep(80000);
    }
    return NULL;
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void)
{
    printf("=== POSIX 信号量 —— 互斥、资源池与生产者-消费者 ===\n\n");

    /* ---------------------------------------------------------------
     * 1. 二元信号量做互斥锁
     * --------------------------------------------------------------- */
    printf("--- 1. 二元信号量作为互斥锁 ---\n");
    printf("  两个线程各递增 %d 次\n", SEM_INCR_LOOPS);

    sem_init(&sem_mutex, 0, 1);  /* 初始值 1 = 二元信号量 */

    pthread_t sm1, sm2;
    pthread_create(&sm1, NULL, sem_safe_incr, NULL);
    pthread_create(&sm2, NULL, sem_safe_incr, NULL);
    pthread_join(sm1, NULL);
    pthread_join(sm2, NULL);

    printf("  结果 = %d %s\n\n", sem_counter,
           sem_counter == 2 * SEM_INCR_LOOPS ? "✓ 正确" : "✗ 错误");
    sem_destroy(&sem_mutex);

    /* ---------------------------------------------------------------
     * 2. 计数信号量控制资源池
     * --------------------------------------------------------------- */
    printf("--- 2. 计数信号量控制资源池 ---\n");
    printf("  资源池大小 = %d, 工作线程数 = %d\n", POOL_SIZE, NUM_WORKERS);
    printf("  （同时最多 %d 个线程工作）\n\n", POOL_SIZE);

    sem_init(&resource_pool, 0, POOL_SIZE);

    pthread_t workers[NUM_WORKERS];
    int worker_ids[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        worker_ids[i] = i + 1;
        pthread_create(&workers[i], NULL, worker_thread, &worker_ids[i]);
    }
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }
    sem_destroy(&resource_pool);

    /* ---------------------------------------------------------------
     * 3. 信号量实现生产者-消费者
     * --------------------------------------------------------------- */
    printf("\n--- 3. 信号量实现生产者-消费者 ---\n");
    printf("  缓冲区大小 = %d\n\n", SEM_BUF_SIZE);

    sem_init(&empty_slots, 0, SEM_BUF_SIZE);
    sem_init(&filled_slots, 0, 0);
    sem_init(&buf_mutex, 0, 1);

    pthread_t sem_prod, sem_cons;
    pthread_create(&sem_prod, NULL, sem_producer, NULL);
    pthread_create(&sem_cons, NULL, sem_consumer, NULL);
    pthread_join(sem_prod, NULL);
    pthread_join(sem_cons, NULL);

    sem_destroy(&empty_slots);
    sem_destroy(&filled_slots);
    sem_destroy(&buf_mutex);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
