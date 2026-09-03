/**
 * @file concurrency-rcu/main.c
 * @brief RCU (Read-Copy-Update) 机制概念演示
 *
 * 说明：真正的 RCU 需要内核支持，这里用用户态 epoch 机制模拟 RCU 的
 * 核心思想——读侧无锁、写侧通过宽限期（grace period）安全回收旧数据。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/* =============================================================================
 * RCU 数据结构
 *
 * 使用全局 epoch 计数器 + 每线程读侧计数器来模拟宽限期。
 * 读侧进入临界区时递增计数器，退出时递减。
 * 写者更新指针后等待所有读者计数器归零（宽限期结束）。
 * ============================================================================= */

typedef struct {
    int id;
    char name[32];
} data_t;

static atomic_uintptr_t g_data = ATOMIC_VAR_INIT(0);    /* 共享指针 */
static atomic_uint g_reader_active = ATOMIC_VAR_INIT(0);  /* 活跃读者数 */
static atomic_bool g_running = ATOMIC_VAR_INIT(true);     /* 运行标志 */

/* =============================================================================
 * 读侧 - 无锁读取
 *
 * RCU 的读侧临界区是 wait-free 的：仅需原子加载指针，无需加锁。
 * 这里用计数来近似宽限期感知。
 * ============================================================================= */

void rcu_read_lock(void) {
    atomic_fetch_add(&g_reader_active, 1);
}

void rcu_read_unlock(void) {
    atomic_fetch_sub(&g_reader_active, 1);
}

data_t *rcu_dereference(void) {
    return (data_t *)atomic_load(&g_data);
}

/* =============================================================================
 * 写侧 - 更新并等待宽限期
 *
 * 1. 分配新数据并复制旧内容
 * 2. 原子更新指针（RCU 的 "copy" + "update"）
 * 3. 等待宽限期：所有在读侧进入前就开始的读者必须退出
 * ============================================================================= */

void rcu_synchronize(void) {
    /* 等待当前活跃读者全部退出 */
    while (atomic_load(&g_reader_active) > 0) {
        sched_yield();
    }
}

void rcu_assign_pointer(data_t *new_data) {
    data_t *old = (data_t *)atomic_exchange(&g_data, (uintptr_t)new_data);
    /* 宽限期：等待所有在旧指针下的读者完成 */
    rcu_synchronize();
    free(old);  /* 宽限期结束后安全回收旧数据 */
}

/* =============================================================================
 * 读者线程
 * ============================================================================= */

void *reader_thread(void *arg) {
    int id = *(int *)arg;
    while (atomic_load(&g_running)) {
        rcu_read_lock();
        data_t *d = rcu_dereference();
        if (d) {
            printf("[读者 %d] 读到 data{id=%d, name=%s}\n",
                   id, d->id, d->name);
        }
        rcu_read_unlock();
        usleep(50000);  /* 50ms */
    }
    printf("[读者 %d] 退出\n", id);
    return NULL;
}

/* =============================================================================
 * 写者线程
 * ============================================================================= */

void *writer_thread(void *arg) {
    (void)arg;
    int count = 0;
    while (count < 5) {
        usleep(200000);  /* 200ms */

        data_t *new_data = (data_t *)malloc(sizeof(data_t));
        assert(new_data);
        new_data->id = ++count;
        snprintf(new_data->name, sizeof(new_data->name), "version-%d", count);

        printf("\n[写者] 更新为 data{id=%d, name=%s} ...\n",
               new_data->id, new_data->name);
        rcu_assign_pointer(new_data);
        printf("[写者] 宽限期结束，旧数据已回收\n\n");
    }
    atomic_store(&g_running, false);
    return NULL;
}

/* =============================================================================
 * 主函数
 * ============================================================================= */

int main(void) {
    printf("=== RCU (Read-Copy-Update) 概念演示 ===\n\n");
    printf("说明：用户态 epoch 模拟 RCU 宽限期\n");
    printf("      读侧无锁（wait-free），写侧需等待读者退出\n");
    printf("      真正的 RCU 需要内核支持（如 Linux 内核的 rcu_read_lock）\n\n");

    /* 初始化共享数据 */
    data_t *init = (data_t *)malloc(sizeof(data_t));
    assert(init);
    init->id = 0;
    snprintf(init->name, sizeof(init->name), "initial");
    atomic_store(&g_data, (uintptr_t)init);

    /* 启动读者线程 */
    pthread_t readers[3];
    int ids[3] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        pthread_create(&readers[i], NULL, reader_thread, &ids[i]);
    }

    /* 启动写者线程 */
    pthread_t writer;
    pthread_create(&writer, NULL, writer_thread, NULL);

    /* 等待写者完成 */
    pthread_join(writer, NULL);

    /* 等待读者退出 */
    for (int i = 0; i < 3; i++) {
        pthread_join(readers[i], NULL);
    }

    /* 清理 */
    data_t *final = (data_t *)atomic_load(&g_data);
    free(final);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
