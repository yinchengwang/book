#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds/queue.h"

// 查看队头元素（不出队）
int32_t circle_peek(const cir_queue_t *cq, void *data)
{
    if (cq == NULL || data == NULL || circle_is_empty(cq)) {
        return -1;
    }
    
    char *source = (char *)cq->item + cq->front * cq->item_size;
    memcpy(data, source, cq->item_size);
    return 0;
}

// 检查队列是否为空
bool circle_is_empty(const cir_queue_t *cq)
{
    return (cq == NULL) ? true : (cq->count == 0);
}

// 检查队列是否已满
bool circle_is_full(const cir_queue_t *cq)
{
    return (cq == NULL) ? true : (cq->count == cq->capacity - 1);
}

// 获取队列中元素数量
int32_t circle_size(const cir_queue_t *cq)
{
    return (cq == NULL) ? 0 : cq->count;
}

// 获取队列容量
int32_t circle_capacity(const cir_queue_t *cq)
{
    return (cq == NULL) ? 0 : (cq->capacity - 1);
}

// 清空队列（但不释放内存）
void circle_clear(cir_queue_t *cq)
{
    if (cq != NULL) {
        cq->front = 0;
        cq->rear = 0;
        cq->count = 0;
    }
}

// 释放队列
void circle_free(cir_queue_t *cq)
{
    if (cq != NULL) {
        if (cq->item != NULL) {
            free(cq->item);
        }
        free(cq);
    }
}

cir_queue_t *initcircle_queue(int32_t capacity, int32_t item_size)
{
    if (capacity > MAX_CIR_QUEUE_SIZE || capacity <= 0 || item_size <= 0) {
        return NULL;
    }

    // 先分配队列结构体内存
    cir_queue_t *q = (cir_queue_t *)malloc(sizeof(cir_queue_t));
    if (q == NULL) {
        return NULL;
    }

    // 再分配数据存储空间
    q->item = malloc(item_size * capacity);
    if (q->item == NULL) {
        free(q);  // 释放已分配的结构体
        return NULL;
    }

    // 所有资源都成功分配后，才设置字段
    q->item_size = item_size;
    q->front = 0;
    q->rear = 0;
    q->capacity = capacity;
    q->rear = 0;

    return q;
}

// 打印队列状态（调试用）
void circle_print_status(const cir_queue_t *cq, const char *name)
{
    if (cq == NULL) {
        printf("%s: NULL\n", name);
        return;
    }

    printf("%s: front=%d, rear=%d, count=%d, capacity=%d, empty=%d, full=%d\n",
           name, cq->front, cq->rear, cq->count, cq->capacity - 1, 
           circle_is_empty(cq), circle_is_full(cq));
}

int32_t circle_enqueue(cir_queue_t *cq, const void *data)
{
    if (cq == NULL || data == NULL) {
        return -1;
    }

    // 检查队列是否已满
    if (circle_is_full(cq)) {
        return -1;
    }

    // 计算目标位置的指针
    char *target = (char *)cq->item + cq->rear * cq->item_size;
    memcpy(target, data, cq->item_size);

    // 更新队尾位置和计数
    cq->rear = (cq->rear + 1) % cq->capacity;
    cq->count++;

    return 0;
}

// 出队操作
int32_t circle_dequeue(cir_queue_t *cq, void *data)
{
    // 参数检查
    if (cq == NULL || data == NULL) {
        return -1;
    }

    // 检查队列是否为空
    if (circle_is_empty(cq)) {
        return -1;
    }

    // 计算源位置的指针
    char *source = (char *)cq->item + cq->front * cq->item_size;
    memcpy(data, source, cq->item_size);
    
    // 更新队头位置和计数
    cq->front = (cq->front + 1) % cq->capacity;
    cq->count--;

    return 0;
}