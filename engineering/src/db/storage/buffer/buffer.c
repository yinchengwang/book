/**
 * @file buffer.c
 * @brief 缓存池实现 - LRU 页面缓存
 *
 * 实现内存页面缓存，带 LRU 淘汰策略
 */

#include "db/buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* ============================================================
 * 哈希函数
 * ============================================================ */

static size_t hash_page_id(page_id_t page_id, size_t hash_size) {
    /* 简单的乘以大素数哈希 */
    return ((size_t)page_id * 11400714819323198485ULL) % hash_size;
}

/* ============================================================
 * LRU 链表操作
 * ============================================================ */

/** 将帧移动到 LRU 尾部（最近使用） */
static void lru_move_to_tail(buffer_pool_t *pool, buffer_frame_t *frame) {
    if (!frame || frame == pool->lru_tail) return;

    /* 从当前位置移除 */
    if (frame->prev) frame->prev->next = frame->next;
    if (frame->next) frame->next->prev = frame->prev;

    /* 如果是头部，更新头部指针 */
    if (frame == pool->lru_head) {
        pool->lru_head = frame->next;
    }

    /* 插入到尾部 */
    frame->next = NULL;
    frame->prev = pool->lru_tail;
    if (pool->lru_tail) {
        pool->lru_tail->next = frame;
    }
    pool->lru_tail = frame;

    /* 如果链表为空 */
    if (!pool->lru_head) {
        pool->lru_head = frame;
    }
}

/** 将帧从 LRU 链表移除 */
static void lru_remove(buffer_pool_t *pool, buffer_frame_t *frame) {
    if (!frame) return;

    if (frame->prev) frame->prev->next = frame->next;
    if (frame->next) frame->next->prev = frame->prev;

    if (frame == pool->lru_head) pool->lru_head = frame->next;
    if (frame == pool->lru_tail) pool->lru_tail = frame->prev;

    frame->prev = NULL;
    frame->next = NULL;
}

/** 将帧添加到 LRU 尾部 */
static void lru_add_to_tail(buffer_pool_t *pool, buffer_frame_t *frame) {
    if (!frame) return;

    frame->next = NULL;
    frame->prev = pool->lru_tail;

    if (pool->lru_tail) {
        pool->lru_tail->next = frame;
    }
    pool->lru_tail = frame;

    if (!pool->lru_head) {
        pool->lru_head = frame;
    }
}

/* ============================================================
 * 哈希表操作
 * ============================================================ */

static void hash_insert(buffer_pool_t *pool, buffer_frame_t *frame) {
    size_t idx = hash_page_id(frame->page_id, pool->hash_size);

    frame->hash_next = pool->hash_table[idx];
    frame->hash_prev = NULL;

    if (pool->hash_table[idx]) {
        pool->hash_table[idx]->hash_prev = frame;
    }
    pool->hash_table[idx] = frame;
}

static void hash_remove(buffer_pool_t *pool, buffer_frame_t *frame) {
    if (frame->hash_prev) {
        frame->hash_prev->hash_next = frame->hash_next;
    } else {
        size_t idx = hash_page_id(frame->page_id, pool->hash_size);
        pool->hash_table[idx] = frame->hash_next;
    }

    if (frame->hash_next) {
        frame->hash_next->hash_prev = frame->hash_prev;
    }

    frame->hash_next = NULL;
    frame->hash_prev = NULL;
}

static buffer_frame_t *hash_lookup(buffer_pool_t *pool, page_id_t page_id) {
    size_t idx = hash_page_id(page_id, pool->hash_size);
    buffer_frame_t *frame = pool->hash_table[idx];

    while (frame) {
        if (frame->page_id == page_id) {
            return frame;
        }
        frame = frame->hash_next;
    }
    return NULL;
}

/* ============================================================
 * 淘汰页面
 * ============================================================ */

/**
 * @brief 淘汰一个页面
 *
 * 从 LRU 头部选择未固定且未在 IO 的页面进行淘汰
 */
static int buffer_evict(buffer_pool_t *pool) {
    buffer_frame_t *frame = pool->lru_head;

    while (frame) {
        /* 跳过被持有的页面 */
        if (frame->pin_count > 0 || (frame->flags & (BF_READING | BF_WRITING))) {
            frame = frame->next;
            continue;
        }

        /* 如果是脏页，先刷盘 */
        if (frame->flags & BF_DIRTY) {
            if (disk_write_page(pool->file, frame->page_id, frame->page) != 0) {
                /* 刷盘失败时先从 LRU 移除，避免重复尝试 */
                lru_remove(pool, frame);
                return -1;
            }
            frame->flags &= ~BF_DIRTY;
            pool->write_count++;
        }

        /* 从缓存中移除 */
        lru_remove(pool, frame);
        hash_remove(pool, frame);

        /* 标记为未分配 */
        frame->page_id = INVALID_PAGE_ID;
        page_free(frame->page);
        frame->page = NULL;
        frame->flags = BF_NONE;
        frame->pin_count = 0;

        /* 添加到空闲链表 */
        frame->next = pool->free_list;
        pool->free_list = frame;
        pool->page_count--;
        pool->evict_count++;

        return 0;
    }

    /* 没有可淘汰的页面 */
    return -1;
}

/* ============================================================
 * 分配帧
 * ============================================================ */

static buffer_frame_t *buffer_alloc_frame(buffer_pool_t *pool) {
    buffer_frame_t *frame;

    /* 优先从空闲链表获取 */
    if (pool->free_list) {
        frame = pool->free_list;
        pool->free_list = frame->next;

        /* 重用帧时需要分配新页面 */
        frame->page = (page_t *)calloc(1, pool->page_size);
        if (!frame->page) {
            /* 分配失败，放回空闲链表 */
            frame->next = pool->free_list;
            pool->free_list = frame;
            return NULL;
        }

        /* 重置帧状态 */
        frame->page_id = INVALID_PAGE_ID;
        frame->flags = 0;
        frame->pin_count = 0;
        frame->hash_next = NULL;
        frame->hash_prev = NULL;
        frame->next = NULL;
        frame->prev = NULL;

        return frame;
    }

    /* 需要淘汰页面 */
    if (pool->page_count >= pool->pool_size) {
        if (buffer_evict(pool) != 0) {
            return NULL;
        }
        /* 淘汰后重试从空闲链表获取 */
        if (pool->free_list) {
            return buffer_alloc_frame(pool);
        }
    }

    /* 创建新帧 */
    frame = (buffer_frame_t *)calloc(1, sizeof(buffer_frame_t));
    if (!frame) return NULL;

    frame->page = (page_t *)calloc(1, pool->page_size);
    if (!frame->page) {
        free(frame);
        return NULL;
    }

    pool->page_count++;
    return frame;
}

/* ============================================================
 * 缓存池操作实现
 * ============================================================ */

buffer_pool_t *buffer_create(db_file_t *file, size_t pool_size) {
    if (!file || pool_size == 0 || pool_size > MAX_BUFFER_SIZE) {
        return NULL;
    }

    buffer_pool_t *pool = (buffer_pool_t *)calloc(1, sizeof(buffer_pool_t));
    if (!pool) return NULL;

    pool->file = file;
    pool->page_size = disk_get_page_size(file);
    pool->pool_size = pool_size;
    pool->page_count = 0;
    pool->free_list = NULL;
    pool->lru_head = NULL;
    pool->lru_tail = NULL;

    /* 初始化哈希表 */
    pool->hash_size = pool_size * 2;
    pool->hash_table = (buffer_frame_t **)calloc(pool->hash_size, sizeof(buffer_frame_t *));
    if (!pool->hash_table) {
        free(pool);
        return NULL;
    }

    /* 初始化空闲帧链表 */
    for (size_t i = 0; i < pool_size && i < 16; i++) {
        buffer_frame_t *frame = (buffer_frame_t *)calloc(1, sizeof(buffer_frame_t));
        if (!frame) break;
        frame->page = (page_t *)calloc(1, pool->page_size);
        if (!frame->page) {
            free(frame);
            break;
        }
        frame->next = pool->free_list;
        pool->free_list = frame;
        pool->page_count++;
    }

    return pool;
}

void buffer_destroy(buffer_pool_t *pool) {
    if (!pool) return;

    /* 刷所有脏页 */
    buffer_flush_all(pool);

    /* 释放所有帧 */
    for (size_t i = 0; i < pool->hash_size; i++) {
        buffer_frame_t *frame = pool->hash_table[i];
        while (frame) {
            buffer_frame_t *next = frame->hash_next;
            if (frame->page) free(frame->page);
            free(frame);
            frame = next;
        }
    }

    /* 释放空闲链表 */
    buffer_frame_t *frame = pool->free_list;
    while (frame) {
        buffer_frame_t *next = frame->next;
        if (frame->page) free(frame->page);
        free(frame);
        frame = next;
    }

    free(pool->hash_table);
    free(pool);
}

page_t *buffer_get_page(buffer_pool_t *pool, page_id_t page_id) {
    if (!pool) return NULL;

    /* 先查哈希表 */
    buffer_frame_t *frame = hash_lookup(pool, page_id);

    if (frame) {
        /* 缓存命中 */
        pool->hit_count++;
        frame->pin_count++;
        lru_move_to_tail(pool, frame);
        return frame->page;
    }

    /* 缓存未命中，从磁盘读取 */
    pool->miss_count++;

    /* 分配帧 */
    frame = buffer_alloc_frame(pool);
    if (!frame) return NULL;

    /* 从磁盘读取页面 */
    if (page_id == INVALID_PAGE_ID) {
        /* 新页面 */
        frame->page->header.page_id = 0;
        frame->page->header.page_type = PAGE_FREE;
        frame->page->header.free_space_offset = PAGE_HEADER_SIZE;
    } else {
        /* 读取已有页面 */
        page_t *disk_page = disk_read_page(pool->file, page_id);
        if (!disk_page) {
            /* 归还帧 */
            frame->next = pool->free_list;
            pool->free_list = frame;
            pool->page_count--;
            return NULL;
        }
        memcpy(frame->page, disk_page, pool->page_size);
        page_free(disk_page);
    }

    /* 初始化帧 */
    frame->page_id = page_id;
    frame->flags = BF_PINNED;
    frame->pin_count = 1;

    /* 加入缓存 */
    hash_insert(pool, frame);
    lru_add_to_tail(pool, frame);

    return frame->page;
}

page_t *buffer_alloc_page(buffer_pool_t *pool, page_type_t type, page_id_t *out_page_id) {
    if (!pool) return NULL;

    /* 分配帧 */
    buffer_frame_t *frame = buffer_alloc_frame(pool);
    if (!frame) return NULL;

    /* 分配磁盘页面 */
    page_id_t new_page_id;
    page_t *disk_page = disk_alloc_page(pool->file, type, &new_page_id);
    if (!disk_page) {
        frame->next = pool->free_list;
        pool->free_list = frame;
        pool->page_count--;
        return NULL;
    }

    /* 复制页面数据 */
    memcpy(frame->page, disk_page, pool->page_size);
    page_free(disk_page);

    /* 初始化帧 */
    frame->page_id = new_page_id;
    frame->flags = BF_PINNED | BF_DIRTY;
    frame->pin_count = 1;

    /* 加入缓存 */
    hash_insert(pool, frame);
    lru_add_to_tail(pool, frame);

    if (out_page_id) {
        *out_page_id = new_page_id;
    }

    return frame->page;
}

void buffer_unpin_page(buffer_pool_t *pool, page_id_t page_id) {
    if (!pool) return;

    buffer_frame_t *frame = hash_lookup(pool, page_id);
    if (!frame) return;

    if (frame->pin_count > 0) {
        frame->pin_count--;
    }

    if (frame->pin_count == 0) {
        frame->flags &= ~BF_PINNED;
    }
}

void buffer_mark_dirty(buffer_pool_t *pool, page_id_t page_id) {
    if (!pool) return;

    buffer_frame_t *frame = hash_lookup(pool, page_id);
    if (!frame) return;

    frame->flags |= BF_DIRTY;
}

int buffer_flush_page(buffer_pool_t *pool, page_id_t page_id) {
    if (!pool) return -1;

    buffer_frame_t *frame = hash_lookup(pool, page_id);
    if (!frame) return -1;

    if (!(frame->flags & BF_DIRTY)) {
        return 0;
    }

    if (disk_write_page(pool->file, page_id, frame->page) != 0) {
        return -1;
    }

    frame->flags &= ~BF_DIRTY;
    pool->write_count++;

    return 0;
}

int buffer_flush_all(buffer_pool_t *pool) {
    if (!pool) return -1;

    int ret = 0;

    for (size_t i = 0; i < pool->hash_size; i++) {
        buffer_frame_t *frame = pool->hash_table[i];
        while (frame) {
            if (frame->flags & BF_DIRTY) {
                if (buffer_flush_page(pool, frame->page_id) != 0) {
                    ret = -1;
                }
            }
            frame = frame->hash_next;
        }
    }

    /* 同步文件 */
    disk_sync(pool->file);

    return ret;
}

void buffer_get_stats(buffer_pool_t *pool,
                      double *hit_rate,
                      size_t *page_count,
                      size_t *dirty_count) {
    if (!pool) return;

    uint64_t total = pool->hit_count + pool->miss_count;
    if (hit_rate) {
        *hit_rate = (total > 0) ? ((double)pool->hit_count / total * 100.0) : 0.0;
    }

    if (page_count) {
        *page_count = pool->page_count;
    }

    if (dirty_count) {
        size_t count = 0;
        for (size_t i = 0; i < pool->hash_size; i++) {
            buffer_frame_t *frame = pool->hash_table[i];
            while (frame) {
                if (frame->flags & BF_DIRTY) count++;
                frame = frame->hash_next;
            }
        }
        *dirty_count = count;
    }
}

bool buffer_contains_page(buffer_pool_t *pool, page_id_t page_id) {
    if (!pool) return false;
    return hash_lookup(pool, page_id) != NULL;
}

page_t *buffer_peek_page(buffer_pool_t *pool, page_id_t page_id) {
    if (!pool) return NULL;

    buffer_frame_t *frame = hash_lookup(pool, page_id);
    return frame ? frame->page : NULL;
}
