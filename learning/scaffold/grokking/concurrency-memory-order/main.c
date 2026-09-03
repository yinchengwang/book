/**
 * @file concurrency-memory-order/main.c
 * @brief 内存序（Memory Order）概念演示
 *
 * 展示 C11 <stdatomic.h> 中不同内存序的效果：
 *   1. memory_order_relaxed   - 无同步约束，仅保证原子性
 *   2. memory_order_acquire   - 后续读/写不能被重排到 acquire 之前
 *   3. memory_order_release   - 之前的读/写不能被重排到 release 之后
 *   4. memory_order_seq_cst   - 全局顺序一致性
 *
 * 同时展示非原子变量的编译器优化导致的自旋失败。
 */

#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

/* =============================================================================
 * 1. Relaxed 顺序：仅保证原子性，不保证顺序
 *
 * 在 x86 上 relaxed 和 seq_cst 行为几乎一致（x86 天然强序），
 * 但在 ARM/POWER 上 relaxed 允许大量重排，可能产生意外结果。
 * ============================================================================= */

static atomic_int g_relaxed = 0;

void relaxed_demo(void) {
    printf("\n--- Relaxed Ordering ---\n");
    printf("  atomic_store(&flag, 1, memory_order_relaxed)\n");
    printf("  atomic_load(&flag, memory_order_relaxed)\n");
    printf("  仅保证原子性，不保证任何顺序约束\n");
    printf("  在弱序 CPU 上，消费者可能看到 'flag=1' 但还没看到前置写入\n");

    /* 概念演示：relaxed 的计数器递增 */
    atomic_store(&g_relaxed, 0);
    for (int i = 0; i < 5; i++) {
        atomic_fetch_add_explicit(&g_relaxed, 1, memory_order_relaxed);
    }
    printf("  relaxed 计数器结果: %d\n",
           atomic_load_explicit(&g_relaxed, memory_order_relaxed));
}

/* =============================================================================
 * 2. Acquire-Release 顺序：生产者-消费者模式
 *
 * release：保证 release 之前的写入在 acquire 之后可见
 * acquire：保证 acquire 之后的读取不会看到 release 之前未完成的数据
 * ============================================================================= */

static atomic_bool g_ready = ATOMIC_VAR_INIT(false);
static int g_payload = 0;

void *producer(void *arg) {
    (void)arg;
    usleep(100000);  /* 100ms */
    g_payload = 42;
    /* release 屏障：g_payload 的写入不会被重排到 g_ready 之后 */
    atomic_store_explicit(&g_ready, true, memory_order_release);
    printf("[生产者] 已发布 payload=%d (release)\n", g_payload);
    return NULL;
}

void *consumer(void *arg) {
    (void)arg;
    /* acquire 屏障：确保能看到 ready=true 之前的 g_payload 写入 */
    while (!atomic_load_explicit(&g_ready, memory_order_acquire)) {
        sched_yield();
    }
    printf("[消费者] 已接收 payload=%d (acquire)\n", g_payload);
    return NULL;
}

void acquire_release_demo(void) {
    printf("\n--- Acquire-Release Ordering ---\n");
    printf("  生产者：写 payload -> release 写 flag\n");
    printf("  消费者：acquire 读 flag -> 读 payload\n");
    printf("  release 之前的写不会被重排到后面\n");
    printf("  acquire 之后的读不会被重排到前面\n\n");

    g_ready = false;
    g_payload = 0;

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons, NULL, consumer, NULL);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
}

/* =============================================================================
 * 3. Sequential Consistency（顺序一致性）
 *
 * 默认 memory_order_seq_cst，是可移植的"安全默认值"。
 * 所有 seq_cst 操作形成一个全局全序（global total order）。
 * 代价：在弱序 CPU 上需要内存屏障指令（如 ARM dmb），性能略低于 acquire/release。
 * ============================================================================= */

static atomic_int g_seq_a = 0;
static atomic_int g_seq_b = 0;
static atomic_int g_seq_x = 0;
static atomic_int g_seq_y = 0;

void *seq_thread_a(void *arg) {
    (void)arg;
    atomic_store_explicit(&g_seq_a, 1, memory_order_seq_cst);
    g_seq_x = atomic_load_explicit(&g_seq_b, memory_order_seq_cst);
    printf("  线程 A: a=1, 读到 b=%d\n", (int)g_seq_x);
    return NULL;
}

void *seq_thread_b(void *arg) {
    (void)arg;
    atomic_store_explicit(&g_seq_b, 1, memory_order_seq_cst);
    g_seq_y = atomic_load_explicit(&g_seq_a, memory_order_seq_cst);
    printf("  线程 B: b=1, 读到 a=%d\n", (int)g_seq_y);
    return NULL;
}

void seq_cst_demo(void) {
    printf("\n--- Sequential Consistency ---\n");
    printf("  所有 seq_cst 操作形成一个全局全序\n");
    printf("  不会出现 x==0 && y==0 的情况（全局一致序保证）\n\n");

    /* 多次运行以展示一致性 */
    for (int run = 0; run < 3; run++) {
        atomic_store(&g_seq_a, 0);
        atomic_store(&g_seq_b, 0);
        atomic_store(&g_seq_x, -1);
        atomic_store(&g_seq_y, -1);

        pthread_t ta, tb;
        pthread_create(&ta, NULL, seq_thread_a, NULL);
        pthread_create(&tb, NULL, seq_thread_b, NULL);
        pthread_join(ta, NULL);
        pthread_join(tb, NULL);

        printf("  运行 %d: x=%d, y=%d\n", run + 1,
               (int)atomic_load(&g_seq_x), (int)atomic_load(&g_seq_y));
    }
}

/* =============================================================================
 * 4. 非原子变量的编译器优化（错误示范）
 *
 * 下面展示一个"错误"的自旋模式。非原子变量在循环中被读取时，
 * 编译器可能将其优化为只从寄存器读取（不重新加载内存），导致自旋永远看不到变化。
 *
 * 正确的做法：使用 atomic_flag 或 atomic_bool + memory_order_acquire。
 * ============================================================================= */

static bool g_bad_flag = false;  /* 非原子！ */

void *bad_spin_writer(void *arg) {
    (void)arg;
    usleep(200000);  /* 200ms */
    g_bad_flag = true;
    printf("[写者] g_bad_flag = true\n");
    return NULL;
}

void bad_spin_demo(void) {
    printf("\n--- 非原子自旋（编译器优化问题） ---\n");
    printf("  使用 volatile 可防止优化，但 volatile 不保证内存序\n");
    printf("  正确做法：使用 atomic_flag 或 atomic_bool + acq_rel\n");

    g_bad_flag = false;
    pthread_t writer;
    pthread_create(&writer, NULL, bad_spin_writer, NULL);

    /* 错误的自旋：编译器可能将 g_bad_flag 的读取优化为寄存器常量 */
    int attempts = 0;
    while (!g_bad_flag) {
        sched_yield();
        attempts++;
        if (attempts > 500) {
            printf("  [读者] 警告：过多次自旋，g_bad_flag 未被检测到\n");
            printf("  原因：编译器将 while(!g_bad_flag) 优化为 while(1)\n");
            break;
        }
    }

    if (g_bad_flag) {
        printf("  [读者] 自旋 %d 次后检测到 g_bad_flag 变化\n", attempts);
    } else {
        printf("  [读者] 已超限，演示编译器优化问题\n");
    }

    pthread_join(writer, NULL);
}

/* =============================================================================
 * 主函数
 * ============================================================================= */

int main(void) {
    printf("=== 内存序 (Memory Order) 概念演示 ===\n\n");
    printf("基于 C11 <stdatomic.h> 的内存序模型\n");
    printf("注意：x86 是强序 CPU，relaxed/acquire/release/seq_cst 差异很小\n");
    printf("在 ARM/POWER 弱序 CPU 上差异显著\n");

    relaxed_demo();
    acquire_release_demo();
    seq_cst_demo();
    bad_spin_demo();

    printf("\n=== 演示完成 ===\n");
    return 0;
}
