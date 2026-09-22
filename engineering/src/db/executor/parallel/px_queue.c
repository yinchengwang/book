/* px_queue.c - VectorBlock MPSC 有界队列（Gap#4） */
#include "db/executor/px_queue.h"
#include <pthread.h>
#include <stdlib.h>

struct px_queue {
    VectorBlock **buf;
    int capacity;
    int head;              /* 出队位置 */
    int tail;              /* 入队位置 */
    int count;
    int nproducers;
    int producers_done;
    int aborted;
    pthread_mutex_t mu;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
};

px_queue_t *px_queue_create(int capacity, int nproducers) {
    if (capacity <= 0 || nproducers <= 0) return NULL;
    px_queue_t *q = (px_queue_t *)calloc(1, sizeof(px_queue_t));
    if (!q) return NULL;
    q->buf = (VectorBlock **)calloc((size_t)capacity, sizeof(VectorBlock *));
    if (!q->buf) { free(q); return NULL; }
    q->capacity = capacity;
    q->nproducers = nproducers;
    pthread_mutex_init(&q->mu, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    return q;
}

int px_queue_push(px_queue_t *q, VectorBlock *block) {
    if (!q || !block) return -1;
    pthread_mutex_lock(&q->mu);
    while (!q->aborted && q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mu);   /* 背压：满则阻塞 */
    }
    if (q->aborted) {
        pthread_mutex_unlock(&q->mu);
        return -1;                                  /* 所有权未移交 */
    }
    q->buf[q->tail] = block;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
    return 0;
}

VectorBlock *px_queue_pop(px_queue_t *q) {
    if (!q) return NULL;
    pthread_mutex_lock(&q->mu);
    for (;;) {
        if (q->count > 0) {
            VectorBlock *b = q->buf[q->head];
            q->head = (q->head + 1) % q->capacity;
            q->count--;
            pthread_cond_signal(&q->not_full);
            pthread_mutex_unlock(&q->mu);
            return b;
        }
        if (q->aborted || q->producers_done == q->nproducers) {
            pthread_mutex_unlock(&q->mu);
            return NULL;                            /* abort 或 EOF */
        }
        pthread_cond_wait(&q->not_empty, &q->mu);
    }
}

void px_queue_producer_done(px_queue_t *q) {
    if (!q) return;
    pthread_mutex_lock(&q->mu);
    if (q->producers_done < q->nproducers) q->producers_done++;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
}

void px_queue_abort(px_queue_t *q) {
    if (!q) return;
    pthread_mutex_lock(&q->mu);
    q->aborted = 1;
    pthread_cond_broadcast(&q->not_full);
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
}

int px_queue_is_aborted(const px_queue_t *q) {
    if (!q) return 0;
    /* 调用方均在有锁路径之后读取；此处直接读（int 读写在支持平台上原子） */
    return q->aborted;
}

void px_queue_destroy(px_queue_t *q) {
    if (!q) return;
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % q->capacity;
        vector_block_destroy(q->buf[idx]);          /* 残余块不泄漏 */
    }
    free(q->buf);
    pthread_mutex_destroy(&q->mu);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    free(q);
}
