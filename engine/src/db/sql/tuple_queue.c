/**
 * @file tuple_queue.c
 * @brief 线程安全元组队列实现
 */

#include "db/sql/tuple_queue.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>

/* Windows 平台使用 CRITICAL_SECTION 和 CONDITION_VARIABLE */
typedef CRITICAL_SECTION pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

static void pthread_mutex_init(pthread_mutex_t *mutex, void *attr) {
    (void)attr;
    InitializeCriticalSection(mutex);
}

static void pthread_mutex_destroy(pthread_mutex_t *mutex) {
    DeleteCriticalSection(mutex);
}

static void pthread_mutex_lock(pthread_mutex_t *mutex) {
    EnterCriticalSection(mutex);
}

static void pthread_mutex_unlock(pthread_mutex_t *mutex) {
    LeaveCriticalSection(mutex);
}

static void pthread_cond_init(pthread_cond_t *cond, void *attr) {
    (void)attr;
    InitializeConditionVariable(cond);
}

static void pthread_cond_destroy(pthread_cond_t *cond) {
    (void)cond;
    /* Windows CONDITION_VARIABLE 无需销毁 */
}

static void pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    SleepConditionVariableCS(cond, mutex, INFINITE);
}

static void pthread_cond_signal(pthread_cond_t *cond) {
    WakeConditionVariable(cond);
}

static void pthread_cond_broadcast(pthread_cond_t *cond) {
    WakeAllConditionVariable(cond);
}

#else
#include <pthread.h>
#endif

/* 默认队列容量 */
#define DEFAULT_QUEUE_CAPACITY 1024

/* ========================================================================
 * TupleQueue API 实现
 * ======================================================================== */

TupleQueue *TupleQueueCreate(int capacity) {
    TupleQueue *queue;

    /* 使用默认容量 */
    if (capacity <= 0) {
        capacity = DEFAULT_QUEUE_CAPACITY;
    }

    queue = (TupleQueue *)calloc(1, sizeof(TupleQueue));
    if (!queue) {
        return NULL;
    }

    /* 分配锁和条件变量 */
    queue->lock = calloc(1, sizeof(pthread_mutex_t));
    queue->not_empty = calloc(1, sizeof(pthread_cond_t));
    queue->not_full = calloc(1, sizeof(pthread_cond_t));

    if (!queue->lock || !queue->not_empty || !queue->not_full) {
        free(queue->lock);
        free(queue->not_empty);
        free(queue->not_full);
        free(queue);
        return NULL;
    }

    /* 初始化锁和条件变量 */
    pthread_mutex_init((pthread_mutex_t *)queue->lock, NULL);
    pthread_cond_init((pthread_cond_t *)queue->not_empty, NULL);
    pthread_cond_init((pthread_cond_t *)queue->not_full, NULL);

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->capacity = capacity;
    queue->closed = false;

    return queue;
}

void TupleQueueDestroy(TupleQueue *queue) {
    TupleQueueNode *node, *next;

    if (!queue) {
        return;
    }

    /* 销毁锁前先清空队列 */
    pthread_mutex_lock((pthread_mutex_t *)queue->lock);

    /* 释放所有节点 */
    node = queue->head;
    while (node) {
        next = node->next;
        free(node);
        node = next;
    }

    pthread_mutex_unlock((pthread_mutex_t *)queue->lock);

    /* 销毁锁和条件变量 */
    pthread_mutex_destroy((pthread_mutex_t *)queue->lock);
    pthread_cond_destroy((pthread_cond_t *)queue->not_empty);
    pthread_cond_destroy((pthread_cond_t *)queue->not_full);

    free(queue->lock);
    free(queue->not_empty);
    free(queue->not_full);
    free(queue);
}

int TupleQueueSend(TupleQueue *queue, void *slot) {
    TupleQueueNode *node;

    if (!queue || !slot) {
        return -1;
    }

    pthread_mutex_lock((pthread_mutex_t *)queue->lock);

    /* 等待队列有空间 */
    while (queue->size >= queue->capacity && !queue->closed) {
        pthread_cond_wait((pthread_cond_t *)queue->not_full,
                          (pthread_mutex_t *)queue->lock);
    }

    /* 队列已关闭 */
    if (queue->closed) {
        pthread_mutex_unlock((pthread_mutex_t *)queue->lock);
        return -1;
    }

    /* 创建新节点 */
    node = (TupleQueueNode *)calloc(1, sizeof(TupleQueueNode));
    if (!node) {
        pthread_mutex_unlock((pthread_mutex_t *)queue->lock);
        return -1;
    }

    node->slot = slot;
    node->next = NULL;

    /* 入队 */
    if (queue->tail) {
        queue->tail->next = node;
        queue->tail = node;
    } else {
        queue->head = queue->tail = node;
    }
    queue->size++;

    /* 通知消费者 */
    pthread_cond_signal((pthread_cond_t *)queue->not_empty);

    pthread_mutex_unlock((pthread_mutex_t *)queue->lock);

    return 0;
}

void *TupleQueueReceive(TupleQueue *queue) {
    TupleQueueNode *node;
    void *slot;

    if (!queue) {
        return NULL;
    }

    pthread_mutex_lock((pthread_mutex_t *)queue->lock);

    /* 等待队列有元素 */
    while (queue->size == 0 && !queue->closed) {
        pthread_cond_wait((pthread_cond_t *)queue->not_empty,
                          (pthread_mutex_t *)queue->lock);
    }

    /* 队列为空且已关闭 */
    if (queue->size == 0) {
        pthread_mutex_unlock((pthread_mutex_t *)queue->lock);
        return NULL;
    }

    /* 出队 */
    node = queue->head;
    slot = node->slot;
    queue->head = node->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;

    free(node);

    /* 通知生产者 */
    pthread_cond_signal((pthread_cond_t *)queue->not_full);

    pthread_mutex_unlock((pthread_mutex_t *)queue->lock);

    return slot;
}

void TupleQueueClose(TupleQueue *queue) {
    if (!queue) {
        return;
    }

    pthread_mutex_lock((pthread_mutex_t *)queue->lock);
    queue->closed = true;
    /* 唤醒所有等待的线程 */
    pthread_cond_broadcast((pthread_cond_t *)queue->not_empty);
    pthread_cond_broadcast((pthread_cond_t *)queue->not_full);
    pthread_mutex_unlock((pthread_mutex_t *)queue->lock);
}

bool TupleQueueIsClosed(TupleQueue *queue) {
    bool closed;

    if (!queue) {
        return true;
    }

    pthread_mutex_lock((pthread_mutex_t *)queue->lock);
    closed = queue->closed;
    pthread_mutex_unlock((pthread_mutex_t *)queue->lock);

    return closed;
}

int TupleQueueSize(TupleQueue *queue) {
    int size;

    if (!queue) {
        return 0;
    }

    pthread_mutex_lock((pthread_mutex_t *)queue->lock);
    size = queue->size;
    pthread_mutex_unlock((pthread_mutex_t *)queue->lock);

    return size;
}