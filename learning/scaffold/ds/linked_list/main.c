/**
 * @file main.c
 * @brief 数据结构链表学习卡片 - 演示单链表与双链表
 *
 * 涵盖内容：
 * - 单链表：节点结构、头插、尾插、按值删除、翻转
 * - 双链表：前后指针、O(1) 任意位置操作
 * - 链表与数组的对比
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ==================== 工具函数 ==================== */

/**
 * 打印分隔线
 */
static void print_separator(void)
{
    printf("  ----------------------------------------\n");
}

/* ==================== 单链表实现 ==================== */

/**
 * 单链表节点
 */
typedef struct SNode {
    int data;
    struct SNode *next;
} SNode;

/**
 * 单链表结构
 */
typedef struct {
    SNode *head; /* 头节点（哨兵） */
    int size;    /* 节点计数 */
} SLinkedList;

/**
 * 创建单链表（带头哨兵）
 */
static SLinkedList *slist_create(void)
{
    SLinkedList *list = (SLinkedList *)malloc(sizeof(SLinkedList));
    if (!list) {
        return NULL;
    }
    list->head = (SNode *)malloc(sizeof(SNode));
    if (!list->head) {
        free(list);
        return NULL;
    }
    list->head->next = NULL;
    list->size = 0;
    return list;
}

/**
 * 释放单链表
 */
static void slist_destroy(SLinkedList *list)
{
    if (!list) return;
    SNode *node = list->head;
    while (node) {
        SNode *next = node->next;
        free(node);
        node = next;
    }
    free(list);
}

/**
 * 打印单链表
 */
static void slist_print(const char *label, const SLinkedList *list)
{
    printf("  %s (size=%d): ", label, list->size);
    SNode *node = list->head->next;
    printf("[");
    while (node) {
        printf("%d", node->data);
        if (node->next) printf(" -> ");
        node = node->next;
    }
    printf("]\n");
}

/**
 * 头部插入 O(1)
 */
static int slist_prepend(SLinkedList *list, int value)
{
    SNode *node = (SNode *)malloc(sizeof(SNode));
    if (!node) {
        return -1;
    }
    node->data = value;
    node->next = list->head->next;
    list->head->next = node;
    list->size++;
    return 0;
}

/**
 * 尾部插入 O(n)
 */
static int slist_append(SLinkedList *list, int value)
{
    SNode *node = (SNode *)malloc(sizeof(SNode));
    if (!node) {
        return -1;
    }
    node->data = value;
    node->next = NULL;

    /* 遍历到尾部 */
    SNode *tail = list->head;
    while (tail->next) {
        tail = tail->next;
    }
    tail->next = node;
    list->size++;
    return 0;
}

/**
 * 按值删除 O(n)
 */
static int slist_delete(SLinkedList *list, int value)
{
    SNode *prev = list->head;
    SNode *curr = list->head->next;

    while (curr) {
        if (curr->data == value) {
            prev->next = curr->next;
            free(curr);
            list->size--;
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

/**
 * 单链表翻转（迭代）O(n)
 */
static void slist_reverse(SLinkedList *list)
{
    SNode *prev = NULL;
    SNode *curr = list->head->next;

    while (curr) {
        SNode *next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    /* 重连头节点 */
    list->head->next = prev;
}

/**
 * 单链表演示
 */
static void demo_singly_linked_list(void)
{
    printf("[linked_list] 单链表演示\n");
    print_separator();

    SLinkedList *list = slist_create();
    if (!list) {
        fprintf(stderr, "创建单链表失败\n");
        return;
    }

    printf("  创建单链表（带头哨兵）\n");

    /* 尾部插入 */
    printf("  尾部插入: 1, 2, 3, 4, 5\n");
    slist_append(list, 1);
    slist_append(list, 2);
    slist_append(list, 3);
    slist_append(list, 4);
    slist_append(list, 5);
    slist_print("插入后", list);

    /* 头部插入 */
    printf("  头部插入: 0\n");
    slist_prepend(list, 0);
    slist_print("插入后", list);

    /* 按值删除 */
    printf("  删除值为 3 的节点\n");
    slist_delete(list, 3);
    slist_print("删除后", list);

    /* 翻转链表 */
    printf("  翻转链表\n");
    slist_reverse(list);
    slist_print("翻转后", list);

    slist_destroy(list);
    printf("\n");
}

/* ==================== 双链表实现 ==================== */

/**
 * 双链表节点
 */
typedef struct DNode {
    int data;
    struct DNode *prev;
    struct DNode *next;
} DNode;

/**
 * 双链表结构
 */
typedef struct {
    DNode *head; /* 头节点（哨兵） */
    DNode *tail; /* 尾节点（哨兵） */
    int size;
} DLinkedList;

/**
 * 创建双链表（双向哨兵）
 */
static DLinkedList *dlist_create(void)
{
    DLinkedList *list = (DLinkedList *)malloc(sizeof(DLinkedList));
    if (!list) {
        return NULL;
    }
    /* 创建双向哨兵头尾节点 */
    list->head = (DNode *)malloc(sizeof(DNode));
    list->tail = (DNode *)malloc(sizeof(DNode));
    if (!list->head || !list->tail) {
        free(list->head);
        free(list->tail);
        free(list);
        return NULL;
    }
    list->head->next = list->tail;
    list->head->prev = NULL;
    list->tail->prev = list->head;
    list->tail->next = NULL;
    list->size = 0;
    return list;
}

/**
 * 释放双链表
 */
static void dlist_destroy(DLinkedList *list)
{
    if (!list) return;
    DNode *node = list->head;
    while (node) {
        DNode *next = node->next;
        free(node);
        node = next;
    }
    free(list);
}

/**
 * 打印双链表
 */
static void dlist_print(const char *label, const DLinkedList *list)
{
    printf("  %s (size=%d): ", label, list->size);
    DNode *node = list->head->next;
    printf("[");
    while (node != list->tail) {
        printf("%d", node->data);
        if (node->next != list->tail) printf(" <-> ");
        node = node->next;
    }
    printf("]\n");
}

/**
 * 双链表头部插入 O(1)
 */
static int dlist_prepend(DLinkedList *list, int value)
{
    DNode *node = (DNode *)malloc(sizeof(DNode));
    if (!node) {
        return -1;
    }
    node->data = value;
    node->next = list->head->next;
    node->prev = list->head;
    list->head->next->prev = node;
    list->head->next = node;
    list->size++;
    return 0;
}

/**
 * 双链表尾部插入 O(1)
 */
static int dlist_append(DLinkedList *list, int value)
{
    DNode *node = (DNode *)malloc(sizeof(DNode));
    if (!node) {
        return -1;
    }
    node->data = value;
    node->next = list->tail;
    node->prev = list->tail->prev;
    list->tail->prev->next = node;
    list->tail->prev = node;
    list->size++;
    return 0;
}

/**
 * 双链表演示
 */
static void demo_doubly_linked_list(void)
{
    printf("[linked_list] 双链表演示\n");
    print_separator();

    DLinkedList *list = dlist_create();
    if (!list) {
        fprintf(stderr, "创建双链表失败\n");
        return;
    }

    printf("  创建双链表（双向哨兵）\n");

    /* 双向插入 */
    printf("  头部插入: 1, 3\n");
    dlist_prepend(list, 1);
    dlist_prepend(list, 3);
    dlist_print("插入后", list);

    printf("  尾部插入: 5, 7\n");
    dlist_append(list, 5);
    dlist_append(list, 7);
    dlist_print("插入后", list);

    /* 任意位置操作优势 */
    printf("  双链表优势：可以 O(1) 访问前后节点\n");
    printf("  单链表尾部插入 O(n)，双链表尾部插入 O(1)\n");

    dlist_destroy(list);
    printf("\n");
}

/* ==================== 链表 vs 数组 ==================== */

/**
 * 链表与数组对比演示
 */
static void demo_linked_list_vs_array(void)
{
    printf("[linked_list] 链表 vs 数组对比\n");
    print_separator();

    printf("  | 操作       | 数组   | 单链表 | 双链表 |\n");
    printf("  |------------|--------|--------|--------|\n");
    printf("  | 随机访问   | O(1)   | O(n)   | O(n)   |\n");
    printf("  | 头部插入   | O(n)   | O(1)   | O(1)   |\n");
    printf("  | 尾部插入   | O(1)*  | O(n)   | O(1)   |\n");
    printf("  | 中间插入   | O(n)   | O(n)   | O(n)   |\n");
    printf("  | 按值删除   | O(n)   | O(n)   | O(n)   |\n");
    printf("  | 内存使用   | 低     | 高     | 更高   |\n");
    printf("  | 缓存友好   | 是     | 否     | 否     |\n");
    printf("\n");

    printf("  选择建议：\n");
    printf("  - 需要随机访问 -> 数组\n");
    printf("  - 频繁头部操作 -> 链表\n");
    printf("  - 需要 O(1) 尾部操作 -> 双链表或带尾指针的单链表\n");
    printf("  - 内存敏感 -> 数组\n");
    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 链表 ===\n\n");

    /* 单链表演示 */
    demo_singly_linked_list();

    /* 双链表演示 */
    demo_doubly_linked_list();

    /* 链表 vs 数组 */
    demo_linked_list_vs_array();

    printf("=== PASS ===\n");
    return 0;
}
