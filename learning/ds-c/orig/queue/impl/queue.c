#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ds/queue.h"


link_list_queue_t *linked_list_queue_create(int capacity)
{
    link_list_queue_t *queue = (link_list_queue_t *)malloc(sizeof(link_list_queue_t));
    if (!queue) {
        printf("[linked_list_queue_create] malloc array queue failed.\n");
        return NULL;
    }
    memset(queue, 0, sizeof(link_list_queue_t));

    queue->capacity = capacity;
    queue->size = 0;
    queue->front = NULL;
    queue->rear = NULL;

    return queue;
}

array_queue_t *array_queue_create(int capacity)
{
    array_queue_t *queue = (array_queue_t *)malloc(sizeof(array_queue_t));
    if (!queue) {
        printf("[array_queue_create] malloc array queue failed.\n");
        return NULL;
    }
    memset(queue, 0, sizeof(array_queue_t));

    queue->data = (void **)malloc(sizeof(void *) * capacity);
    if (queue->data == NULL) {
        free(queue);
        printf("[array_queue_create] malloc array data failed.\n");
        return NULL;
    }
    memset(queue->data, 0, sizeof(void *) * capacity);

    queue->capacity = capacity;
    queue->front = 0;
    queue->rear = 0;
    queue->size = 0;

    return queue;
}

queue_t *queue_create(int capacity, queue_type_e queue_type)
{
    queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
    if (!queue) {
        printf("[queue_create] malloc memory failed.\n");
        return NULL;
    }
    memset(queue, 0, sizeof(queue_t));

    if (queue_type == QUEUE_TYPE_ARRAY) {
        queue->data.array_queue = array_queue_create(capacity);
        if (queue->data.array_queue == NULL) {
            free(queue);
            return NULL;
        }
    } else if (queue_type == QUEUE_TYPE_LINKED_LIST) {
        queue->data.linked_list_queue = linked_list_queue_create(capacity);
        if (queue->data.linked_list_queue == NULL) {
            free(queue);
            return NULL;
        }
    } else {
        free(queue);
        printf("[queue_create] Unknown queue type: %d", queue_type);
        return NULL;
    }

    queue->type = queue_type;
    return queue;
}

void linked_list_queue_destroy(link_list_queue_t *queue)
{
    if (!queue) {
        return;
    }

    link_list_node_t *current = queue->front;
    while (current) {
        link_list_node_t *tmp = current;
        current = current->next;
        free(tmp);
    }

    free(queue);
}

void array_queue_destroy(array_queue_t *queue)
{
    if (!queue) {
        return;
    }

    if (queue->data) {
        free(queue->data);
    }

    free(queue);
}

void queue_destroy(queue_t *queue)
{
    if (!queue) {
        return;
    }

    if (queue->type == QUEUE_TYPE_ARRAY) {
        array_queue_destroy(queue->data.array_queue);
    } else if(queue->type == QUEUE_TYPE_LINKED_LIST) {
        linked_list_queue_destroy(queue->data.linked_list_queue);
    } else {
        printf("[queue_destroy] Unknown queue type: %d", queue->type);
        return;
    }

    free(queue);
}

bool is_array_queue_full(array_queue_t *queue)
{
    assert(queue != NULL);

    return (queue->rear + 1) % queue->capacity == queue->front;
}

bool is_linked_list_queue_full(link_list_queue_t *queue)
{
    assert(queue != NULL);

    // 传0代表无限制
    if (queue->capacity == 0) {
        return false;
    }

    // 链表实现的队列如果不限制capacity一般不会出现满的情况
    return queue->size == queue->capacity;
}

bool is_queue_full(queue_t *queue)
{
    assert(queue != NULL);

    if (queue->type == QUEUE_TYPE_ARRAY) {
        return is_array_queue_full(queue->data.array_queue);
    } else if (queue->type == QUEUE_TYPE_LINKED_LIST) {
        return is_linked_list_queue_full(queue->data.linked_list_queue);
    } else {
        printf("[is_queue_full] Unknown queue type: %d", queue->type);
        return false;
    }

    // return false;
}

bool is_array_queue_empty(array_queue_t *queue)
{
    assert(queue != NULL);

    return queue->front == queue->rear;
}

bool is_linked_list_queue_empty(link_list_queue_t *queue)
{
    assert(queue != NULL);

    return queue->front == NULL;
}

bool is_queue_empty(queue_t *queue)
{
    assert(queue != NULL);

    if (queue->type == QUEUE_TYPE_ARRAY) {
        return is_array_queue_empty(queue->data.array_queue);
    } else if (queue->type == QUEUE_TYPE_LINKED_LIST) {
        return is_linked_list_queue_empty(queue->data.linked_list_queue);
    } else {
        printf("[is_queue_empty] Unknown queue type: %d", queue->type);
        return false;
    }

    // return true;
}

int32_t array_queue_enqueue(array_queue_t *queue, void *data)
{
    if (!queue) {
        printf("[array_queue_enqueue] array queue is null.\n");
        return -1;
    }

    if (is_array_queue_full(queue)) {
        printf("[array_queue_enqueue] array queue is full.\n");
        return -1;
    }

    queue->data[queue->rear] = data;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->size++;

    return STATUS_OK;
}

int32_t linked_list_queue_enqueue(link_list_queue_t *queue, void *data)
{
    if (!queue) {
        return -1;
    }

    if (is_linked_list_queue_full(queue)) {
        return -1;
    }

    link_list_node_t *new_node = (link_list_node_t *)malloc(sizeof(link_list_node_t));
    if (!new_node) {
        printf("[linked_list_queue_enqueue] malloc new queue node failed.\n");
        return -1;
    }
    memset(new_node, 0, sizeof(link_list_node_t));
    new_node->data = data;

    if (!queue->rear) {
        queue->front = new_node;
        queue->rear = new_node;

        new_node->next = new_node;
        new_node->pre = new_node;
    } else {
        new_node->pre = queue->rear;
        new_node->next = queue->front;

        queue->rear->next = new_node;
        queue->front->pre = new_node;

        queue->rear = new_node;
    }

    queue->size++;
    return STATUS_OK;
}

int32_t enqueue(queue_t *que, void *data)
{
    if (!que) {
        printf("[enqueue] queue is NULL");
        return -1;
    }

    if (que->type == QUEUE_TYPE_ARRAY) {
        return array_queue_enqueue(que->data.array_queue, data);
    } else if (que->type == QUEUE_TYPE_LINKED_LIST) {
        return linked_list_queue_enqueue(que->data.linked_list_queue, data);
    } else {
        printf("[enqueue] Unknown queue type: %d", que->type);
        return -1;
    }
}

int32_t array_queue_dequeue(array_queue_t *queue)
{
    if (!queue) {
        printf("[array_queue_dequeue] queue is NULL");
        return -1;
    }

    if (queue->size == 0) {
        printf("[array_queue_dequeue] queue is empty");
        return -1;
    }

    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;

    return STATUS_OK;
}

int32_t linked_list_queue_dequeue(link_list_queue_t *queue)
{
    if (!queue) {
        printf("[linked_list_queue_dequeue] queue is NULL");
        return -1;
    }

    if (queue->size == 0 || !queue->front) {
        return STATUS_OK;
    }

    link_list_node_t *tmp = queue->front;
    if (queue->front == queue->rear) { // 队列中只有1个节点
        queue->front = NULL;
        queue->rear = NULL;
    } else {
        queue->front = queue->front->next;
        queue->front->pre = queue->rear;
        queue->rear->next = queue->front;
    }
    queue->size--;
    free(tmp);

    return STATUS_OK;
}

int32_t dequeue(queue_t *que, void *data)
{
    if (!que) {
        printf("[dequeue] queue is NULL");
        return -1;
    }

    if (que->type == QUEUE_TYPE_ARRAY) {
        if (que->data.array_queue->size == 0) {
            printf("[dequeue] queue is empty");
            return -1;
        }
        // 返回队首数据
        if (data) {
            *(void **)data = que->data.array_queue->data[que->data.array_queue->front];
        }
        return array_queue_dequeue(que->data.array_queue);
    } else if (que->type == QUEUE_TYPE_LINKED_LIST) {
        if (que->data.linked_list_queue->size == 0) {
            printf("[dequeue] queue is empty");
            return -1;
        }
        // 返回队首数据
        if (data && que->data.linked_list_queue->front) {
            *(void **)data = que->data.linked_list_queue->front->data;
        }
        return linked_list_queue_dequeue(que->data.linked_list_queue);
    } else {
        printf("[dequeue] Unknown queue type: %d", que->type);
        return -1;
    }
}

void *array_queue_front(array_queue_t *queue)
{
    if (!queue) {
        printf("[array_queue_front] queue is NULL");
        return NULL;
    }

    if (is_array_queue_empty(queue)) {
        printf("[array_queue_front] queue is empty");
        return NULL;
    }

    return queue->data[queue->front];
}

void *linked_list_queue_front(link_list_queue_t *queue)
{
    if (!queue) {
        printf("[linked_list_queue_front] queue is NULL");
        return NULL;
    }

    if (is_linked_list_queue_empty(queue)) {
        printf("[linked_list_queue_front] queue is empty");
        return NULL;
    }

    return queue->front->data;
}

void *front(queue_t *queue)
{
    if (!queue) {
        printf("[front] queue is NULL");
        return NULL;
    }

    if (queue->type == QUEUE_TYPE_ARRAY) {
        return array_queue_front(queue->data.array_queue);
    } else if (queue->type == QUEUE_TYPE_LINKED_LIST) {
        return linked_list_queue_front(queue->data.linked_list_queue);
    } else {
        printf("[front] Unknown queue type: %d", queue->type);
        return NULL;
    }
}

void *array_queue_back(array_queue_t *queue)
{
    if (!queue) {
        printf("[array_queue_back] queue is NULL");
        return NULL;
    }

    if (is_array_queue_empty(queue)) {
        printf("[array_queue_back] queue is empty");
        return NULL;
    }

    return queue->data[(queue->rear - 1) % queue->capacity];
}

void *linked_list_queue_back(link_list_queue_t *queue)
{
    if (!queue) {
        printf("[linked_list_queue_back] queue is NULL");
        return NULL;
    }

    if (is_linked_list_queue_empty(queue)) {
        printf("[linked_list_queue_back] queue is empty");
        return NULL;
    }

    return queue->rear->data;
}

void *back(queue_t *queue)
{
    if (!queue) {
        printf("[back] queue is NULL");
        return NULL;
    }

    if (queue->type == QUEUE_TYPE_ARRAY) {
        return array_queue_back(queue->data.array_queue);
    } else if (queue->type == QUEUE_TYPE_LINKED_LIST) {
        return linked_list_queue_back(queue->data.linked_list_queue);
    } else {
        printf("[back] Unknown queue type: %d", queue->type);
        return NULL;
    }
}

int array_queue_length(array_queue_t *queue)
{
    if (!queue) {
        printf("[array_queue_length] queue is NULL");
        return -1;
    }

    return (int)queue->size;
}

int linked_list_queue_length(link_list_queue_t *queue)
{
    if (!queue) {
        printf("[linked_list_queue_length] queue is NULL");
        return -1;
    }

    // 如果没有记录长度的话只能通过遍历获取
    return queue->size;
}

int queue_length(queue_t *queue)
{
    if (!queue) {
        printf("[queue_length] queue is NULL");
        return -1;
    }

    if (queue->type == QUEUE_TYPE_ARRAY) {
        return array_queue_length(queue->data.array_queue);
    } else if (queue->type == QUEUE_TYPE_LINKED_LIST) {
        return linked_list_queue_length(queue->data.linked_list_queue);
    } else {
        printf("[queue_length] Unknown queue type: %d", queue->type);
        return -1;
    }
}