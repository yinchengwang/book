/**
 * @file concurrency-lockfree/main.c
 * @brief 无锁数据结构演示——Lock-Free Stack 与 ABA 问题
 *
 * 包含:
 * 1. 无锁栈（基于 CAS atomic_compare_exchange_strong）
 * 2. ABA 问题演示：CAS 无法区分同一指针的两次不同状态
 * 3. 标签指针（Tagged Pointer）缓解 ABA
 * 4. 无锁计数器 vs 互斥锁计数器性能对比
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

/* =============================================================================
 * 1. Lock-Free Stack (无锁栈)
 *
 * 基于 CAS (Compare-And-Swap) 实现无锁栈的 push 和 pop 操作。
 *
 * ABA 问题：
 *   T1 读取 top -> A
 *   T1 准备 CAS(top, A, C) 但被暂停
 *   T2 pop A, push B, push A (A 被重用)
 *   T1 CAS 成功，但 top->next 已被 T2 修改，导致结构损坏
 * ============================================================================= */

typedef struct node_t {
    int value;
    struct node_t *next;
} node_t;

/* 将指针转为 uintptr_t 后存入 atomic_uintptr_t，规避 _Atomic(T*) 的兼容性问题 */
static atomic_uintptr_t g_stack_top = ATOMIC_VAR_INIT(0);

/* 无锁 push */
void lf_push(int val) {
    node_t *new_node = (node_t *)malloc(sizeof(node_t));
    new_node->value = val;
    uintptr_t old = atomic_load(&g_stack_top);
    do {
        new_node->next = (node_t *)old;
    } while (!atomic_compare_exchange_weak(&g_stack_top, &old,
                                           (uintptr_t)new_node));
}

/* 无锁 pop（易受 ABA 影响） */
int lf_pop(int *out) {
    uintptr_t old = atomic_load(&g_stack_top);
    node_t *next;
    do {
        if (old == 0) return -1;  /* 栈空 */
        next = ((node_t *)old)->next;
    } while (!atomic_compare_exchange_weak(&g_stack_top, &old,
                                           (uintptr_t)next));
    *out = ((node_t *)old)->value;
    free((node_t *)old);
    return 0;
}

/* =============================================================================
 * 2. 标签指针 - 缓解 ABA 问题
 *
 * 将指针和标签打包成一个 uintptr_t：
 *   高 N 位 = 标签，低 N 位 = 指针（指针低 2-4 位因对齐而空闲）
 * 每次 CAS 同时检查指针和标签，即使指针值重复（ABA），
 * 标签不同会导致 CAS 失败。
 *
 * 注意：实际生产环境建议使用专门的 hazard pointer 或 RCU 机制，
 * 这里仅为概念演示。
 * ============================================================================= */

#define TAG_SHIFT 3
#define TAG_MASK  (~((uintptr_t)((1UL << TAG_SHIFT) - 1)))
#define TAG_INC   (1UL << TAG_SHIFT)

static atomic_uintptr_t g_tagged_stack = ATOMIC_VAR_INIT(0);

void tp_push(int val) {
    node_t *new_node = (node_t *)malloc(sizeof(node_t));
    new_node->value = val;

    uintptr_t old = atomic_load(&g_tagged_stack);
    uintptr_t desired;
    do {
        new_node->next = (node_t *)(old & TAG_MASK);
        desired = ((uintptr_t)new_node) | ((old + TAG_INC) & ~TAG_MASK);
    } while (!atomic_compare_exchange_weak(&g_tagged_stack, &old, desired));
}

int tp_pop(int *out) {
    uintptr_t old = atomic_load(&g_tagged_stack);
    uintptr_t desired;
    do {
        node_t *ptr = (node_t *)(old & TAG_MASK);
        if (ptr == NULL) return -1;
        desired = ((uintptr_t)(ptr->next)) | ((old + TAG_INC) & ~TAG_MASK);
    } while (!atomic_compare_exchange_weak(&g_tagged_stack, &old, desired));

    *out = ((node_t *)(old & TAG_MASK))->value;
    free((node_t *)(old & TAG_MASK));
    return 0;
}

/* =============================================================================
 * 3. 无锁计数器 vs 互斥锁计数器 性能对比
 * ============================================================================= */

static atomic_long g_lockfree_counter = 0;
static long g_mutex_counter = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int n;
} thread_arg_t;

void *lf_counter_thread(void *arg) {
    int n = ((thread_arg_t *)arg)->n;
    for (int i = 0; i < n; i++) {
        atomic_fetch_add(&g_lockfree_counter, 1);
    }
    return NULL;
}

void *mutex_counter_thread(void *arg) {
    int n = ((thread_arg_t *)arg)->n;
    for (int i = 0; i < n; i++) {
        pthread_mutex_lock(&g_mutex);
        g_mutex_counter++;
        pthread_mutex_unlock(&g_mutex);
    }
    return NULL;
}

long time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

/* =============================================================================
 * 主函数
 * ============================================================================= */

int main(void) {
    printf("=== Lock-Free 无锁数据结构演示 ===\n\n");

    /* --------------------------------------------------------------
     * 1. 无锁栈测试
     * -------------------------------------------------------------- */
    printf("--- 1. 无锁栈 (CAS 实现) ---\n");
    lf_push(10);
    lf_push(20);
    lf_push(30);
    int val;
    while (lf_pop(&val) == 0) {
        printf("  pop: %d\n", val);
    }

    /* --------------------------------------------------------------
     * 2. 标签指针栈测试
     * -------------------------------------------------------------- */
    printf("\n--- 2. 标签指针栈 (缓解 ABA) ---\n");
    tp_push(100);
    tp_push(200);
    while (tp_pop(&val) == 0) {
        printf("  tagged pop: %d\n", val);
    }

    /* --------------------------------------------------------------
     * 3. 性能对比：100 万次递增，4 线程
     * -------------------------------------------------------------- */
    printf("\n--- 3. 性能对比 (4 线程, 1000000 次/线程) ---\n");

    const int N = 1000000;
    pthread_t threads[4];
    long t0, t1;

    /* 无锁版 */
    g_lockfree_counter = 0;
    t0 = time_ns();
    thread_arg_t lf_args[4];
    for (int i = 0; i < 4; i++) {
        lf_args[i].n = N;
        pthread_create(&threads[i], NULL, lf_counter_thread, &lf_args[i]);
    }
    for (int i = 0; i < 4; i++) pthread_join(threads[i], NULL);
    t1 = time_ns();
    printf("  无锁计数器: %ld (%.3f ms)\n",
           (long)atomic_load(&g_lockfree_counter),
           (double)(t1 - t0) / 1e6);

    /* 互斥锁版 */
    g_mutex_counter = 0;
    t0 = time_ns();
    thread_arg_t mutex_args[4];
    for (int i = 0; i < 4; i++) {
        mutex_args[i].n = N;
        pthread_create(&threads[i], NULL, mutex_counter_thread, &mutex_args[i]);
    }
    for (int i = 0; i < 4; i++) pthread_join(threads[i], NULL);
    t1 = time_ns();
    printf("  互斥锁计数器: %ld (%.3f ms)\n",
           (long)g_mutex_counter,
           (double)(t1 - t0) / 1e6);

    pthread_mutex_destroy(&g_mutex);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
