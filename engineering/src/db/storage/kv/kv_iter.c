/**
 * @file kv_iter.c
 * @brief KV 扫描迭代器实现
 */

#include "db/kv.h"
#include "db/page.h"
#include "db/buffer.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/* 前向声明 */
struct kv_s;

/** KV 记录头部 */
typedef struct kv_record_s {
    uint32_t key_len;
    uint32_t value_len;
} kv_record_t;

/** KV 扫描迭代器 */
struct kv_iter_s {
    buffer_pool_t *pool;          /**< 缓存池 */
    page_id_t     data_page_id;   /**< 数据页 ID */

    /* 扫描范围 */
    void         *start_key;
    size_t        start_len;
    void         *end_key;
    size_t        end_len;

    /* 当前状态 */
    page_t       *page;           /**< 当前页面 */
    uint16_t      current_offset; /**< 当前记录偏移 */
    bool          started;        /**< 是否已开始扫描 */

    /* 当前键值对（用户获取时复制一份） */
    void         *key;
    size_t        key_len;
    void         *value;
    size_t        value_len;
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 比较键（支持 NULL 表示无穷小/无穷大） */
static int kv_key_compare(const void *k1, size_t l1, const void *k2, size_t l2) {
    if (k1 == NULL) return (k2 == NULL) ? 0 : -1;
    if (k2 == NULL) return 1;

    size_t min_len = (l1 < l2) ? l1 : l2;
    int cmp = memcmp(k1, k2, min_len);
    if (cmp != 0) return cmp;
    return (int)(l1 - l2);
}

/** 检查键是否在范围内 */
static int kv_key_in_range(const void *key, size_t key_len,
                           const void *start, size_t start_len,
                           const void *end, size_t end_len) {
    /* 检查是否 >= start */
    if (start && kv_key_compare(key, key_len, start, start_len) < 0) {
        return 0;
    }

    /* 检查是否 <= end */
    if (end && kv_key_compare(key, key_len, end, end_len) > 0) {
        return 0;
    }

    return 1;
}

/* ============================================================
 * API 实现
 * ============================================================ */

kv_iter_t *kv_scan(kv_t *db,
                   const void *start_key, size_t start_len,
                   const void *end_key, size_t end_len) {
    if (!db) return NULL;

    kv_iter_t *iter = (kv_iter_t *)calloc(1, sizeof(kv_iter_t));
    if (!iter) return NULL;

    iter->pool = db->pool;
    iter->data_page_id = 1;  /* 第一个数据页 */

    /* 复制范围边界 */
    if (start_key && start_len > 0) {
        iter->start_key = malloc(start_len);
        if (iter->start_key) {
            memcpy(iter->start_key, start_key, start_len);
            iter->start_len = start_len;
        }
    }

    if (end_key && end_len > 0) {
        iter->end_key = malloc(end_len);
        if (iter->end_key) {
            memcpy(iter->end_key, end_key, end_len);
            iter->end_len = end_len;
        }
    }

    iter->page = NULL;
    iter->current_offset = 0;
    iter->started = false;

    return iter;
}

kv_result_t kv_iter_next(kv_iter_t *iter) {
    if (!iter) return KV_ERROR;

    /* 获取数据页 */
    if (!iter->page) {
        iter->page = buffer_get_page(iter->pool, iter->data_page_id);
        if (!iter->page) {
            return KV_NOT_FOUND;
        }
        iter->current_offset = PAGE_HEADER_SIZE;
    }

    /* 遍历页面中的记录 */
    while (iter->current_offset < iter->page->header.free_space_offset) {
        kv_record_t *rec = (kv_record_t *)(iter->page->data + iter->current_offset);

        /* 检查数据是否越界 */
        size_t rec_size = sizeof(kv_record_t) + rec->key_len + rec->value_len;
        if (iter->current_offset + rec_size > PAGE_DATA_SIZE) {
            break;
        }

        uint8_t *key = iter->page->data + iter->current_offset + sizeof(kv_record_t);
        uint32_t key_len = rec->key_len;
        uint8_t *value = key + key_len;
        uint32_t value_len = rec->value_len;

        /* 更新偏移到下一条记录 */
        iter->current_offset += (uint16_t)rec_size;

        /* 跳过不在范围内的键 */
        if (!kv_key_in_range(key, key_len,
                             iter->start_key, iter->start_len,
                             iter->end_key, iter->end_len)) {
            continue;
        }

        /* 找到有效记录，复制给用户 */
        if (iter->key) free(iter->key);
        if (iter->value) free(iter->value);

        iter->key = malloc(key_len);
        if (iter->key) {
            memcpy(iter->key, key, key_len);
            iter->key_len = key_len;
        }

        iter->value = malloc(value_len);
        if (iter->value) {
            memcpy(iter->value, value, value_len);
            iter->value_len = value_len;
        }

        iter->started = true;
        return KV_OK;
    }

    /* 页面遍历完毕 */
    return KV_NOT_FOUND;
}

const void *kv_iter_key(kv_iter_t *iter) {
    if (!iter || !iter->started) return NULL;
    return iter->key;
}

size_t kv_iter_key_len(kv_iter_t *iter) {
    if (!iter || !iter->started) return 0;
    return iter->key_len;
}

const void *kv_iter_value(kv_iter_t *iter) {
    if (!iter || !iter->started) return NULL;
    return iter->value;
}

size_t kv_iter_value_len(kv_iter_t *iter) {
    if (!iter || !iter->started) return 0;
    return iter->value_len;
}

void kv_iter_free(kv_iter_t *iter) {
    if (!iter) return;

    if (iter->page) {
        buffer_unpin_page(iter->pool, iter->data_page_id);
    }

    if (iter->start_key) free(iter->start_key);
    if (iter->end_key) free(iter->end_key);
    if (iter->key) free(iter->key);
    if (iter->value) free(iter->value);

    free(iter);
}
