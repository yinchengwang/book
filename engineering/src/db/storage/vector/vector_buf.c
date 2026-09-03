/**
 * @file vector_buf.c
 * @brief 向量索引与 Buffer Pool 集成实现
 *
 * 提供向量页面的缓存管理功能。
 */
#define _POSIX_C_SOURCE 200809L

#include <db/storage/vector/vector_buf.h>
#include <db/storage/vector/vector_persist.h>
#include <db/log.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 向量 Buffer 条目 */
typedef struct vector_buf_entry_s {
    uint32_t            segment_id;   /**< 段 ID */
    uint32_t            page_no;      /**< 页面号 */
    uint8_t             page_type;    /**< 页面类型 */
    bool                is_dirty;     /**< 是否脏页 */
    bool                is_pinned;    /**< 是否被 pin */
    uint32_t            ref_count;    /**< 引用计数 */
    uint32_t            last_access;  /**< 上次访问时间 */
    void               *data;         /**< 页面数据 */
    struct vector_buf_entry_s *next; /**< 链表下一项 */
} vector_buf_entry_t;

/** 向量 Buffer 池 */
struct vector_buf_pool_s {
    vector_buf_config_t config;       /**< 配置 */
    vector_buf_entry_t **entries;     /**< Hash 表 */
    uint32_t            entry_count;  /**< 条目数 */
    uint32_t            hash_size;    /**< Hash 表大小 */
    vector_buf_stats_t  stats;        /**< 统计信息 */
    pthread_mutex_t     mutex;        /**< 互斥锁 */
    bool                initialized;  /**< 是否初始化 */
};

/* Hash 表大小 */
#define VECTOR_BUF_HASH_SIZE 1024

/** 计算 Hash 值 */
static uint32_t vector_buf_hash(uint32_t segment_id, uint32_t page_no) {
    return (segment_id * 31 + page_no) % VECTOR_BUF_HASH_SIZE;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

/**
 * @brief 创建默认配置
 */
vector_buf_config_t vector_buf_default_config(void) {
    vector_buf_config_t config = {
        .pool_size = VECTOR_BUF_DEFAULT_SIZE,
        .page_size = VECTOR_BUF_PAGE_SIZE,
        .prefetch_enabled = true,
        .prefetch_distance = 4
    };
    return config;
}

/**
 * @brief 创建向量 Buffer 池
 */
void *vector_buf_create(const vector_buf_config_t *config) {
    vector_buf_pool_t *pool = (vector_buf_pool_t *)calloc(1, sizeof(vector_buf_pool_t));
    if (!pool) {
        LOG_ERROR("分配 Buffer 池失败");
        return NULL;
    }

    /* 使用默认配置 */
    if (config) {
        pool->config = *config;
    } else {
        pool->config = vector_buf_default_config();
    }

    /* 分配 Hash 表 */
    pool->hash_size = VECTOR_BUF_HASH_SIZE;
    pool->entries = (vector_buf_entry_t **)calloc(pool->hash_size, sizeof(vector_buf_entry_t *));
    if (!pool->entries) {
        LOG_ERROR("分配 Hash 表失败");
        free(pool);
        return NULL;
    }

    /* 初始化统计 */
    memset(&pool->stats, 0, sizeof(pool->stats));
    pool->stats.max_size = pool->config.pool_size;

    /* 初始化互斥锁 */
    pthread_mutex_init(&pool->mutex, NULL);
    pool->initialized = true;

    LOG_INFO("创建向量 Buffer 池: size=%u, page_size=%u",
             pool->config.pool_size, pool->config.page_size);
    return pool;
}

/**
 * @brief 销毁向量 Buffer 池
 */
void vector_buf_destroy(void *pool_ptr) {
    if (!pool_ptr) return;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);

    /* 刷新所有脏页 */
    vector_buf_flush(pool, 0);

    /* 释放所有条目 */
    for (uint32_t i = 0; i < pool->hash_size; i++) {
        vector_buf_entry_t *entry = pool->entries[i];
        while (entry) {
            vector_buf_entry_t *next = entry->next;
            if (entry->data) {
                free(entry->data);
            }
            free(entry);
            entry = next;
        }
    }

    free(pool->entries);
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);

    free(pool);
    LOG_INFO("销毁向量 Buffer 池");
}

/**
 * @brief 获取向量页面
 */
vector_buf_t *vector_buf_get(void *pool_ptr, uint32_t segment_id,
                             uint32_t page_no, uint8_t page_type) {
    if (!pool_ptr) return NULL;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);

    /* 查找现有条目 */
    uint32_t hash = vector_buf_hash(segment_id, page_no);
    vector_buf_entry_t *entry = pool->entries[hash];

    while (entry) {
        if (entry->segment_id == segment_id &&
            entry->page_no == page_no &&
            entry->page_type == page_type) {
            /* 缓存命中 */
            pool->stats.hits++;
            entry->ref_count++;
            entry->last_access = (uint32_t)time(NULL);
            pthread_mutex_unlock(&pool->mutex);

            vector_buf_t *buf = (vector_buf_t *)calloc(1, sizeof(vector_buf_t));
            if (buf) {
                buf->desc.buf_id = (int32_t)hash;
                buf->desc.segment_id = segment_id;
                buf->desc.page_no = page_no;
                buf->desc.page_type = page_type;
                buf->desc.is_dirty = entry->is_dirty;
                buf->desc.is_pinned = true;
                buf->desc.ref_count = entry->ref_count;
                buf->data = entry->data;
            }
            return buf;
        }
        entry = entry->next;
    }

    /* 缓存未命中 */
    pool->stats.misses++;

    /* 检查是否需要淘汰 */
    if (pool->entry_count >= pool->config.pool_size) {
        /* 驱逐最少使用的条目 */
        vector_buf_evict_lru(pool_ptr);
    }

    /* 分配新条目 */
    entry = (vector_buf_entry_t *)calloc(1, sizeof(vector_buf_entry_t));
    if (!entry) {
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }

    entry->segment_id = segment_id;
    entry->page_no = page_no;
    entry->page_type = page_type;
    entry->is_dirty = false;
    entry->is_pinned = true;
    entry->ref_count = 1;
    entry->last_access = (uint32_t)time(NULL);
    entry->data = calloc(1, pool->config.page_size);

    if (!entry->data) {
        free(entry);
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }

    /* 添加到 Hash 表 */
    entry->next = pool->entries[hash];
    pool->entries[hash] = entry;
    pool->entry_count++;

    pthread_mutex_unlock(&pool->mutex);

    vector_buf_t *buf = (vector_buf_t *)calloc(1, sizeof(vector_buf_t));
    if (buf) {
        buf->desc.buf_id = (int32_t)hash;
        buf->desc.segment_id = segment_id;
        buf->desc.page_no = page_no;
        buf->desc.page_type = page_type;
        buf->desc.is_dirty = false;
        buf->desc.is_pinned = true;
        buf->desc.ref_count = 1;
        buf->data = entry->data;
    }

    LOG_DEBUG("缓存未命中: seg=%u, page=%u", segment_id, page_no);
    return buf;
}

/**
 * @brief 释放向量页面
 */
void vector_buf_release(vector_buf_t *buf) {
    if (!buf) return;
    /* 更新引用计数 */
    /* 实际释放逻辑在池销毁时处理 */
    free(buf);
}

/**
 * @brief 标记页面为脏
 */
void vector_buf_mark_dirty(vector_buf_t *buf) {
    if (!buf) return;
    buf->desc.is_dirty = true;
}

/**
 * @brief 刷新脏页到磁盘
 */
int vector_buf_flush(void *pool_ptr, uint32_t segment_id) {
    if (!pool_ptr) return 0;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);

    int flushed = 0;

    for (uint32_t i = 0; i < pool->hash_size; i++) {
        vector_buf_entry_t *entry = pool->entries[i];
        while (entry) {
            if (entry->is_dirty) {
                if (segment_id == 0 || entry->segment_id == segment_id) {
                    /* TODO: 实际写入磁盘 */
                    entry->is_dirty = false;
                    flushed++;
                    pool->stats.writes++;
                }
            }
            entry = entry->next;
        }
    }

    pthread_mutex_unlock(&pool->mutex);

    if (flushed > 0) {
        LOG_INFO("刷新脏页: count=%d, segment=%u", flushed, segment_id);
    }
    return flushed;
}

/**
 * @brief 预取向量页面
 */
void vector_buf_prefetch(void *pool_ptr, uint32_t segment_id,
                         uint32_t start_page, uint32_t count) {
    if (!pool_ptr || !((vector_buf_pool_t *)pool_ptr)->config.prefetch_enabled) {
        return;
    }

    /* 异步预取 (简化实现) */
    for (uint32_t i = 0; i < count && i < ((vector_buf_pool_t *)pool_ptr)->config.prefetch_distance; i++) {
        vector_buf_get(pool_ptr, segment_id, start_page + i, VECTOR_PAGE_TYPE_DATA);
    }
}

/* ========================================================================
 * 统计和监控
 * ======================================================================== */

/**
 * @brief 获取缓存统计
 */
void vector_buf_get_stats(void *pool_ptr, vector_buf_stats_t *stats) {
    if (!pool_ptr || !stats) return;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);
    *stats = pool->stats;
    stats->current_size = pool->entry_count;
    pthread_mutex_unlock(&pool->mutex);
}

/**
 * @brief 重置统计
 */
void vector_buf_reset_stats(void *pool_ptr) {
    if (!pool_ptr) return;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);
    memset(&pool->stats, 0, sizeof(pool->stats));
    pool->stats.max_size = pool->config.pool_size;
    pthread_mutex_unlock(&pool->mutex);
}

/**
 * @brief 获取缓存命中率
 */
double vector_buf_hit_rate(void *pool_ptr) {
    if (!pool_ptr) return 0.0;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);

    uint64_t total = pool->stats.hits + pool->stats.misses;
    double rate = total > 0 ? (double)pool->stats.hits / total : 0.0;

    pthread_mutex_unlock(&pool->mutex);
    return rate;
}

/* ========================================================================
 * 缓存预热
 * ======================================================================== */

/**
 * @brief 从文件预热缓存
 */
int vector_buf_warmup(void *pool_ptr, uint32_t segment_id,
                      uint32_t start_page, uint32_t count) {
    if (!pool_ptr) return 0;

    int warmed = 0;
    for (uint32_t i = 0; i < count; i++) {
        vector_buf_t *buf = vector_buf_get(pool_ptr, segment_id,
                                           start_page + i, VECTOR_PAGE_TYPE_DATA);
        if (buf) {
            vector_buf_release(buf);
            warmed++;
        }
    }

    LOG_INFO("缓存预热完成: segment=%u, pages=%u/%u",
             segment_id, warmed, count);
    return warmed;
}

/**
 * @brief 获取预热进度
 */
double vector_buf_warmup_progress(void *pool_ptr, uint32_t segment_id) {
    (void)pool_ptr;
    (void)segment_id;
    /* TODO: 实现预热进度跟踪 */
    return 1.0;
}

/* ========================================================================
 * 脏页管理
 * ======================================================================== */

/**
 * @brief 获取脏页数量
 */
uint32_t vector_buf_dirty_count(void *pool_ptr, uint32_t segment_id) {
    if (!pool_ptr) return 0;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < pool->hash_size; i++) {
        vector_buf_entry_t *entry = pool->entries[i];
        while (entry) {
            if (entry->is_dirty) {
                if (segment_id == 0 || entry->segment_id == segment_id) {
                    count++;
                }
            }
            entry = entry->next;
        }
    }

    pthread_mutex_unlock(&pool->mutex);
    return count;
}

/**
 * @brief 检查页面是否为脏
 */
bool vector_buf_is_dirty(const vector_buf_t *buf) {
    return buf ? buf->desc.is_dirty : false;
}

/* ========================================================================
 * 内存管理
 * ======================================================================== */

/**
 * @brief 获取缓存内存使用
 */
uint64_t vector_buf_memory_usage(void *pool_ptr) {
    if (!pool_ptr) return 0;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);

    uint64_t usage = sizeof(vector_buf_pool_t);
    usage += pool->hash_size * sizeof(vector_buf_entry_t *);
    usage += pool->entry_count * (sizeof(vector_buf_entry_t) + pool->config.page_size);

    pthread_mutex_unlock(&pool->mutex);
    return usage;
}

/**
 * @brief 设置缓存大小限制
 */
int vector_buf_set_max_size(void *pool_ptr, uint32_t max_size) {
    if (!pool_ptr) return -1;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);
    pool->config.pool_size = max_size;
    pool->stats.max_size = max_size;
    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

/**
 * @brief 驱逐最少使用的页面
 */
uint32_t vector_buf_evict_lru(void *pool_ptr) {
    if (!pool_ptr) return 0;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    pthread_mutex_lock(&pool->mutex);

    uint32_t evicted = 0;
    uint32_t oldest_time = UINT32_MAX;
    vector_buf_entry_t *oldest_entry = NULL;
    vector_buf_entry_t *oldest_prev = NULL;
    uint32_t oldest_bucket = 0;

    /* 查找最老的未使用页面 */
    for (uint32_t i = 0; i < pool->hash_size; i++) {
        vector_buf_entry_t *entry = pool->entries[i];
        vector_buf_entry_t *prev = NULL;

        while (entry) {
            if (entry->ref_count == 0 && entry->last_access < oldest_time) {
                oldest_time = entry->last_access;
                oldest_entry = entry;
                oldest_prev = prev;
                oldest_bucket = i;
            }
            prev = entry;
            entry = entry->next;
        }
    }

    /* 驱逐找到的页面 */
    if (oldest_entry) {
        /* 从链表中移除 */
        if (oldest_prev) {
            oldest_prev->next = oldest_entry->next;
        } else {
            pool->entries[oldest_bucket] = oldest_entry->next;
        }

        /* 刷新脏页 */
        if (oldest_entry->is_dirty) {
            /* TODO: 写入磁盘 */
            pool->stats.writes++;
        }

        /* 释放内存 */
        free(oldest_entry->data);
        free(oldest_entry);
        pool->entry_count--;
        pool->stats.evictions++;
        evicted = 1;

        LOG_DEBUG("驱逐 LRU 页面: seg=%u, page=%u",
                  oldest_entry->segment_id, oldest_entry->page_no);
    }

    pthread_mutex_unlock(&pool->mutex);
    return evicted;
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 打印缓存状态
 */
void vector_buf_print_status(void *pool_ptr) {
    if (!pool_ptr) return;

    vector_buf_pool_t *pool = (vector_buf_pool_t *)pool_ptr;
    vector_buf_stats_t stats;

    vector_buf_get_stats(pool_ptr, &stats);

    printf("\n");
    printf("========================================\n");
    printf("        向量 Buffer 池状态\n");
    printf("========================================\n");
    printf("当前条目数: %u / %u\n", stats.current_size, stats.max_size);
    printf("缓存命中:   %lu\n", (unsigned long)stats.hits);
    printf("缓存未命中: %lu\n", (unsigned long)stats.misses);
    printf("写入次数:   %lu\n", (unsigned long)stats.writes);
    printf("淘汰次数:   %lu\n", (unsigned long)stats.evictions);
    printf("命中率:     %.2f%%\n", vector_buf_hit_rate(pool_ptr) * 100);
    printf("内存使用:   %lu bytes\n", (unsigned long)vector_buf_memory_usage(pool_ptr));
    printf("========================================\n");
    printf("\n");
}
