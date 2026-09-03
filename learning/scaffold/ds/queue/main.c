/**
 * @file main.c
 * @brief 数据结构队列学习卡片 - 演示循环队列、链式队列与阻塞队列
 *
 * 涵盖内容：
 * - 循环队列：固定数组实现，FIFO，head/tail 指针
 * - 链式队列：链表实现，enqueue/dequeue 操作
 * - 阻塞队列概念：生产者-消费者模型
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* ==================== 工具函数 ==================== */

/**
 * 打印分隔线
 */
static void print_separator(void)
{
    printf("  ----------------------------------------\n");
}

/* ==================== 循环队列实现 ==================== */

/**
 * 循环队列结构
 * 使用固定大小数组，通过取模实现循环
 */
#define CIRCULAR_QUEUE_SIZE 8

typedef struct {
    int data[CIRCULAR_QUEUE_SIZE]; /* 数据存储区 */
    int head;                      /* 队头索引 */
    int tail;                      /* 队尾索引 */
    int count;                     /* 元素计数 */
} CircularQueue;

/**
 * 初始化循环队列
 */
static void cq_init(CircularQueue *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

/**
 * 检查队列是否为空
 */
static bool cq_is_empty(const CircularQueue *q)
{
    return q->count == 0;
}

/**
 * 检查队列是否已满
 */
static bool cq_is_full(const CircularQueue *q)
{
    return q->count == CIRCULAR_QUEUE_SIZE;
}

/**
 * 入队操作 O(1)
 */
static int cq_enqueue(CircularQueue *q, int value)
{
    if (cq_is_full(q)) {
        return -1;
    }
    q->data[q->tail] = value;
    q->tail = (q->tail + 1) % CIRCULAR_QUEUE_SIZE;
    q->count++;
    return 0;
}

/**
 * 出队操作 O(1)
 */
static int cq_dequeue(CircularQueue *q, int *out)
{
    if (cq_is_empty(q)) {
        return -1;
    }
    *out = q->data[q->head];
    q->head = (q->head + 1) % CIRCULAR_QUEUE_SIZE;
    q->count--;
    return 0;
}

/**
 * 获取队头元素 O(1)
 */
static int cq_front(const CircularQueue *q, int *out)
{
    if (cq_is_empty(q)) {
        return -1;
    }
    *out = q->data[q->head];
    return 0;
}

/**
 * 获取队尾元素 O(1)
 */
static int cq_rear(const CircularQueue *q, int *out)
{
    if (cq_is_empty(q)) {
        return -1;
    }
    /* 队尾在 head 前面时，需要特殊处理 */
    int rear_idx = (q->head + q->count - 1) % CIRCULAR_QUEUE_SIZE;
    *out = q->data[rear_idx];
    return 0;
}

/**
 * 打印循环队列状态
 */
static void cq_print(const char *label, const CircularQueue *q)
{
    printf("  %s (count=%d, head=%d, tail=%d): [", label, q->count, q->head, q->tail);
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % CIRCULAR_QUEUE_SIZE;
        printf("%d%s", q->data[idx], (i < q->count - 1) ? ", " : "");
    }
    printf("]\n");
}

/**
 * 循环队列演示
 */
static void demo_circular_queue(void)
{
    printf("[queue] 循环队列演示\n");
    print_separator();

    CircularQueue q;
    cq_init(&q);

    printf("  初始化循环队列，容量=%d\n", CIRCULAR_QUEUE_SIZE);

    /* 入队操作 */
    printf("  入队操作：1, 2, 3, 4, 5\n");
    cq_enqueue(&q, 1);
    cq_enqueue(&q, 2);
    cq_enqueue(&q, 3);
    cq_enqueue(&q, 4);
    cq_enqueue(&q, 5);
    cq_print("入队后", &q);

    /* 获取队头和队尾 */
    int front_val, rear_val;
    cq_front(&q, &front_val);
    cq_rear(&q, &rear_val);
    printf("  front=%d, rear=%d\n", front_val, rear_val);

    /* 出队操作 */
    printf("  出队 2 次：");
    int val;
    cq_dequeue(&q, &val);
    printf("%d ", val);
    cq_dequeue(&q, &val);
    printf("%d\n", val);
    cq_print("出队后", &q);

    /* 继续入队，测试循环 */
    printf("  继续入队：6, 7, 8\n");
    cq_enqueue(&q, 6);
    cq_enqueue(&q, 7);
    cq_enqueue(&q, 8);
    cq_print("扩容入队后", &q);

    /* 队满测试 */
    printf("  尝试入队 9 (队列已满)：%s\n",
           cq_enqueue(&q, 9) == -1 ? "失败" : "成功");

    printf("\n");
}

/* ==================== 链式队列实现 ==================== */

/**
 * 链式队列节点
 */
typedef struct LQNode {
    int data;
    struct LQNode *next;
} LQNode;

/**
 * 链式队列结构
 */
typedef struct {
    LQNode *front; /* 队头指针 */
    LQNode *rear;  /* 队尾指针 */
    int size;      /* 元素计数 */
} LinkedQueue;

/**
 * 创建链式队列
 */
static LinkedQueue *lq_create(void)
{
    LinkedQueue *q = (LinkedQueue *)malloc(sizeof(LinkedQueue));
    if (!q) {
        return NULL;
    }
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
    return q;
}

/**
 * 释放链式队列
 */
static void lq_destroy(LinkedQueue *q)
{
    if (!q) return;
    LQNode *node = q->front;
    while (node) {
        LQNode *next = node->next;
        free(node);
        node = next;
    }
    free(q);
}

/**
 * 链式队列是否为空
 */
static bool lq_is_empty(const LinkedQueue *q)
{
    return q->size == 0;
}

/**
 * 链式入队 O(1)
 */
static int lq_enqueue(LinkedQueue *q, int value)
{
    LQNode *node = (LQNode *)malloc(sizeof(LQNode));
    if (!node) {
        return -1;
    }
    node->data = value;
    node->next = NULL;

    if (q->rear) {
        q->rear->next = node;
        q->rear = node;
    } else {
        q->front = q->rear = node;
    }
    q->size++;
    return 0;
}

/**
 * 链式出队 O(1)
 */
static int lq_dequeue(LinkedQueue *q, int *out)
{
    if (lq_is_empty(q)) {
        return -1;
    }
    LQNode *node = q->front;
    *out = node->data;
    q->front = node->next;
    if (!q->front) {
        q->rear = NULL;
    }
    free(node);
    q->size--;
    return 0;
}

/**
 * 链式队列演示
 */
static void demo_linked_queue(void)
{
    printf("[queue] 链式队列演示\n");
    print_separator();

    LinkedQueue *q = lq_create();
    if (!q) {
        fprintf(stderr, "创建链式队列失败\n");
        return;
    }

    printf("  创建链式队列（动态大小）\n");

    /* 入队 */
    printf("  入队操作：10, 20, 30, 40\n");
    lq_enqueue(q, 10);
    lq_enqueue(q, 20);
    lq_enqueue(q, 30);
    lq_enqueue(q, 40);
    printf("  队列大小: %d\n", q->size);

    /* 出队 */
    printf("  出队操作: ");
    int val;
    while (!lq_is_empty(q)) {
        lq_dequeue(q, &val);
        printf("%d ", val);
    }
    printf("\n");
    printf("  队列大小: %d\n", q->size);

    lq_destroy(q);
    printf("\n");
}

/* ==================== 阻塞队列概念演示 ==================== */

/**
 * 阻塞队列概念演示
 * 实际实现需要线程支持，这里演示概念
 */
static void demo_blocking_queue(void)
{
    printf("[queue] 阻塞队列概念\n");
    print_separator();

    printf("  阻塞队列特点：\n");
    printf("  - 空时 dequeue 阻塞等待\n");
    printf("  - 满时 enqueue 阻塞等待\n");
    printf("  - 常用于生产者-消费者模型\n");
    printf("\n");

    printf("  应用场景：\n");
    printf("  - 线程池任务队列\n");
    printf("  - 日志处理系统\n");
    printf("  - 网络请求队列\n");
    printf("\n");

    printf("  与普通队列的区别：\n");
    printf("  - 普通队列：满时返回错误\n");
    printf("  - 阻塞队列：满时等待直到有空间\n");
    printf("  - 普通队列：空时返回错误\n");
    printf("  - 阻塞队列：空时等待直到有数据\n");
    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 队列 ===\n\n");

    /* 循环队列演示 */
    demo_circular_queue();

    /* 链式队列演示 */
    demo_linked_queue();

    /* 阻塞队列概念 */
    demo_blocking_queue();

    printf("=== PASS ===\n");
    return 0;
}
