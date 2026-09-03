/**
 * @file concurrency-thread-basics/main.c
 * @brief 线程基础：创建、传参、返回值与竞态条件演示
 *
 * 演示内容:
 * 1. pthread_create / pthread_join 基本用法
 * 2. 向线程函数传递参数
 * 3. 从线程获取返回值
 * 4. 竞态条件（Race Condition）—— 无同步的多线程累加
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

/* ============================================================================
 * 1. 基础线程：打印 + 参数传递
 * ============================================================================ */

/** 线程函数：打印传入的消息 */
static void *print_message(void *arg) {
    char *msg = (char *)arg;
    printf("  子线程: \"%s\"\n", msg);
    return NULL;
}

/* ============================================================================
 * 2. 参数传递 + 返回值
 * ============================================================================ */

/** 传递给线程的复杂参数结构 */
typedef struct {
    int start;
    int end;
} Range;

/** 线程函数：计算 range 内整数之和并返回 */
static void *sum_range(void *arg) {
    Range *r = (Range *)arg;
    int *result = (int *)malloc(sizeof(int));
    if (!result) return NULL;
    *result = 0;
    for (int i = r->start; i <= r->end; i++) {
        *result += i;
    }
    printf("  子线程: sum(%d..%d) = %d\n", r->start, r->end, *result);
    return (void *)result;
}

/* ============================================================================
 * 3. 竞态条件演示
 * ============================================================================ */

#define RACE_LOOPS 1000000

static int shared_counter = 0;  /* 无保护共享变量 */

/** 线程函数：反复递增共享计数器（无锁） */
static void *race_increment(void *arg) {
    (void)arg;
    for (int i = 0; i < RACE_LOOPS; i++) {
        shared_counter++;  /* 非原子操作：读-改-写三步 */
    }
    return NULL;
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void)
{
    printf("=== 线程基础 —— 创建、传参、返回值与竞态条件 ===\n\n");

    /* ---------------------------------------------------------------
     * 1. 基础线程创建与等待
     * --------------------------------------------------------------- */
    printf("--- 1. pthread_create / pthread_join 基础 ---\n");

    pthread_t t1, t2;
    int err;

    err = pthread_create(&t1, NULL, print_message, "你好，线程！");
    if (err != 0) {
        fprintf(stderr, "创建线程 t1 失败: %s\n", strerror(err));
        return 1;
    }

    err = pthread_create(&t2, NULL, print_message, "第二个线程也在运行！");
    if (err != 0) {
        fprintf(stderr, "创建线程 t2 失败: %s\n", strerror(err));
        return 1;
    }

    /* 等待两个线程结束 */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("  两个线程已结束\n\n");

    /* ---------------------------------------------------------------
     * 2. 传递参数、获取返回值
     * --------------------------------------------------------------- */
    printf("--- 2. 传参 + 返回值 ---\n");

    Range r1 = {1, 100};
    pthread_t t3;
    void *retval = NULL;

    err = pthread_create(&t3, NULL, sum_range, &r1);
    if (err != 0) {
        fprintf(stderr, "创建线程 t3 失败: %s\n", strerror(err));
        return 1;
    }

    pthread_join(t3, &retval);
    if (retval) {
        int sum = *(int *)retval;
        printf("  主线程收到: sum(1..100) = %d\n\n", sum);
        free(retval);
    }

    /* ---------------------------------------------------------------
     * 3. 竞态条件
     * --------------------------------------------------------------- */
    printf("--- 3. 竞态条件演示 ---\n");
    printf("  两个线程各递增 %d 次，期望值 = %d\n",
           RACE_LOOPS, 2 * RACE_LOOPS);

    shared_counter = 0;
    pthread_t t4, t5;

    pthread_create(&t4, NULL, race_increment, NULL);
    pthread_create(&t5, NULL, race_increment, NULL);

    pthread_join(t4, NULL);
    pthread_join(t5, NULL);

    printf("  实际值 = %d  ", shared_counter);
    if (shared_counter != 2 * RACE_LOOPS) {
        printf("← 竞态条件！结果不正确\n");
    } else {
        printf("（巧合正确，下次运行可能不同）\n");
    }

    printf("\n=== 演示完成 ===\n");
    return 0;
}
