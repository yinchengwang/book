/**
 * @file concurrency-atomic/main.c
 * @brief C11 <stdatomic.h> 原子操作演示
 *
 * 演示内容:
 * 1. atomic_int 原子递增 vs mutex 保护递增性能对比
 * 2. atomic_compare_exchange_strong (CAS) 实现无锁栈
 * 3. atomic_flag 自旋锁
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>

#define NUM_OPS     100000
#define NUM_THREADS 4

/* ============================================================================
 * 1. 原子递增 vs Mutex 保护递增
 * ============================================================================ */

static atomic_int g_atomic_counter = 0;
static int g_mutex_counter = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *atomic_inc_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_OPS; i++) {
        atomic_fetch_add(&g_atomic_counter, 1);
    }
    return NULL;
}

static void *mutex_inc_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_OPS; i++) {
        pthread_mutex_lock(&g_mutex);
        g_mutex_counter++;
        pthread_mutex_unlock(&g_mutex);
    }
    return NULL;
}

static double run_benchmark(void *(*worker_fn)(void *), const char *name) {
    pthread_t threads[NUM_THREADS];
    g_atomic_counter = 0;  /* reset for atomic case */
    g_mutex_counter = 0;   /* reset for mutex case */

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker_fn, NULL);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("  %s: %.3fs\n", name, elapsed);
    return elapsed;
}

/* ============================================================================
 * 2. CAS 无锁栈 (Lock-free Stack)
 * ============================================================================ */

typedef struct LfNode {
    int value;
    struct LfNode *next;
} LfNode;

typedef struct {
    atomic_intptr_t top;  /* 栈顶指针，使用原子指针 */
} LockFreeStack;

static void lfs_push(LockFreeStack *s, int val) {
    LfNode *node = (LfNode *)malloc(sizeof(LfNode));
    node->value = val;

    LfNode *old_top;
    do {
        old_top = (LfNode *)atomic_load(&s->top);
        node->next = old_top;
    } while (!atomic_compare_exchange_strong(&s->top,
                 (intptr_t *)&old_top, (intptr_t)node));
}

static int lfs_pop(LockFreeStack *s) {
    LfNode *old_top;
    LfNode *new_top;
    do {
        old_top = (LfNode *)atomic_load(&s->top);
        if (old_top == NULL) return -1;
        new_top = old_top->next;
    } while (!atomic_compare_exchange_strong(&s->top,
                 (intptr_t *)&old_top, (intptr_t)new_top));

    int val = old_top->value;
    free(old_top);
    return val;
}

static void test_lockfree_stack(void) {
    printf("\n--- 2. CAS 无锁栈 ---\n");
    LockFreeStack stack = { .top = 0 };

    lfs_push(&stack, 10);
    lfs_push(&stack, 20);
    lfs_push(&stack, 30);

    printf("  出栈: %d\n", lfs_pop(&stack));
    printf("  出栈: %d\n", lfs_pop(&stack));
    printf("  出栈: %d\n", lfs_pop(&stack));
    printf("  空栈 pop: %d\n", lfs_pop(&stack));
}

/* ============================================================================
 * 3. atomic_flag 自旋锁
 * ============================================================================ */

typedef struct {
    atomic_flag flag;
} SpinLock;

static void spin_lock(SpinLock *sl) {
    while (atomic_flag_test_and_set(&sl->flag)) {
        /* 忙等待 */
    }
}

static void spin_unlock(SpinLock *sl) {
    atomic_flag_clear(&sl->flag);
}

static atomic_int g_spin_counter = 0;
static SpinLock g_spinlock = { .flag = ATOMIC_FLAG_INIT };

static void *spin_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_OPS; i++) {
        spin_lock(&g_spinlock);
        g_spin_counter++;
        spin_unlock(&g_spinlock);
    }
    return NULL;
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    printf("=== C11 原子操作演示 ===\n\n");

    printf("--- 1. 性能对比 (%d 线程 x %d 次递增) ---\n",
           NUM_THREADS, NUM_OPS);
    run_benchmark(atomic_inc_worker, "atomic_fetch_add");
    run_benchmark(mutex_inc_worker,   "mutex 保护");

    test_lockfree_stack();

    printf("\n--- 3. atomic_flag 自旋锁 ---\n");
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, spin_worker, NULL);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    printf("  自旋锁保护累加结果: %d (期望 %d)\n",
           (int)g_spin_counter, NUM_THREADS * NUM_OPS);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
