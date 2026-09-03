/**
 * @file concurrency-condition-var/main.c
 * @brief 条件变量：生产者-消费者模式与有界缓冲区
 *
 * 演示内容:
 * 1. pthread_cond_wait / pthread_cond_signal 生产者-消费者
 * 2. pthread_cond_broadcast 通知所有等待线程
 * 3. 有界缓冲区（环形缓冲区）实现
 * 4. 正确的 mutex + cond_var 使用模式
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * 有界缓冲区（环形缓冲区）
 * ============================================================================ */

#define BUFFER_SIZE 8

/** 有界缓冲区结构 */
typedef struct {
    int buffer[BUFFER_SIZE];   /**< 数据存储 */
    int head;                  /**< 读取位置 */
    int tail;                  /**< 写入位置 */
    int count;                 /**< 当前元素数 */
    pthread_mutex_t mutex;     /**< 互斥锁 */
    pthread_cond_t not_full;   /**< 条件：缓冲区不满 */
    pthread_cond_t not_empty;  /**< 条件：缓冲区不空 */
} BoundedBuffer;

/** 初始化有界缓冲区 */
static void buffer_init(BoundedBuffer *bb) {
    bb->head = 0;
    bb->tail = 0;
    bb->count = 0;
    pthread_mutex_init(&bb->mutex, NULL);
    pthread_cond_init(&bb->not_full, NULL);
    pthread_cond_init(&bb->not_empty, NULL);
}

/** 销毁有界缓冲区 */
static void buffer_destroy(BoundedBuffer *bb) {
    pthread_mutex_destroy(&bb->mutex);
    pthread_cond_destroy(&bb->not_full);
    pthread_cond_destroy(&bb->not_empty);
}

/** 生产者：放入数据，满则等待 */
static void buffer_put(BoundedBuffer *bb, int value) {
    pthread_mutex_lock(&bb->mutex);

    /* 缓冲区满时等待 not_full 条件 */
    while (bb->count >= BUFFER_SIZE) {
        printf("  缓冲区满，生产者等待...\n");
        pthread_cond_wait(&bb->not_full, &bb->mutex);
    }

    /* 放入数据 */
    bb->buffer[bb->tail] = value;
    bb->tail = (bb->tail + 1) % BUFFER_SIZE;
    bb->count++;
    printf("  生产者: 放入 %d (缓冲区: %d/%d)\n",
           value, bb->count, BUFFER_SIZE);

    /* 通知等待的消费者 */
    pthread_cond_signal(&bb->not_empty);
    pthread_mutex_unlock(&bb->mutex);
}

/** 消费者：取出数据，空则等待 */
static int buffer_get(BoundedBuffer *bb) {
    pthread_mutex_lock(&bb->mutex);

    /* 缓冲区空时等待 not_empty 条件 */
    while (bb->count <= 0) {
        printf("  缓冲区空，消费者等待...\n");
        pthread_cond_wait(&bb->not_empty, &bb->mutex);
    }

    /* 取出数据 */
    int value = bb->buffer[bb->head];
    bb->head = (bb->head + 1) % BUFFER_SIZE;
    bb->count--;
    printf("  消费者: 取出 %d (缓冲区: %d/%d)\n",
           value, bb->count, BUFFER_SIZE);

    /* 通知等待的生产者 */
    pthread_cond_signal(&bb->not_full);
    pthread_mutex_unlock(&bb->mutex);
    return value;
}

/* ============================================================================
 * 广播演示：一次通知所有等待线程
 * ============================================================================ */

static pthread_mutex_t barrier_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  barrier_cond  = PTHREAD_COND_INITIALIZER;
static int barrier_ready = 0;

/** 等待屏障的线程 */
static void *barrier_waiter(void *arg) {
    int id = *(int *)arg;
    pthread_mutex_lock(&barrier_mutex);

    while (!barrier_ready) {
        printf("  线程%d: 等待屏障...\n", id);
        pthread_cond_wait(&barrier_cond, &barrier_mutex);
    }

    printf("  线程%d: 通过屏障！\n", id);
    pthread_mutex_unlock(&barrier_mutex);
    return NULL;
}

/** 触发屏障（广播） */
static void *barrier_trigger(void *arg) {
    (void)arg;
    usleep(200000);  /* 确保等待线程先就绪 */

    pthread_mutex_lock(&barrier_mutex);
    barrier_ready = 1;
    printf("\n  触发者: pthread_cond_broadcast 唤醒所有等待线程!\n");
    pthread_cond_broadcast(&barrier_cond);
    pthread_mutex_unlock(&barrier_mutex);
    return NULL;
}

/* ============================================================================
 * 顶级线程函数（C11 不支持嵌套函数或 auto 返回类型推断）
 * ============================================================================ */

/** 生产者线程包装函数 */
typedef struct {
    BoundedBuffer *bb;
    int *data;
    int start;
    int end;
} ProdArg;

static void *producer_thread(void *arg) {
    ProdArg *pa = (ProdArg *)arg;
    for (int i = pa->start; i < pa->end; i++) {
        buffer_put(pa->bb, pa->data[i]);
        usleep(20000);
    }
    return NULL;
}

/** 消费者线程包装函数 */
static void *consumer_thread(void *arg) {
    BoundedBuffer *bb = (BoundedBuffer *)arg;
    for (int i = 0; i < 10; i++) {
        buffer_get(bb);
        usleep(50000);
    }
    return NULL;
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void)
{
    printf("=== 条件变量 —— 生产者-消费者与有界缓冲区 ===\n\n");

    /* ---------------------------------------------------------------
     * 1. 有界缓冲区（信号模式）
     * --------------------------------------------------------------- */
    printf("--- 1. 有界缓冲区 (pthread_cond_signal) ---\n");
    printf("  缓冲区大小 = %d\n\n", BUFFER_SIZE);

    BoundedBuffer bb;
    buffer_init(&bb);

    static int prod_data[] = {11, 22, 33, 44, 55, 66, 77, 88, 99, 100};

    ProdArg args[2];
    args[0].bb = &bb; args[0].data = prod_data; args[0].start = 0; args[0].end = 5;
    args[1].bb = &bb; args[1].data = prod_data; args[1].start = 5; args[1].end = 10;

    pthread_t producers[2], consumer;
    pthread_create(&producers[0], NULL, producer_thread, &args[0]);
    pthread_create(&producers[1], NULL, producer_thread, &args[1]);
    pthread_create(&consumer, NULL, consumer_thread, &bb);

    pthread_join(producers[0], NULL);
    pthread_join(producers[1], NULL);
    pthread_join(consumer, NULL);

    buffer_destroy(&bb);

    /* ---------------------------------------------------------------
     * 2. 条件变量广播
     * --------------------------------------------------------------- */
    printf("\n--- 2. pthread_cond_broadcast 演示 ---\n");
    printf("  多个线程等待屏障，触发者一次唤醒所有\n\n");

    int ids[] = {1, 2, 3, 4};
    pthread_t waiters[4], trigger;

    for (int i = 0; i < 4; i++) {
        pthread_create(&waiters[i], NULL, barrier_waiter, &ids[i]);
    }
    pthread_create(&trigger, NULL, barrier_trigger, NULL);

    for (int i = 0; i < 4; i++) {
        pthread_join(waiters[i], NULL);
    }
    pthread_join(trigger, NULL);

    pthread_mutex_destroy(&barrier_mutex);
    pthread_cond_destroy(&barrier_cond);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
