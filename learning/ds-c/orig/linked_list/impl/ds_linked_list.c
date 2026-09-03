/*
 * ds_linked_list.c —— 双向链表演示（学习层独立实现）
 *
 * ============================================================
 * 本文件是学习层独立实现的双向链表 demo，用于：
 * 1. 演示双向链表的核心操作（头尾增删、遍历）
 * 2. 不依赖任何 algo-prod / 工程层类型（ds_list_t vs algo-prod list_t）
 * 3. 作为学习层 ds 公共 API 的一部分（声明在 learning/include/ds/linked_list.h）
 *
 * 历史：
 * - S1 时 linked_list.c 存在，依赖 algo-prod/list/list.h
 * - S5 实施双轨纪律时删除（S6 task 1.4.1）
 * - S7 重新独立实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds/linked_list.h"

/*
 * 链表节点类型：双向链表，每个节点有 prev 和 next
 * 与 algo-prod 的 list_node_t 不同（后者是单链表节点）
 */
typedef struct ds_list_node {
    int value;
    struct ds_list_node *prev;
    struct ds_list_node *next;
} ds_list_node_t;

/*
 * 链表容器：持有头尾指针、节点计数
 */
typedef struct {
    ds_list_node_t *head;
    ds_list_node_t *tail;
    size_t size;
} ds_list_t;

static ds_list_t *ds_list_create(void);
static void ds_list_destroy(ds_list_t *list);
static int ds_list_push_back(ds_list_t *list, int value);
static int ds_list_push_front(ds_list_t *list, int value);
static int ds_list_pop_back(ds_list_t *list, int *value);
static int ds_list_pop_front(ds_list_t *list, int *value);
static void ds_list_print(const ds_list_t *list);

static ds_list_t *ds_list_create(void) {
    ds_list_t *list = (ds_list_t *)calloc(1, sizeof(ds_list_t));
    if (!list) {
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

static void ds_list_destroy(ds_list_t *list) {
    if (!list) {
        return;
    }
    ds_list_node_t *curr = list->head;
    while (curr) {
        ds_list_node_t *next = curr->next;
        free(curr);
        curr = next;
    }
    free(list);
}

static int ds_list_push_back(ds_list_t *list, int value) {
    if (!list) {
        return -1;
    }
    ds_list_node_t *node = (ds_list_node_t *)calloc(1, sizeof(ds_list_node_t));
    if (!node) {
        return -1;
    }
    node->value = value;
    node->prev = list->tail;
    node->next = NULL;
    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->size++;
    return 0;
}

static int ds_list_push_front(ds_list_t *list, int value) {
    if (!list) {
        return -1;
    }
    ds_list_node_t *node = (ds_list_node_t *)calloc(1, sizeof(ds_list_node_t));
    if (!node) {
        return -1;
    }
    node->value = value;
    node->next = list->head;
    node->prev = NULL;
    if (list->head) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }
    list->head = node;
    list->size++;
    return 0;
}

static int ds_list_pop_back(ds_list_t *list, int *value) {
    if (!list || !value || !list->tail) {
        return -1;
    }
    ds_list_node_t *node = list->tail;
    *value = node->value;
    list->tail = node->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }
    free(node);
    list->size--;
    return 0;
}

static int ds_list_pop_front(ds_list_t *list, int *value) {
    if (!list || !value || !list->head) {
        return -1;
    }
    ds_list_node_t *node = list->head;
    *value = node->value;
    list->head = node->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }
    free(node);
    list->size--;
    return 0;
}

static void ds_list_print(const ds_list_t *list) {
    if (!list || !list->head) {
        printf("  (empty)\n");
        return;
    }
    printf("  head -> ");
    for (ds_list_node_t *curr = list->head; curr; curr = curr->next) {
        printf("[%d]%s", curr->value, curr->next ? " <-> " : " <- tail\n");
    }
    printf("  size=%zu\n", list->size);
}

/* size helper，供未来扩展 */
__attribute__((unused))
static size_t ds_list_size(const ds_list_t *list) {
    return list ? list->size : 0;
}

/*
 * 演示函数：展示链表的核心操作
 * - 创建：list = ds_list_create
 * - 头尾插入：push_back / push_front
 * - 头尾删除：pop_back / pop_front
 * - 遍历打印：ds_list_print
 * - 释放：ds_list_destroy
 */
void ds_linked_list_demo(void) {
    printf("\n=== ds_linked_list_demo (双向链表) ===\n");
    ds_list_t *list = ds_list_create();
    if (!list) {
        printf("  创建失败\n");
        return;
    }

    /* 尾插 1, 2, 3 */
    ds_list_push_back(list, 1);
    ds_list_push_back(list, 2);
    ds_list_push_back(list, 3);

    /* 头插 0, -1 */
    ds_list_push_front(list, 0);
    ds_list_push_front(list, -1);

    printf("  插入 -1, 0, 1, 2, 3 后：\n");
    ds_list_print(list);

    /* 头删 */
    int v;
    ds_list_pop_front(list, &v);
    ds_list_pop_front(list, &v);
    printf("  头删两次（值分别为 %d, %d）后：\n", v, v);
    ds_list_print(list);

    /* 尾删 */
    ds_list_pop_back(list, &v);
    printf("  尾删一次（值 %d）后：\n", v);
    ds_list_print(list);

    ds_list_destroy(list);
    printf("=== ds_linked_list_demo 结束 ===\n\n");
}
