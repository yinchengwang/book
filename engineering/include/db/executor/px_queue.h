/**
 * @file px_queue.h
 * @brief Gap#4 并行执行——VectorBlock 多生产者单消费者有界队列
 *
 * 所有权：push 成功即移交块所有权；push 失败（abort）所有权不移交。
 * pop 返回的块由调用方负责 vector_block_destroy。
 * 关闭语义：EOF = 全部生产者 producer_done 且队列空；
 *           abort = 错误/取消，pop 一律返回 NULL 且 is_aborted=1。
 * destroy 负责销毁队列中残余未消费块（无泄漏）。
 */
#ifndef DB_EXECUTOR_PX_QUEUE_H
#define DB_EXECUTOR_PX_QUEUE_H

#include "db/core/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct px_queue px_queue_t;

px_queue_t  *px_queue_create(int capacity, int nproducers);
int          px_queue_push(px_queue_t *q, VectorBlock *block);
VectorBlock *px_queue_pop(px_queue_t *q);
void         px_queue_producer_done(px_queue_t *q);
void         px_queue_abort(px_queue_t *q);
int          px_queue_is_aborted(const px_queue_t *q);
void         px_queue_destroy(px_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_PX_QUEUE_H */
