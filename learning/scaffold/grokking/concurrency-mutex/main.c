/**
 * @file concurrency-mutex/main.c
 * @brief 互斥锁：保护共享数据、读写锁与死锁演示
 *
 * 演示内容:
 * 1. pthread_mutex_lock/unlock 保护共享计数器
 * 2. 死锁场景（ABBA 死锁锁）
 * 3. pthread_rwlock_t 读写锁：多读单写
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * 1. 互斥锁保护共享数据
 * ============================================================================ */

#define INCR_LOOPS 500000

static int safe_counter = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/** 线程函数：加锁后递增共享计数器 */
static void *safe_increment(void *arg) {
    (void)arg;
    for (int i = 0; i < INCR_LOOPS; i++) {
        pthread_mutex_lock(&mutex);
        safe_counter++;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

/* ============================================================================
 * 2. 死锁演示（ABBA 死锁）
 * ============================================================================ */

static pthread_mutex_t lock_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock_b = PTHREAD_MUTEX_INITIALIZER;

/** 线程函数：先锁 A 再锁 B */
static void *deadlock_thread1(void *arg) {
    (void)arg;
    printf("  线程1: 尝试获取锁 A...\n");
    pthread_mutex_lock(&lock_a);
    printf("  线程1: 拿到锁 A，尝试获取锁 B...\n");
    /* 模拟一点工作，让线程 2 有机会拿到锁 B */
    usleep(10000);
    pthread_mutex_lock(&lock_b);
    printf("  线程1: 拿到锁 A 和 B\n");
    pthread_mutex_unlock(&lock_b);
    pthread_mutex_unlock(&lock_a);
    return NULL;
}

/** 线程函数：先锁 B 再锁 A（和线程 1 顺序相反 → 死锁） */
static void *deadlock_thread2(void *arg) {
    (void)arg;
    printf("  线程2: 尝试获取锁 B...\n");
    pthread_mutex_lock(&lock_b);
    printf("  线程2: 拿到锁 B，尝试获取锁 A...\n");
    usleep(10000);
    pthread_mutex_lock(&lock_a);
    printf("  线程2: 拿到锁 B 和 A\n");
    pthread_mutex_unlock(&lock_a);
    pthread_mutex_unlock(&lock_b);
    return NULL;
}

/* ============================================================================
 * 3. 读写锁演示
 * ============================================================================ */

static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
static int shared_data = 0;
static int reader_count = 0;   /* 当前正在读的读者数 */
static pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;

/** 读者线程：获取读锁 */
static void *reader_thread(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 3; i++) {
        pthread_rwlock_rdlock(&rwlock);
        pthread_mutex_lock(&reader_mutex);
        reader_count++;
        printf("  读者%d: 开始读 (当前 %d 个读者), 数据 = %d\n",
               id, reader_count, shared_data);
        pthread_mutex_unlock(&reader_mutex);

        usleep(50000);  /* 模拟读取耗时 */

        pthread_mutex_lock(&reader_mutex);
        reader_count--;
        printf("  读者%d: 结束读 (剩余 %d 个读者)\n", id, reader_count);
        pthread_mutex_unlock(&reader_mutex);
        pthread_rwlock_unlock(&rwlock);

        usleep(30000);
    }
    return NULL;
}

/** 写者线程：获取写锁（独占） */
static void *writer_thread(void *arg) {
    (void)arg;
    for (int i = 0; i < 2; i++) {
        pthread_rwlock_wrlock(&rwlock);
        shared_data++;
        printf("  写者: 写入数据 = %d (独占写锁)\n", shared_data);
        usleep(100000);
        pthread_rwlock_unlock(&rwlock);

        usleep(80000);
    }
    return NULL;
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void)
{
    printf("=== 互斥锁 —— mutex、死锁与读写锁 ===\n\n");

    /* ---------------------------------------------------------------
     * 1. 互斥锁保护共享数据
     * --------------------------------------------------------------- */
    printf("--- 1. 互斥锁保护共享计数器 ---\n");
    printf("  两个线程各递增 %d 次（加锁保护）\n", INCR_LOOPS);

    pthread_t t1, t2;
    pthread_create(&t1, NULL, safe_increment, NULL);
    pthread_create(&t2, NULL, safe_increment, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("  期望值 = %d, 实际值 = %d %s\n",
           2 * INCR_LOOPS, safe_counter,
           safe_counter == 2 * INCR_LOOPS ? "✓ 正确" : "✗ 错误");

    /* ---------------------------------------------------------------
     * 2. 死锁演示
     * --------------------------------------------------------------- */
    printf("\n--- 2. 死锁演示（ABBA 死锁） ---\n");
    printf("  两个线程按相反顺序加锁 → 将发生死锁\n");
    printf("  若程序卡住不动即死锁！（等待 1 秒后自动继续）\n");

    /*
     * 死锁检测技巧：
     * 用分离线程运行死锁，主线程只等 1 秒即继续
     */
    pthread_t d1, d2;
    pthread_create(&d1, NULL, deadlock_thread1, NULL);
    pthread_create(&d2, NULL, deadlock_thread2, NULL);

    /* 等待最多 1 秒，如果线程死锁则超时继续 */
    struct timespec ts = {0, 500000000L}; /* 500ms */
    pthread_detach(d1);
    pthread_detach(d2);
    nanosleep(&ts, NULL);
    printf("  (主线程超时继续，死锁线程仍在等待)\n");

    /* 演示用：重置锁状态（实际工程中应避免死锁发生） */
    pthread_mutex_destroy(&lock_a);
    pthread_mutex_destroy(&lock_b);

    /* ---------------------------------------------------------------
     * 3. 读写锁演示
     * --------------------------------------------------------------- */
    printf("\n--- 3. 读写锁 (pthread_rwlock_t) ---\n");
    printf("  多个读者可同时读，写者独占\n\n");

    pthread_rwlock_init(&rwlock, NULL);

    int ids[] = {1, 2, 3};
    pthread_t readers[3], writer;

    pthread_create(&writer, NULL, writer_thread, NULL);
    for (int i = 0; i < 3; i++) {
        pthread_create(&readers[i], NULL, reader_thread, &ids[i]);
    }

    pthread_join(writer, NULL);
    for (int i = 0; i < 3; i++) {
        pthread_join(readers[i], NULL);
    }

    pthread_rwlock_destroy(&rwlock);

    /* 清理全局锁 */
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&reader_mutex);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
