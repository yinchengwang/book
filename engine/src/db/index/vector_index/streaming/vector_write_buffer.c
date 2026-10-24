/**
 * @file vector_write_buffer.c
 * @brief 向量写入缓冲区实现 - 环形队列
 */

#include "vector_write_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/**
 * @brief 向量写入缓冲区内部结构
 *
 * 使用环形队列实现，支持无锁push/pop（需要外部同步）。
 * 数据布局：所有向量连续存储，head指向队列头，tail指向队列尾。
 */
struct vector_write_buffer {
    float *data;           /* 向量数据存储 */
    int32_t capacity;      /* 缓冲区容量 */
    int32_t dims;          /* 向量维度 */
    int32_t flush_threshold; /* 自动刷写阈值 */

    int32_t head;          /* 队列头索引（最早进入） */
    int32_t tail;          /* 队列尾索引（最新进入） */
    int32_t size;          /* 当前元素数 */

    /* 统计信息 */
    int64_t total_pushed;  /* 累计推送数 */
    int64_t total_flushed; /* 累计刷新数 */
    int32_t flush_count;   /* 刷写次数 */

    /* 同步原语（预留，简化实现暂不使用） */
    void *mutex;           /* 预留互斥锁 */
};

/* ========================================================================
 * 配置
 * ======================================================================== */

vector_buffer_config_t vector_buffer_config_default(int32_t dims) {
    vector_buffer_config_t config = {
        .capacity = VECTOR_BUFFER_DEFAULT_CAPACITY,
        .flush_threshold = VECTOR_BUFFER_DEFAULT_FLUSH_THRESHOLD,
        .dims = dims,
        .batch_size = VECTOR_BUFFER_DEFAULT_BATCH_SIZE
    };
    return config;
}

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

vector_write_buffer_t *vector_buffer_create(const vector_buffer_config_t *config) {
    vector_buffer_config_t default_cfg = vector_buffer_config_default(0);

    if (config != NULL) {
        /* 使用用户配置 */
    } else {
        /* 使用默认配置，需要外部设置dims */
        return NULL;
    }

    int32_t capacity = config ? config->capacity : default_cfg.capacity;
    int32_t dims = config ? config->dims : default_cfg.dims;
    int32_t flush_threshold = config ? config->flush_threshold : default_cfg.flush_threshold;

    if (capacity <= 0 || dims <= 0) {
        return NULL;
    }

    vector_write_buffer_t *buf = (vector_write_buffer_t *)calloc(1, sizeof(vector_write_buffer_t));
    if (buf == NULL) {
        return NULL;
    }

    /* 分配数据存储 */
    buf->data = (float *)malloc((size_t)capacity * dims * sizeof(float));
    if (buf->data == NULL) {
        free(buf);
        return NULL;
    }

    buf->capacity = capacity;
    buf->dims = dims;
    buf->flush_threshold = flush_threshold > 0 ? flush_threshold : capacity / 10;
    buf->head = 0;
    buf->tail = 0;
    buf->size = 0;
    buf->total_pushed = 0;
    buf->total_flushed = 0;
    buf->flush_count = 0;
    buf->mutex = NULL;

    return buf;
}

void vector_buffer_destroy(vector_write_buffer_t *buf) {
    if (buf == NULL) {
        return;
    }

    if (buf->data != NULL) {
        free(buf->data);
    }

    /* 释放互斥锁（如果有） */
    /* TODO: 根据实际同步机制实现 */

    free(buf);
}

/* ========================================================================
 * 基本操作
 * ======================================================================== */

/**
 * @brief 计算环形缓冲区索引
 */
static inline int32_t ring_index(const vector_write_buffer_t *buf, int32_t i) {
    int32_t idx = i % buf->capacity;
    if (idx < 0) {
        idx += buf->capacity;
    }
    return idx;
}

int32_t vector_buffer_push(vector_write_buffer_t *buf, const float *vector) {
    if (buf == NULL || vector == NULL) {
        return -1;
    }

    if (buf->size >= buf->capacity) {
        return -1;  /* 缓冲区已满 */
    }

    /* 计算写入位置 */
    int32_t pos = ring_index(buf, buf->tail);

    /* 复制向量数据 */
    float *dest = buf->data + (size_t)pos * buf->dims;
    memcpy(dest, vector, (size_t)buf->dims * sizeof(float));

    /* 更新尾指针和大小 */
    buf->tail++;
    buf->size++;
    buf->total_pushed++;

    return 0;
}

int32_t vector_buffer_push_batch(vector_write_buffer_t *buf, const float *vectors, int32_t n) {
    if (buf == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    int32_t pushed = 0;
    for (int32_t i = 0; i < n; i++) {
        if (buf->size >= buf->capacity) {
            break;  /* 缓冲区已满 */
        }

        const float *vec = vectors + (size_t)i * buf->dims;
        int32_t pos = ring_index(buf, buf->tail);

        float *dest = buf->data + (size_t)pos * buf->dims;
        memcpy(dest, vec, (size_t)buf->dims * sizeof(float));

        buf->tail++;
        buf->size++;
        buf->total_pushed++;
        pushed++;
    }

    return pushed;
}

int32_t vector_buffer_pop(vector_write_buffer_t *buf, float *vectors, int32_t n) {
    if (buf == NULL || vectors == NULL || n <= 0) {
        return 0;
    }

    int32_t popped = 0;
    int32_t to_pop = (n < buf->size) ? n : buf->size;

    for (int32_t i = 0; i < to_pop; i++) {
        int32_t pos = ring_index(buf, buf->head);

        float *src = buf->data + (size_t)pos * buf->dims;
        float *dest = vectors + (size_t)i * buf->dims;
        memcpy(dest, src, (size_t)buf->dims * sizeof(float));

        buf->head++;
        buf->size--;
        popped++;
    }

    return popped;
}

int32_t vector_buffer_flush(vector_write_buffer_t *buf, float *vectors, int32_t max_n) {
    if (buf == NULL || vectors == NULL) {
        return 0;
    }

    int32_t flushed = vector_buffer_pop(buf, vectors, max_n);
    if (flushed > 0) {
        buf->total_flushed += flushed;
        buf->flush_count++;
    }

    return flushed;
}

int32_t vector_buffer_size(const vector_write_buffer_t *buf) {
    return buf ? buf->size : 0;
}

int32_t vector_buffer_capacity(const vector_write_buffer_t *buf) {
    return buf ? buf->capacity : 0;
}

bool vector_buffer_empty(const vector_write_buffer_t *buf) {
    return buf == NULL || buf->size == 0;
}

bool vector_buffer_full(const vector_write_buffer_t *buf) {
    return buf == NULL || buf->size >= buf->capacity;
}

int32_t vector_buffer_set_flush_threshold(vector_write_buffer_t *buf, int32_t threshold) {
    if (buf == NULL || threshold <= 0) {
        return -1;
    }

    buf->flush_threshold = threshold;
    return 0;
}

int32_t vector_buffer_get_flush_threshold(const vector_write_buffer_t *buf) {
    return buf ? buf->flush_threshold : 0;
}

bool vector_buffer_need_flush(const vector_write_buffer_t *buf) {
    if (buf == NULL) {
        return false;
    }
    return buf->size >= buf->flush_threshold;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

void vector_buffer_get_stats(const vector_write_buffer_t *buf, vector_buffer_stats_t *stats) {
    if (buf == NULL || stats == NULL) {
        return;
    }

    stats->current_size = buf->size;
    stats->capacity = buf->capacity;
    stats->flush_threshold = buf->flush_threshold;
    stats->total_pushed = buf->total_pushed;
    stats->total_flushed = buf->total_flushed;
    stats->flush_count = buf->flush_count;
}
