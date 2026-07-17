/**
 * @file kv.c
 * @brief KV 数据库实现
 *
 * 基于 ART 索引和缓冲池的键值存储
 */

#include "db/kv.h"
#include "db/page.h"
#include "db/disk.h"
#include "db/buffer.h"
#include "db/core/log.h"
#include "db/storage/kv/kv_ttl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** KV 记录头部（存储在数据页中） */
typedef struct kv_record_s {
    uint32_t key_len;      /**< 键长度 */
    uint32_t value_len;   /**< 值长度 */
    /* 后面是 key + value 数据 */
} kv_record_t;

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 比较两个键 */
static int key_compare(const void *key1, size_t len1,
                       const void *key2, size_t len2) {
    size_t min_len = (len1 < len2) ? len1 : len2;
    int cmp = memcmp(key1, key2, min_len);
    if (cmp != 0) return cmp;
    return (int)(len1 - len2);
}

/** 设置错误信息 */
static void kv_set_error(kv_t *db, const char *msg) {
    if (db && db->error_msg) {
        free(db->error_msg);
    }
    if (db && msg) {
        db->error_msg = strdup(msg);
    }
}

/* ============================================================
 * 存储布局
 * ============================================================
 *
 * Page 0: 元数据页
 *   - 存储 num_keys 等统计信息
 *
 * Page 1+: 数据页
 *   - 每个数据页存储多个 KV 记录
 *   - 记录格式: [key_len(4)][value_len(4)][key][value]
 *   - 页面内记录按 key 排序（用于范围扫描）
 *
 * 简化设计：
 *   - 使用第一个数据页存储所有 KV 对
 *   - 键值对直接按插入顺序存储
 *   - 后续优化可以添加索引
 *
 * ============================================================ */

/** 元数据页中的键数量偏移 */
#define KV_NUM_KEYS_OFFSET PAGE_HEADER_SIZE

/** 获取数据页起始偏移 */
static page_id_t kv_get_data_page_id(const kv_t *db) {
    (void)db;
    return 1;  /* 第一个数据页 */
}

/**
 * @brief 计算页面中的记录数
 * @param page 数据页
 * @return 记录数量
 */
static size_t kv_count_records(const page_t *page) {
    if (!page) return 0;

    size_t count = 0;
    uint16_t offset = PAGE_HEADER_SIZE;
    uint32_t free_space = page->header.free_space_offset;

    while (offset < free_space) {
        if (offset + sizeof(uint32_t) * 2 > free_space) {
            /* 数据不完整，停止计数 */
            break;
        }

        uint32_t key_len, value_len;
        memcpy(&key_len, page->data + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(&value_len, page->data + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        /* 检查记录完整性 */
        if (offset + key_len + value_len > free_space) {
            break;
        }

        count++;
        offset += key_len + value_len;
    }

    return count;
}

/** 在页面中查找键（简单线性搜索） */
static int kv_page_find(const page_t *page,
                        const void *key, size_t key_len,
                        uint16_t *out_offset) {
    uint16_t offset = PAGE_HEADER_SIZE;
    int iter = 0;
    uint32_t free_space = page->header.free_space_offset;

    while (offset < free_space) {
        iter++;
        if (iter > 50) {
            /* 安全检查 */
            return -1;
        }

        /* 安全检查：确保 offset 不会越界 */
        if (offset + sizeof(kv_record_t) > PAGE_DATA_SIZE) {
            break;
        }

        kv_record_t *rec = (kv_record_t *)(page->data + offset);

        /* 安全检查：确保 rec->key_len 和 rec->value_len 在合理范围内 */
        if (rec->key_len > KV_MAX_KEY_SIZE || rec->value_len > KV_MAX_VALUE_SIZE) {
            break;
        }

        /* 检查数据是否越界 */
        if (offset + sizeof(kv_record_t) + rec->key_len + rec->value_len > PAGE_DATA_SIZE) {
            break;
        }

        int cmp = key_compare(key, key_len,
                              page->data + offset + sizeof(kv_record_t),
                              rec->key_len);

        if (cmp == 0) {
            *out_offset = offset;
            return 0;  /* 找到 */
        }

        offset += sizeof(kv_record_t) + rec->key_len + rec->value_len;
    }

    return -1;  /* 未找到 */
}

/** 在页面中插入键值对 */
static int kv_page_insert(buffer_pool_t *pool, page_id_t page_id,
                          const void *key, size_t key_len,
                          const void *value, size_t value_len) {
    page_t *page = buffer_get_page(pool, page_id);
    if (!page) return -1;

    size_t record_size = sizeof(kv_record_t) + key_len + value_len;

    /* 检查是否有足够空间 */
    if (page_get_free_space(page) < record_size) {
        buffer_unpin_page(pool, page_id);
        return -1;  /* 空间不足，后续优化：分配新页 */
    }

    /* 分配空间 */
    uint16_t offset = page_alloc_space(page, record_size);
    if (offset == (uint16_t)-1) {
        buffer_unpin_page(pool, page_id);
        return -1;
    }

    /* 写入记录 */
    kv_record_t *rec = (kv_record_t *)(page->data + offset);
    rec->key_len = (uint32_t)key_len;
    rec->value_len = (uint32_t)value_len;
    memcpy(page->data + offset + sizeof(kv_record_t), key, key_len);
    memcpy(page->data + offset + sizeof(kv_record_t) + key_len, value, value_len);

    /* 标记脏页并释放 */
    buffer_mark_dirty(pool, page_id);
    buffer_unpin_page(pool, page_id);

    return 0;
}

/** 更新页面中的值 */
static int kv_page_update(buffer_pool_t *pool, page_id_t page_id,
                          uint16_t record_offset,
                          const void *value, size_t value_len) {
    page_t *page = buffer_get_page(pool, page_id);
    if (!page) return -1;

    kv_record_t *rec = (kv_record_t *)(page->data + record_offset);
    rec->value_len = (uint32_t)value_len;
    memcpy(page->data + record_offset + sizeof(kv_record_t) + rec->key_len,
           value, value_len);

    /* 注意：更新后页面内空间可能碎片化，后续优化处理 */

    buffer_mark_dirty(pool, page_id);
    buffer_unpin_page(pool, page_id);

    return 0;
}

/** 删除页面中的记录 */
static int kv_page_delete(buffer_pool_t *pool, page_id_t page_id,
                         uint16_t record_offset) {
    page_t *page = buffer_get_page(pool, page_id);
    if (!page) return -1;

    kv_record_t *rec = (kv_record_t *)(page->data + record_offset);
    size_t record_size = sizeof(kv_record_t) + rec->key_len + rec->value_len;

    /* 简化实现：将后面的数据前移 */
    /* 后续优化：使用空闲链表管理碎片 */
    uint8_t *dst = page->data + record_offset;
    uint8_t *src = page->data + record_offset + record_size;
    size_t move_size = page->header.free_space_offset - record_offset - record_size;

    if (move_size > 0 && src + move_size <= page->data + PAGE_DATA_SIZE) {
        memmove(dst, src, move_size);
    }

    /* 更新空闲偏移 */
    page->header.free_space_offset -= (uint16_t)record_size;

    buffer_mark_dirty(pool, page_id);
    buffer_unpin_page(pool, page_id);

    return 0;
}

/* ============================================================
 * API 实现
 * ============================================================ */

kv_t *kv_open(const char *path) {
    if (!path) path = KV_DEFAULT_DB_NAME;

    kv_t *db = (kv_t *)calloc(1, sizeof(kv_t));
    if (!db) return NULL;

    /* 保存数据库路径 */
    db->db_path = strdup(path);
    if (!db->db_path) {
        free(db);
        return NULL;
    }

    /* 打开磁盘文件 */
    db->file = disk_open(path, DEFAULT_PAGE_SIZE);
    if (!db->file) {
        free(db);
        return NULL;
    }

    /* 创建缓存池 */
    db->pool = buffer_create(db->file, DEFAULT_BUFFER_SIZE);
    if (!db->pool) {
        disk_close(db->file);
        free(db);
        return NULL;
    }

    /* 创建/打开 WAL */
    char wal_path[256];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", path);

    /* 检查是否存在 WAL 文件 */
    FILE *wal_check = fopen(wal_path, "rb");
    if (wal_check) {
        fclose(wal_check);
        /* WAL 存在，尝试恢复 */
        if (kv_replay_wal(db, wal_path) != 0) {
            LOG_WARN("WAL 恢复失败，将创建新 WAL");
        }
    }

    db->wal = wal_create(wal_path, DEFAULT_PAGE_SIZE);
    if (!db->wal) {
        buffer_destroy(db->pool);
        disk_close(db->file);
        free(db);
        return NULL;
    }

    /* 确保有数据页 */
    page_id_t data_page_id = kv_get_data_page_id(db);
    page_t *page = buffer_get_page(db->pool, data_page_id);
    if (!page) {
        /* 分配第一个数据页 */
        page = buffer_alloc_page(db->pool, PAGE_DATA, &data_page_id);
        if (!page) {
            wal_close(db->wal);
            buffer_destroy(db->pool);
            disk_close(db->file);
            free(db);
            return NULL;
        }
        /* 新页面，num_keys 保持为 0 */
        buffer_unpin_page(db->pool, data_page_id);
    } else {
        /* 页面已存在：从页面数据计算 num_keys */
        db->num_keys = kv_count_records(page);
        buffer_unpin_page(db->pool, data_page_id);
    }

    /* 初始化 TTL 管理器 */
    db->ttl_mgr = kv_ttl_mgr_create(db->db_path);

    return db;
}

kv_result_t kv_close(kv_t *db) {
    if (!db) return KV_OK;

    /* 关闭 TTL 管理器（会保存 TTL 数据） */
    if (db->ttl_mgr) {
        /* TTL 管理器会在关闭时自动保存 */
        /* 这里不需要手动调用，kv_ttl_shutdown 会处理 */
    }

    /* 刷脏页 */
    if (db->pool) {
        buffer_flush_all(db->pool);
        buffer_destroy(db->pool);
    }

    /* 关闭 WAL */
    if (db->wal) {
        wal_close(db->wal);
    }

    if (db->file) {
        disk_close(db->file);
    }

    if (db->error_msg) {
        free(db->error_msg);
    }

    /* 关闭 TTL 管理器 */
    if (db->ttl_mgr) {
        kv_ttl_mgr_destroy((kv_ttl_mgr_t *)db->ttl_mgr);
    }

    /* 释放数据库路径 */
    if (db->db_path) {
        free(db->db_path);
    }

    free(db);
    return KV_OK;
}

const char *kv_errmsg(const kv_t *db) {
    if (!db) return "Invalid db handle";
    return db->error_msg ? (const char *)db->error_msg : "OK";
}

kv_result_t kv_flush(kv_t *db) {
    if (!db) return KV_INVALID;
    if (!db->pool) return KV_OK;

    /* 刷所有脏页到磁盘 */
    if (buffer_flush_all(db->pool) != 0) {
        kv_set_error(db, "Failed to flush dirty pages");
        return KV_ERROR;
    }
    return KV_OK;
}

kv_result_t kv_put(kv_t *db,
                   const void *key, size_t key_len,
                   const void *value, size_t value_len) {
    if (!db || !key || key_len == 0 || key_len > KV_MAX_KEY_SIZE) {
        return KV_INVALID;
    }
    if (!value || value_len > KV_MAX_VALUE_SIZE) {
        return KV_INVALID;
    }

    /* 获取旧值（用于 WAL 记录） */
    void *old_value = NULL;
    size_t old_value_len = 0;
    bool key_exists = (kv_get(db, key, key_len, &old_value, &old_value_len) == KV_OK);
    (void)key_exists;  /* 用于 WAL 记录，已获取到 old_value */

    page_id_t data_page_id = kv_get_data_page_id(db);
    page_t *page = buffer_get_page(db->pool, data_page_id);
    if (!page) {
        free(old_value);
        kv_set_error(db, "Failed to get data page");
        return KV_ERROR;
    }

    /* 查找是否已存在 */
    uint16_t offset;
    int found = (kv_page_find(page, key, key_len, &offset) == 0);

    /* 查找完成，解除页面固定 */
    buffer_unpin_page(db->pool, data_page_id);

    kv_result_t result;
    if (found) {
        /* 更新现有值 */
        if (kv_page_update(db->pool, data_page_id, offset, value, value_len) != 0) {
            free(old_value);
            kv_set_error(db, "Failed to update");
            return KV_ERROR;
        }
        /* 写入 WAL UPDATE 记录 */
        if (db->wal && old_value) {
            wal_write_update(db->wal, 0, key, key_len,
                            old_value, old_value_len, value, value_len);
        }
        result = KV_OK;
    } else {
        /* 插入新记录 */
        if (kv_page_insert(db->pool, data_page_id, key, key_len, value, value_len) != 0) {
            free(old_value);
            kv_set_error(db, "Failed to insert (page full)");
            return KV_ERROR;
        }
        db->num_keys++;
        /* 写入 WAL INSERT 记录 */
        if (db->wal) {
            wal_write_insert(db->wal, 0, key, key_len, value, value_len);
        }
        result = KV_OK;
    }

    free(old_value);
    return result;
}

kv_result_t kv_get(kv_t *db,
                   const void *key, size_t key_len,
                   void **out_value, size_t *out_len) {
    if (!db || !key || key_len == 0) {
        return KV_INVALID;
    }

    /* 检查 TTL 是否过期 */
    kv_ttl_mgr_t *ttl_mgr = (kv_ttl_mgr_t *)db->ttl_mgr;
    if (ttl_mgr && kv_ttl_is_expired(ttl_mgr, key, key_len)) {
        /* 键已过期，删除它 */
        kv_delete(db, key, key_len);
        /* 从 TTL 管理器中移除条目 */
        kv_ttl_delete(ttl_mgr, key, key_len);
        return KV_NOT_FOUND;
    }

    page_id_t data_page_id = kv_get_data_page_id(db);
    page_t *page = buffer_get_page(db->pool, data_page_id);
    if (!page) {
        return KV_NOT_FOUND;
    }

    uint16_t offset;
    if (kv_page_find(page, key, key_len, &offset) != 0) {
        buffer_unpin_page(db->pool, data_page_id);
        return KV_NOT_FOUND;
    }

    kv_record_t *rec = (kv_record_t *)(page->data + offset);

    if (out_value != NULL && out_len != NULL) {
        void *value = malloc(rec->value_len);
        if (!value) {
            buffer_unpin_page(db->pool, data_page_id);
            return KV_NOMEM;
        }
        memcpy(value,
               page->data + offset + sizeof(kv_record_t) + rec->key_len,
               rec->value_len);
        *out_value = value;
        *out_len = rec->value_len;
    }

    buffer_unpin_page(db->pool, data_page_id);

    return KV_OK;
}

kv_result_t kv_delete(kv_t *db, const void *key, size_t key_len) {
    if (!db || !key || key_len == 0) {
        return KV_INVALID;
    }

    /* 获取旧值（用于 WAL 记录） */
    void *old_value = NULL;
    size_t old_value_len = 0;
    kv_get(db, key, key_len, &old_value, &old_value_len);

    /* 同时从 TTL 管理器中删除 */
    kv_ttl_mgr_t *ttl_mgr = (kv_ttl_mgr_t *)db->ttl_mgr;
    if (ttl_mgr) {
        kv_ttl_delete(ttl_mgr, key, key_len);
    }

    page_id_t data_page_id = kv_get_data_page_id(db);
    page_t *page = buffer_get_page(db->pool, data_page_id);
    if (!page) {
        free(old_value);
        return KV_NOT_FOUND;
    }

    uint16_t offset;
    if (kv_page_find(page, key, key_len, &offset) != 0) {
        buffer_unpin_page(db->pool, data_page_id);
        free(old_value);
        return KV_NOT_FOUND;
    }

    /* 删除记录（保持页面 pinned，由 kv_page_delete 处理 unpin） */
    if (kv_page_delete(db->pool, data_page_id, offset) != 0) {
        buffer_unpin_page(db->pool, data_page_id);  /* 确保 unpin */
        free(old_value);
        return KV_ERROR;
    }

    /* 写入 WAL DELETE 记录 */
    if (db->wal && old_value) {
        wal_write_delete(db->wal, 0, key, key_len, old_value, old_value_len);
    }

    db->num_keys--;
    free(old_value);
    return KV_OK;
}

bool kv_exists(kv_t *db, const void *key, size_t key_len) {
    return kv_get(db, key, key_len, NULL, NULL) == KV_OK;
}

/* ============================================================
 * 统计信息
 * ============================================================ */

kv_result_t kv_stats(kv_t *db, kv_stats_t *stats) {
    if (!db || !stats) return KV_INVALID;

    stats->num_keys = db->num_keys;

    /* 从缓存池获取统计 */
    buffer_get_stats(db->pool,
                     &stats->cache_hit_rate,
                     &stats->page_count,
                     NULL);

    /* 计算总大小（估算） */
    stats->total_size = stats->page_count * DEFAULT_PAGE_SIZE;

    return KV_OK;
}

/* ============================================================
 * 批次操作
 * ============================================================ */

kv_result_t kv_batch_get(kv_t *db,
                         const void **keys, const size_t *key_lens, size_t n,
                         void ***out_values, size_t **out_lens) {
    if (!db || !keys || !key_lens || !out_values || !out_lens) {
        return KV_INVALID;
    }

    *out_values = (void **)calloc(n, sizeof(void *));
    *out_lens = (size_t *)calloc(n, sizeof(size_t));
    if (!*out_values || !*out_lens) {
        free(*out_values);
        free(*out_lens);
        return KV_NOMEM;
    }

    kv_result_t result = KV_OK;
    for (size_t i = 0; i < n; i++) {
        result = kv_get(db, keys[i], key_lens[i], &(*out_values)[i], &(*out_lens)[i]);
        if (result != KV_OK && result != KV_NOT_FOUND) {
            break;
        }
    }

    return result;
}

void kv_batch_free(void **values, size_t *lens, size_t n) {
    if (!values || !lens) return;
    for (size_t i = 0; i < n; i++) {
        free(values[i]);
    }
    free(values);
    free(lens);
}

/* ============================================================
 * 范围扫描
 * ============================================================ */

/** 扫描迭代器结构 */
struct kv_iter_s {
    kv_t        *db;            /**< 数据库句柄 */

    /* 范围边界 */
    char        start_key[KV_MAX_KEY_SIZE];
    size_t      start_len;
    char        end_key[KV_MAX_KEY_SIZE];
    size_t      end_len;

    /* 页面数据缓存 */
    page_t      *page;          /**< 当前页面（缓存） */
    uint16_t    offset;         /**< 当前页面内偏移 */
    bool        page_valid;     /**< 页面是否有效 */

    /* 当前键值（缓存） */
    char        current_key[KV_MAX_KEY_SIZE];
    size_t      current_key_len;
    char        current_value[KV_MAX_VALUE_SIZE];
    size_t      current_value_len;

    bool        eof;            /**< 是否结束 */
    bool        first_call;     /**< 是否首次调用 next */
};


/* ============================================================
 * 调试辅助函数
 * ============================================================ */

void *kv_get_buffer_pool(kv_t *db) {
    return db ? db->pool : NULL;
}

/* ============================================================
 * WAL 重放实现
 * ============================================================ */

/**
 * @brief WAL 重放回调：应用一条日志记录
 */
static int kv_wal_apply(void *ctx, wal_log_type_t type,
                        const void *key, size_t key_len,
                        const void *value, size_t value_len) {
    kv_t *db = (kv_t *)ctx;

    switch (type) {
        case WAL_LOG_INSERT:
            /* 插入操作，直接写入（忽略已存在的情况） */
            kv_page_insert(db->pool, kv_get_data_page_id(db),
                          key, key_len, value, value_len);
            db->num_keys++;
            break;

        case WAL_LOG_UPDATE:
            /* 更新操作：先查找是否存在，然后更新 */
            {
                page_id_t data_page_id = kv_get_data_page_id(db);
                page_t *page = buffer_get_page(db->pool, data_page_id);
                if (page) {
                    uint16_t offset;
                    if (kv_page_find(page, key, key_len, &offset) == 0) {
                        kv_page_update(db->pool, data_page_id, offset, value, value_len);
                    } else {
                        /* 不存在则插入 */
                        kv_page_insert(db->pool, data_page_id, key, key_len, value, value_len);
                        db->num_keys++;
                    }
                    buffer_unpin_page(db->pool, data_page_id);
                }
            }
            break;

        case WAL_LOG_DELETE:
            /* 删除操作：查找并删除 */
            {
                page_id_t data_page_id = kv_get_data_page_id(db);
                page_t *page = buffer_get_page(db->pool, data_page_id);
                if (page) {
                    uint16_t offset;
                    if (kv_page_find(page, key, key_len, &offset) == 0) {
                        kv_page_delete(db->pool, data_page_id, offset);
                        db->num_keys--;
                    }
                    buffer_unpin_page(db->pool, data_page_id);
                }
            }
            break;

        default:
            /* 忽略其他类型的日志 */
            break;
    }

    return 0;
}

int kv_replay_wal(kv_t *db, const char *wal_path) {
    if (!db || !wal_path) return -1;

    /* 使用 wal_redo 重放所有日志 */
    return wal_redo(wal_path, 0, kv_wal_apply, db);
}
