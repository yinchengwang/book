/*
 * 向量删除标记位图实现
 *
 * 墓碑式删除：标记删除状态，物理删除由 GC 后台线程完成。
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <db/index/vector_index/delete/vector_delete_bitmap.h>

/* 默认初始容量 */
#define DEFAULT_INITIAL_CAPACITY 1024
/* 扩容倍数 */
#define EXPANSION_FACTOR 2

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * 扩容位图
 */
static int32_t bitmap_expand(vector_delete_bitmap_t *bitmap, int32_t new_capacity)
{
    if (new_capacity <= bitmap->capacity) {
        return 0;
    }

    /* 计算新的字节数（向上取整到 8 的倍数） */
    int32_t new_bytes = (new_capacity + 7) / 8;
    int32_t old_bytes = (bitmap->capacity + 7) / 8;

    uint8_t *new_bitmap = (uint8_t *)calloc(new_bytes, sizeof(uint8_t));
    if (!new_bitmap) {
        return -1;
    }

    /* 复制旧数据 */
    if (bitmap->bitmap) {
        memcpy(new_bitmap, bitmap->bitmap, old_bytes);
        free(bitmap->bitmap);
    }

    bitmap->bitmap    = new_bitmap;
    bitmap->capacity  = new_capacity;

    return 0;
}

/* ============================================================================
 * 公共 API 实现
 * ============================================================================ */

vector_delete_bitmap_t *vector_delete_bitmap_create(int32_t initial_capacity)
{
    if (initial_capacity <= 0) {
        initial_capacity = DEFAULT_INITIAL_CAPACITY;
    }

    vector_delete_bitmap_t *bitmap = (vector_delete_bitmap_t *)malloc(
        sizeof(vector_delete_bitmap_t));
    if (!bitmap) {
        return NULL;
    }

    /* 计算初始字节数 */
    int32_t initial_bytes = (initial_capacity + 7) / 8;

    bitmap->bitmap         = (uint8_t *)calloc(initial_bytes, sizeof(uint8_t));
    if (!bitmap->bitmap) {
        free(bitmap);
        return NULL;
    }

    bitmap->max_id          = -1;  /* 尚未有任何向量 */
    bitmap->capacity        = initial_capacity;
    bitmap->deleted_count   = 0;

    if (pthread_mutex_init(&bitmap->lock, NULL) != 0) {
        free(bitmap->bitmap);
        free(bitmap);
        return NULL;
    }

    return bitmap;
}

void vector_delete_bitmap_destroy(vector_delete_bitmap_t *bitmap)
{
    if (!bitmap) {
        return;
    }

    pthread_mutex_destroy(&bitmap->lock);

    if (bitmap->bitmap) {
        free(bitmap->bitmap);
    }

    free(bitmap);
}

int32_t vector_delete_mark(vector_delete_bitmap_t *bitmap, int32_t id)
{
    if (!bitmap || id < 0) {
        return -1;
    }

    pthread_mutex_lock(&bitmap->lock);

    /* 自动扩容 */
    if (id >= bitmap->capacity) {
        int32_t new_capacity = bitmap->capacity * EXPANSION_FACTOR;
        while (new_capacity <= id) {
            new_capacity *= EXPANSION_FACTOR;
        }
        if (bitmap_expand(bitmap, new_capacity) != 0) {
            pthread_mutex_unlock(&bitmap->lock);
            return -1;
        }
    }

    /* 计算位位置 */
    int32_t byte_offset = id / 8;
    int32_t bit_offset  = id % 8;
    uint8_t mask        = (1 << bit_offset);

    /* 如果尚未标记，则计数加一 */
    if ((bitmap->bitmap[byte_offset] & mask) == 0) {
        bitmap->deleted_count++;
    }

    /* 设置删除标记 */
    bitmap->bitmap[byte_offset] |= mask;

    /* 更新最大 ID */
    if (id > bitmap->max_id) {
        bitmap->max_id = id;
    }

    pthread_mutex_unlock(&bitmap->lock);

    return 0;
}

int32_t vector_delete_unmark(vector_delete_bitmap_t *bitmap, int32_t id)
{
    if (!bitmap || id < 0) {
        return -1;
    }

    pthread_mutex_lock(&bitmap->lock);

    /* ID 超出范围，忽略 */
    if (id >= bitmap->capacity) {
        pthread_mutex_unlock(&bitmap->lock);
        return 0;
    }

    /* 计算位位置 */
    int32_t byte_offset = id / 8;
    int32_t bit_offset  = id % 8;
    uint8_t mask        = (1 << bit_offset);

    /* 如果已标记，则计数减一 */
    if ((bitmap->bitmap[byte_offset] & mask) != 0) {
        bitmap->deleted_count--;
    }

    /* 清除删除标记 */
    bitmap->bitmap[byte_offset] &= ~mask;

    pthread_mutex_unlock(&bitmap->lock);

    return 0;
}

bool vector_delete_is_deleted(const vector_delete_bitmap_t *bitmap, int32_t id)
{
    if (!bitmap || id < 0) {
        return false;
    }

    /* 超出范围视为未删除 */
    if (id >= bitmap->capacity) {
        return false;
    }

    int32_t byte_offset = id / 8;
    int32_t bit_offset  = id % 8;

    return (bitmap->bitmap[byte_offset] & (1 << bit_offset)) != 0;
}

int32_t vector_delete_mark_batch(vector_delete_bitmap_t *bitmap, const int32_t *ids, int32_t n)
{
    if (!bitmap || !ids || n <= 0) {
        return 0;
    }

    int32_t success_count = 0;

    for (int32_t i = 0; i < n; i++) {
        if (vector_delete_mark(bitmap, ids[i]) == 0) {
            success_count++;
        }
    }

    return success_count;
}

int32_t vector_delete_unmark_batch(vector_delete_bitmap_t *bitmap, const int32_t *ids, int32_t n)
{
    if (!bitmap || !ids || n <= 0) {
        return 0;
    }

    int32_t success_count = 0;

    for (int32_t i = 0; i < n; i++) {
        if (vector_delete_unmark(bitmap, ids[i]) == 0) {
            success_count++;
        }
    }

    return success_count;
}

int32_t vector_delete_get_stats(const vector_delete_bitmap_t *bitmap, vector_delete_stats_t *stats)
{
    if (!bitmap || !stats) {
        return -1;
    }

    pthread_mutex_lock((pthread_mutex_t *)&bitmap->lock);

    stats->max_id        = bitmap->max_id;
    stats->total_vectors = bitmap->max_id + 1;
    stats->deleted_count = bitmap->deleted_count;
    stats->deleted_ratio = (stats->total_vectors > 0)
                               ? (float)bitmap->deleted_count / stats->total_vectors
                               : 0.0f;

    pthread_mutex_unlock((pthread_mutex_t *)&bitmap->lock);

    return 0;
}

void vector_delete_clear(vector_delete_bitmap_t *bitmap)
{
    if (!bitmap) {
        return;
    }

    pthread_mutex_lock(&bitmap->lock);

    if (bitmap->bitmap) {
        memset(bitmap->bitmap, 0, (bitmap->capacity + 7) / 8);
    }
    bitmap->deleted_count = 0;

    pthread_mutex_unlock(&bitmap->lock);
}

int32_t vector_delete_get_deleted_ids(const vector_delete_bitmap_t *bitmap,
                                       int32_t *out_ids,
                                       int32_t max_out)
{
    if (!bitmap || !out_ids || max_out <= 0) {
        return 0;
    }

    int32_t count = 0;

    pthread_mutex_lock((pthread_mutex_t *)&bitmap->lock);

    /* 遍历所有已使用的 ID 范围 */
    for (int32_t id = 0; id <= bitmap->max_id && count < max_out; id++) {
        if (vector_delete_is_deleted(bitmap, id)) {
            out_ids[count++] = id;
        }
    }

    pthread_mutex_unlock((pthread_mutex_t *)&bitmap->lock);

    return count;
}
