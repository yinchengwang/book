/*
 * queue.h - 队列类型定义
 */
#ifndef ALGO_DS_QUEUE_H
#define ALGO_DS_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ds/data_type.h"

#ifdef __cplusplus
extern "C" {
#endif

// 队列类型枚举
typedef enum {
    QUEUE_TYPE_ARRAY,
    QUEUE_TYPE_LINKED_LIST
} queue_type_e;

// 数组队列结构
typedef struct array_queue {
    void **data;
    size_t capacity;
    size_t front;
    size_t rear;
    size_t size;
} array_queue_t;

// 链表队列节点（双向链表）
typedef struct link_list_node {
    void *data;
    struct link_list_node *next;
    struct link_list_node *pre;
} link_list_node_t;

// 链表队列结构
typedef struct link_list_queue {
    size_t capacity;
    size_t size;
    link_list_node_t *front;
    link_list_node_t *rear;
} link_list_queue_t;

// 统一队列结构
typedef struct queue {
    queue_type_e type;
    union {
        array_queue_t *array_queue;
        link_list_queue_t *linked_list_queue;
    } data;
} queue_t;

// ============================================================
// 数组队列操作
// ============================================================

array_queue_t *array_queue_create(int capacity);
void array_queue_destroy(array_queue_t *queue);
int32_t array_queue_enqueue(array_queue_t *queue, void *data);
int32_t array_queue_dequeue(array_queue_t *queue);
void *array_queue_front(array_queue_t *queue);
void *array_queue_back(array_queue_t *queue);
bool is_array_queue_full(array_queue_t *queue);
bool is_array_queue_empty(array_queue_t *queue);
int32_t array_queue_length(array_queue_t *queue);

// ============================================================
// 链表队列操作
// ============================================================

link_list_queue_t *linked_list_queue_create(int capacity);
void linked_list_queue_destroy(link_list_queue_t *queue);
int32_t linked_list_queue_enqueue(link_list_queue_t *queue, void *data);
int32_t linked_list_queue_dequeue(link_list_queue_t *queue);
void *linked_list_queue_front(link_list_queue_t *queue);
void *linked_list_queue_back(link_list_queue_t *queue);
bool is_linked_list_queue_full(link_list_queue_t *queue);
bool is_linked_list_queue_empty(link_list_queue_t *queue);
int32_t linked_list_queue_length(link_list_queue_t *queue);

// ============================================================
// 统一队列操作
// ============================================================

queue_t *queue_create(int capacity, queue_type_e queue_type);
void queue_destroy(queue_t *queue);
int32_t enqueue(queue_t *queue, void *data);
int32_t dequeue(queue_t *queue, void *data);
void *front(queue_t *queue);
void *back(queue_t *queue);
bool is_queue_full(queue_t *queue);
bool is_queue_empty(queue_t *queue);
int32_t queue_length(queue_t *queue);

// ============================================================
// 演示函数
// ============================================================

void ds_queue_demo(void);

#ifdef __cplusplus
}
#endif

#endif // ALGO_DS_QUEUE_H
