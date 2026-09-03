/**
 * @file main.c
 * @brief RCU（Read-Copy-Update）机制演示
 *
 * 演示 RCU 锁原理：读侧临界区、写侧宽限期
 * 展示无锁数据结构在数据库中的应用
 *
 * 编译：gcc -std=c11 -o rcu main.c -lpthread
 * 运行：./rcu
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/* RCU 链表节点 */
typedef struct rcu_node {
    int data;
    struct rcu_node *next;
} RcuNode;

/* 简单的 RCU 链表（模拟）*/
typedef struct {
    RcuNode *head;
    pthread_mutex_t write_lock;
    int read_count;
    int write_count;
} RcuList;

/* 全局 RCU 链表 */
static RcuList g_rcu_list = {NULL, PTHREAD_MUTEX_INITIALIZER, 0, 0};
static volatile int g_running = 1;

/* RCU 读操作：不需要锁！ */
static void rcu_read_side(void) {
    /* 读侧临界区——无需锁，开销极小 */
    __atomic_fetch_add(&g_rcu_list.read_count, 1, __ATOMIC_RELAXED);

    /* 遍历链表 */
    long sum = 0;
    int count = 0;
    RcuNode *p = __atomic_load_n(&g_rcu_list.head, __ATOMIC_ACQUIRE);
    while (p) {
        sum += p->data;
        count++;
        p = p->next;
    }

    /* 读侧结束——cost = 0 */
    __atomic_sub_fetch(&g_rcu_list.read_count, 1, __ATOMIC_RELAXED);

    if (count > 0) {
        printf("[RCU 读] 遍历 %d 个节点, sum=%ld\n", count, sum);
    }
}

/* RCU 写操作：需要宽限期 */
static void rcu_write_side(int data) {
    /* 写侧——持有互斥锁（单写者）*/
    pthread_mutex_lock(&g_rcu_list.write_lock);

    __atomic_fetch_add(&g_rcu_list.write_count, 1, __ATOMIC_RELAXED);

    /* 创建新节点 */
    RcuNode *new_node = malloc(sizeof(RcuNode));
    new_node->data = data;
    new_node->next = NULL;

    RcuNode *old_head = g_rcu_list.head;

    if (!old_head) {
        /* 空链表：直接更新 */
        __atomic_store_n(&g_rcu_list.head, new_node, __ATOMIC_RELEASE);
    } else {
        /* 头插法：创建新头 */
        new_node->next = old_head;
        __atomic_store_n(&g_rcu_list.head, new_node, __ATOMIC_RELEASE);

        /* 模拟同步等待（宽限期）—— readers 退出后 free */
        printf("[RCU 写] 节点 %d 已发布，等待宽限期...\n", data);
        /* 真实 RCU: synchronize_rcu() 等待所有 CPU 执行上下文切换 */
        /* 此处简化：等待旧节点没有读者 */
        usleep(1000);  /* 1ms 模拟宽限期 */

        /* 宽限期过后，旧节点可以安全释放 */
        printf("[RCU 写] 宽限期已过，旧节点可安全回收\n");
    }

    __atomic_sub_fetch(&g_rcu_list.write_count, 1, __ATOMIC_RELAXED);
    pthread_mutex_unlock(&g_rcu_list.write_lock);
}

/* 读者线程 */
static void *reader_thread(void *arg) {
    int id = *(int *)arg;
    int ops = 0;

    while (g_running && ops < 50) {
        rcu_read_side();
        ops++;
        usleep(20000);  /* 20ms */
    }

    printf("[读者 %d] 完成 %d 次读操作\n", id, ops);
    return NULL;
}

/* 写者线程 */
static void *writer_thread(void *arg) {
    int id = *(int *)arg;
    int ops = 0;

    while (g_running && ops < 10) {
        rcu_write_side(ops * 100 + id);
        ops++;
        usleep(100000);  /* 100ms */
    }

    printf("[写者 %d] 完成 %d 次写操作\n", id, ops);
    return NULL;
}

int main(void) {
    printf("===========================================\n");
    printf("  RCU (Read-Copy-Update) 机制演示\n");
    printf("===========================================\n");

    /* 1. RCU 核心概念 */
    printf("\n=== 1. RCU 核心概念 ===\n");
    printf(
        "RCU 是无锁同步机制，核心思想:\n"
        "  - 读操作无锁（零开销）\n"
        "  - 写操作用 COW（Copy-on-Write）\n"
        "  - 宽限期（Grace Period）保证安全回收\n\n"
        "适用场景:\n"
        "  - 读多写少（数据库元数据/配置）\n"
        "  - 需要极高的读吞吐（网络路由表）\n"
        "  - 无法接受读侧锁开销（热点路径）\n"
    );

    /* 2. 启动读者和写者 */
    printf("\n=== 2. 读者/写者场景测试 ===\n");

    pthread_t readers[3], writer;
    int reader_ids[] = {1, 2, 3};
    int writer_id = 1;

    for (int i = 0; i < 3; i++) {
        pthread_create(&readers[i], NULL, reader_thread, &reader_ids[i]);
    }
    pthread_create(&writer, NULL, writer_thread, &writer_id);

    usleep(1500000);  /* 1.5 秒 */
    g_running = 0;

    for (int i = 0; i < 3; i++) {
        pthread_join(readers[i], NULL);
    }
    pthread_join(writer, NULL);

    /* 3. RCU 在 Linux 内核中的使用 */
    printf("\n\n=== 3. RCU 在 Linux 内核中的使用 ===\n");
    printf(
        "  - 网络路由表查询（fib_table_lookup）\n"
        "  - 文件描述符表查找\n"
        "  - 内核模块列表\n"
        "  - DCache/Inode 缓存\n"
        "  - 命名空间管理\n"
    );

    /* 4. RCU API 对比 */
    printf("\n\n=== 4. RCU 用户态 API ===\n");
    printf(
        "  读侧:\n"
        "    rcu_read_lock()    — 标记进入读临界区\n"
        "    rcu_read_unlock()  — 标记离开读临界区\n"
        "    rcu_dereference()  — 安全读取 RCU 保护的指针\n"
        "\n"
        "  写侧:\n"
        "    synchronize_rcu()  — 等待宽限期（阻塞）\n"
        "    call_rcu()         — 异步等待（回调方式）\n"
        "    rcu_assign_pointer() — 更新 RCU 保护的指针\n"
    );

    /* 5. 数据库应用 */
    printf("\n\n=== 5. 数据库中的 RCU 应用 ===\n");
    printf(
        "  - Catalog 缓存（表定义/索引定义）\n"
        "  - 查询计划缓存（预编译语句）\n"
        "  - GUC 配置参数读取\n"
        "  - 连接池中的路由表\n"
        "  - 快速路径锁（lock-free hash table）\n"
    );

    /* 清理 */
    RcuNode *p = g_rcu_list.head;
    while (p) {
        RcuNode *next = p->next;
        free(p);
        p = next;
    }

    printf("\n===========================================\n");
    printf("  RCU 机制演示完成\n");
    printf("===========================================\n");

    return 0;
}
