/**
 * @file tuple_queue.h
 * @brief 线程安全元组队列
 *
 * 实现生产者-消费者模式的线程安全队列：
 *   - 支持 TupleQueueSend/TupleQueueReceive 操作
 *   - 空队列时消费者阻塞等待
 *   - 满队列时生产者阻塞等待
 *   - 支持 close 操作通知消费者结束
 *
 * 用于 Gather 节点从多个并行 worker 收集结果元组。
 */

#ifndef DB_SQL_TUPLE_QUEUE_H
#define DB_SQL_TUPLE_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * TupleQueue 数据结构
 * ======================================================================== */

/**
 * @brief 元组队列节点
 */
typedef struct TupleQueueNode {
    void                   *slot;      /**< 元组槽指针 */
    struct TupleQueueNode  *next;     /**< 下一个节点 */
} TupleQueueNode;

/**
 * @brief 线程安全元组队列
 */
typedef struct TupleQueue {
    void               *lock;          /**< 互斥锁 (pthread_mutex_t) */
    void               *not_empty;     /**< 非空条件变量 (pthread_cond_t) */
    void               *not_full;      /**< 非满条件变量 (pthread_cond_t) */
    TupleQueueNode     *head;          /**< 队列头 */
    TupleQueueNode     *tail;          /**< 队列尾 */
    int                size;           /**< 当前元素数量 */
    int                capacity;       /**< 队列容量 */
    bool               closed;         /**< 队列是否已关闭 */
} TupleQueue;

/* ========================================================================
 * TupleQueue API
 * ======================================================================== */

/**
 * @brief 创建元组队列
 *
 * @param capacity 队列容量（0 表示使用默认值 1024）
 * @return 新创建的 TupleQueue；失败返回 NULL
 */
TupleQueue *TupleQueueCreate(int capacity);

/**
 * @brief 销毁元组队列
 *
 * 释放队列中所有元素和队列本身。
 *
 * @param queue TupleQueue（可为 NULL）
 */
void TupleQueueDestroy(TupleQueue *queue);

/**
 * @brief 发送元组到队列
 *
 * 生产者操作：将元组槽入队。
 * 如果队列已满，阻塞等待直到有空间或队列被关闭。
 *
 * @param queue TupleQueue
 * @param slot  元组槽指针
 *
 * @return 成功返回 0；队列已关闭返回 -1
 */
int TupleQueueSend(TupleQueue *queue, void *slot);

/**
 * @brief 从队列接收元组
 *
 * 消费者操作：从队列取出元组槽。
 * 如果队列为空，阻塞等待直到有元素或队列被关闭。
 *
 * @param queue TupleQueue
 *
 * @return 元组槽指针；队列已关闭且为空返回 NULL
 */
void *TupleQueueReceive(TupleQueue *queue);

/**
 * @brief 关闭元组队列
 *
 * 通知所有等待的消费者队列已结束。
 * 等待中的 Send 操作会返回 -1。
 * 等待中的 Receive 操作会在队列清空后返回 NULL。
 *
 * @param queue TupleQueue
 */
void TupleQueueClose(TupleQueue *queue);

/**
 * @brief 检查队列是否已关闭
 *
 * @param queue TupleQueue
 * @return true 表示已关闭
 */
bool TupleQueueIsClosed(TupleQueue *queue);

/**
 * @brief 获取队列当前大小
 *
 * @param queue TupleQueue
 * @return 队列中元素数量
 */
int TupleQueueSize(TupleQueue *queue);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_TUPLE_QUEUE_H */
