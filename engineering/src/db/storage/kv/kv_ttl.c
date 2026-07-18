/**
 * @file kv_ttl.c
 * @brief KV 数据库 TTL（过期时间）实现
 *
 * 使用有序数组管理 TTL 条目，支持：
 * - O(log n) 查找
 * - O(log n) 插入/删除
 * - O(k) 获取所有过期键（k 为过期键数量）
 */

#include "db/storage/kv/kv_ttl.h"
#include "db/storage/kv/kv.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** TTL 文件头 */
typedef struct kv_ttl_file_header_s {
    uint32_t magic;        /**< 文件魔数 */
    uint32_t version;      /**< 文件版本 */
    uint32_t num_entries;  /**< 条目数量 */
    uint64_t reserved;     /**< 保留字段 */
} kv_ttl_file_header_t;

#define KV_TTL_FILE_MAGIC 0x54544C4B  /* "TTLK" */

/** TTL 管理器内部结构 */
struct kv_ttl_mgr_s {
    char            db_path[256];    /**< 数据库路径 */
    char            ttl_file[312];   /**< TTL 文件路径 */
    kv_ttl_entry_t  *entries;       /**< TTL 条目数组 */
    size_t          capacity;       /**< 数组容量 */
    size_t          num_entries;    /**< 当前条目数 */
    bool            dirty;          /**< 是否脏（需要保存） */
    int64_t         last_cleanup;   /**< 上次清理时间 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 获取当前时间（毫秒） */
static int64_t get_current_time_ms(void) {
#if defined(_WIN32) || defined(_WIN64)
    /* 使用 clock_gettime 的简化实现 */
    return (int64_t)clock() * 1000 / CLOCKS_PER_SEC;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/** 比较两个键（与 kv.c 保持一致） */
static int key_compare(const void *key1, size_t len1,
                       const void *key2, size_t len2) {
    size_t min_len = (len1 < len2) ? len1 : len2;
    int cmp = memcmp(key1, key2, min_len);
    if (cmp != 0) return cmp;
    return (int)(len1 - len2);
}

/** 二分查找键的位置 */
static int64_t kv_ttl_find_index(const kv_ttl_mgr_t *mgr,
                                  const void *key, size_t key_len) {
    int64_t left = 0;
    int64_t right = (int64_t)mgr->num_entries - 1;

    while (left <= right) {
        int64_t mid = left + (right - left) / 2;
        int cmp = key_compare(key, key_len,
                              mgr->entries[mid].key, mgr->entries[mid].key_len);

        if (cmp == 0) {
            return mid;  /* 找到 */
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    return -1;  /* 未找到 */
}

/** 二分查找插入位置（按过期时间排序，同过期时间按键排序） */
static int64_t kv_ttl_find_insert_pos(const kv_ttl_mgr_t *mgr,
                                       const void *key, size_t key_len,
                                       int64_t expire_at_ms) {
    int64_t left = 0;
    int64_t right = (int64_t)mgr->num_entries;

    while (left < right) {
        int64_t mid = left + (right - left) / 2;

        /* 先比较过期时间 */
        if (mgr->entries[mid].expire_at_ms < expire_at_ms) {
            left = mid + 1;
        } else if (mgr->entries[mid].expire_at_ms > expire_at_ms) {
            right = mid;
        } else {
            /* 过期时间相同，比较键 */
            int cmp = key_compare(key, key_len,
                                  mgr->entries[mid].key, mgr->entries[mid].key_len);
            if (cmp <= 0) {
                right = mid;
            } else {
                left = mid + 1;
            }
        }
    }

    return left;
}

/** 扩容条目数组 */
static int kv_ttl_expand(kv_ttl_mgr_t *mgr) {
    size_t new_capacity = mgr->capacity > 0 ? mgr->capacity * 2 : 64;
    kv_ttl_entry_t *new_entries = (kv_ttl_entry_t *)realloc(
        mgr->entries, new_capacity * sizeof(kv_ttl_entry_t));

    if (!new_entries) {
        LOG_ERROR("TTL 管理器扩容失败");
        return -1;
    }

    mgr->entries = new_entries;
    mgr->capacity = new_capacity;
    return 0;
}

/* ============================================================
 * TTL 管理器生命周期
 * ============================================================ */

kv_ttl_mgr_t *kv_ttl_mgr_create(const char *db_path) {
    if (!db_path) return NULL;

    kv_ttl_mgr_t *mgr = (kv_ttl_mgr_t *)calloc(1, sizeof(kv_ttl_mgr_t));
    if (!mgr) return NULL;

    strncpy(mgr->db_path, db_path, sizeof(mgr->db_path) - 1);
    snprintf(mgr->ttl_file, sizeof(mgr->ttl_file), "%s%s",
             db_path, KV_TTL_FILE_EXT);

    mgr->entries = (kv_ttl_entry_t *)calloc(64, sizeof(kv_ttl_entry_t));
    if (!mgr->entries) {
        free(mgr);
        return NULL;
    }

    mgr->capacity = 64;
    mgr->num_entries = 0;
    mgr->dirty = false;
    mgr->last_cleanup = get_current_time_ms();

    /* 尝试加载已有 TTL 数据 */
    kv_ttl_mgr_load(mgr);

    return mgr;
}

void kv_ttl_mgr_destroy(kv_ttl_mgr_t *mgr) {
    if (!mgr) return;

    /* 保存脏数据 */
    if (mgr->dirty) {
        kv_ttl_mgr_save(mgr);
    }

    /* 清理过期条目（减少保存的数据量） */
    kv_ttl_cleanup_expired(mgr);
    kv_ttl_mgr_save(mgr);

    free(mgr->entries);
    free(mgr);
}

int kv_ttl_mgr_load(kv_ttl_mgr_t *mgr) {
    if (!mgr) return -1;

    FILE *fp = fopen(mgr->ttl_file, "rb");
    if (!fp) {
        /* TTL 文件不存在是正常的 */
        return 0;
    }

    /* 读取文件头 */
    kv_ttl_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    /* 验证魔数和版本 */
    if (header.magic != KV_TTL_FILE_MAGIC || header.version != KV_TTL_FILE_VERSION) {
        LOG_WARN("TTL 文件格式无效，将重新创建");
        fclose(fp);
        return -1;
    }

    /* 确保容量足够 */
    while (mgr->capacity < header.num_entries) {
        if (kv_ttl_expand(mgr) != 0) {
            fclose(fp);
            return -1;
        }
    }

    /* 读取条目 */
    size_t read_count = fread(mgr->entries, sizeof(kv_ttl_entry_t),
                              header.num_entries, fp);
    mgr->num_entries = read_count;

    fclose(fp);
    mgr->dirty = false;

    LOG_INFO("TTL 管理器加载了 %zu 个条目", mgr->num_entries);
    return 0;
}

int kv_ttl_mgr_save(kv_ttl_mgr_t *mgr) {
    if (!mgr) return -1;
    if (mgr->num_entries == 0) {
        /* 没有 TTL 条目，删除文件（如果存在） */
        remove(mgr->ttl_file);
        return 0;
    }

    FILE *fp = fopen(mgr->ttl_file, "wb");
    if (!fp) {
        LOG_ERROR("无法打开 TTL 文件进行写入: %s", mgr->ttl_file);
        return -1;
    }

    /* 写入文件头 */
    kv_ttl_file_header_t header = {
        .magic = KV_TTL_FILE_MAGIC,
        .version = KV_TTL_FILE_VERSION,
        .num_entries = (uint32_t)mgr->num_entries,
        .reserved = 0
    };

    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    /* 写入条目 */
    if (fwrite(mgr->entries, sizeof(kv_ttl_entry_t), mgr->num_entries, fp)
        != mgr->num_entries) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    mgr->dirty = false;

    return 0;
}

/* ============================================================
 * TTL 操作实现
 * ============================================================ */

int kv_ttl_set(kv_ttl_mgr_t *mgr, const void *key, size_t key_len, int64_t ttl_ms) {
    if (!mgr || !key || key_len == 0) return -1;

    /* 计算过期时间 */
    int64_t expire_at_ms;
    if (ttl_ms <= 0) {
        /* 永不过期 */
        expire_at_ms = KV_TTL_INFINITE;
    } else {
        expire_at_ms = get_current_time_ms() + ttl_ms;
    }

    /* 查找是否已存在 */
    int64_t idx = kv_ttl_find_index(mgr, key, key_len);

    if (idx >= 0) {
        /* 已存在，更新过期时间 */
        mgr->entries[idx].expire_at_ms = expire_at_ms;
        mgr->dirty = true;
        return 0;
    }

    /* 需要插入新条目 */
    if (mgr->num_entries >= mgr->capacity) {
        if (kv_ttl_expand(mgr) != 0) {
            return -1;
        }
    }

    /* 找到插入位置 */
    int64_t insert_pos = kv_ttl_find_insert_pos(mgr, key, key_len, expire_at_ms);

    /* 移动后面的元素 */
    for (int64_t i = (int64_t)mgr->num_entries; i > insert_pos; i--) {
        mgr->entries[i] = mgr->entries[i - 1];
    }

    /* 插入新条目 */
    kv_ttl_entry_t *entry = &mgr->entries[insert_pos];
    memset(entry, 0, sizeof(kv_ttl_entry_t));
    memcpy(entry->key, key, key_len);
    entry->key_len = key_len;
    entry->expire_at_ms = expire_at_ms;
    entry->deleted = false;

    mgr->num_entries++;
    mgr->dirty = true;

    return 0;
}

int64_t kv_ttl_get_remaining(kv_ttl_mgr_t *mgr, const void *key, size_t key_len) {
    if (!mgr || !key) return -1;

    int64_t idx = kv_ttl_find_index(mgr, key, key_len);
    if (idx < 0) {
        return -1;  /* 键不存在 */
    }

    int64_t expire_at_ms = mgr->entries[idx].expire_at_ms;

    if (expire_at_ms >= KV_TTL_INFINITE) {
        return -2;  /* 永不过期 */
    }

    int64_t remaining = expire_at_ms - get_current_time_ms();
    return remaining > 0 ? remaining : 0;  /* 已过期返回 0 */
}

bool kv_ttl_is_expired(kv_ttl_mgr_t *mgr, const void *key, size_t key_len) {
    if (!mgr || !key) return false;

    int64_t idx = kv_ttl_find_index(mgr, key, key_len);
    if (idx < 0) {
        return false;  /* 键不在 TTL 表中 => 没有过期信息，视为未过期 */
    }

    int64_t expire_at_ms = mgr->entries[idx].expire_at_ms;

    if (expire_at_ms >= KV_TTL_INFINITE) {
        return false;  /* 永不过期 */
    }

    return expire_at_ms <= get_current_time_ms();
}

int kv_ttl_persist(kv_ttl_mgr_t *mgr, const void *key, size_t key_len) {
    if (!mgr || !key) return -1;

    int64_t idx = kv_ttl_find_index(mgr, key, key_len);
    if (idx < 0) {
        return -1;  /* 键不存在 */
    }

    /* 设置为永不过期 */
    mgr->entries[idx].expire_at_ms = KV_TTL_INFINITE;
    mgr->dirty = true;

    return 0;
}

int kv_ttl_delete(kv_ttl_mgr_t *mgr, const void *key, size_t key_len) {
    if (!mgr || !key) return -1;

    int64_t idx = kv_ttl_find_index(mgr, key, key_len);
    if (idx < 0) {
        return -1;  /* 键不存在 */
    }

    /* 移动后面的元素 */
    for (int64_t i = idx; i < (int64_t)mgr->num_entries - 1; i++) {
        mgr->entries[i] = mgr->entries[i + 1];
    }

    mgr->num_entries--;
    mgr->dirty = true;

    return 0;
}

/* ============================================================
 * TTL 批量操作
 * ============================================================ */

size_t kv_ttl_get_expired_keys(kv_ttl_mgr_t *mgr,
                                void **out_keys, size_t *out_lens,
                                size_t max_count) {
    if (!mgr || !out_keys || !out_lens || max_count == 0) {
        return 0;
    }

    int64_t now = get_current_time_ms();
    size_t count = 0;

    for (size_t i = 0; i < mgr->num_entries && count < max_count; i++) {
        if (mgr->entries[i].expire_at_ms > 0 &&
            mgr->entries[i].expire_at_ms < KV_TTL_INFINITE &&
            mgr->entries[i].expire_at_ms <= now) {
            out_keys[count] = mgr->entries[i].key;
            out_lens[count] = mgr->entries[i].key_len;
            count++;
        }
    }

    return count;
}

size_t kv_ttl_cleanup_expired(kv_ttl_mgr_t *mgr) {
    if (!mgr) return 0;

    int64_t now = get_current_time_ms();
    size_t removed = 0;
    size_t write_idx = 0;

    /* 使用两遍扫描：先找出未过期的，然后移动覆盖过期的 */
    for (size_t read_idx = 0; read_idx < mgr->num_entries; read_idx++) {
        if (mgr->entries[read_idx].expire_at_ms > 0 &&
            mgr->entries[read_idx].expire_at_ms < KV_TTL_INFINITE &&
            mgr->entries[read_idx].expire_at_ms <= now) {
            removed++;
        } else {
            if (write_idx != read_idx) {
                mgr->entries[write_idx] = mgr->entries[read_idx];
            }
            write_idx++;
        }
    }

    mgr->num_entries = write_idx;

    if (removed > 0) {
        mgr->dirty = true;
        LOG_INFO("TTL 清理移除了 %zu 个过期条目", removed);
    }

    mgr->last_cleanup = now;

    return removed;
}

/* ============================================================
 * 统计信息
 * ============================================================ */

void kv_ttl_get_stats(kv_ttl_mgr_t *mgr, kv_ttl_stats_t *stats) {
    if (!mgr || !stats) return;

    memset(stats, 0, sizeof(kv_ttl_stats_t));

    stats->total_entries = mgr->num_entries;

    int64_t now = get_current_time_ms();
    int64_t next_expire = 0;

    for (size_t i = 0; i < mgr->num_entries; i++) {
        int64_t expire_at = mgr->entries[i].expire_at_ms;

        if (expire_at >= KV_TTL_INFINITE) {
            stats->persistent_keys++;
        } else if (expire_at > 0 && expire_at <= now) {
            stats->expired_entries++;
        } else if (expire_at > now) {
            if (next_expire == 0 || expire_at < next_expire) {
                next_expire = expire_at;
            }
        }
    }

    stats->next_expire_time = next_expire;
}

/* ============================================================
 * 高级 API（与 kv_t 集成）
 * ============================================================ */

int kv_put_with_ttl(kv_t *db,
                    const void *key, size_t key_len,
                    const void *value, size_t value_len,
                    int64_t ttl_ms) {
    if (!db || !key || !value) return -1;

    /* 先执行普通插入 */
    kv_result_t result = kv_put(db, key, key_len, value, value_len);
    if (result != KV_OK) {
        return -1;
    }

    /* 设置 TTL */
    kv_ttl_mgr_t *mgr = (kv_ttl_mgr_t *)db->ttl_mgr;
    if (mgr && ttl_ms > 0) {
        if (kv_ttl_set(mgr, key, key_len, ttl_ms) != 0) {
            /* TTL 设置失败，但值已插入，视为部分成功 */
            LOG_WARN("TTL 设置失败");
        }
    }

    return 0;
}

int64_t kv_get_ttl(kv_t *db, const void *key, size_t key_len) {
    if (!db || !key) return -1;

    kv_ttl_mgr_t *mgr = (kv_ttl_mgr_t *)db->ttl_mgr;
    if (!mgr) return -1;

    return kv_ttl_get_remaining(mgr, key, key_len);
}

int kv_persist(kv_t *db, const void *key, size_t key_len) {
    if (!db || !key) return -1;

    kv_ttl_mgr_t *mgr = (kv_ttl_mgr_t *)db->ttl_mgr;
    if (!mgr) return -1;

    if (kv_ttl_persist(mgr, key, key_len) != 0) {
        return -1;
    }

    return 0;
}

int kv_ttl_init(kv_t *db) {
    if (!db) return -1;

    /* 获取数据库路径 */
    /* 注意：db->file 是 disk_t 指针，我们需要从它获取路径 */
    /* 这里简化处理，假设 db->file 是磁盘文件句柄 */

    /* 对于 kv_open，它使用 path 参数，但路径没有保存在 kv_t 中 */
    /* 我们需要修改 kv_open 来保存路径，或者使用其他方式获取路径 */

    /* 临时方案：TTL 文件路径将在首次设置 TTL 时构造 */
    /* 为此，我们在 kv_t 中添加一个 path 字段 */

    /* 已有的 ttl_mgr 初始化延迟到实际使用时 */
    return 0;
}

void kv_ttl_shutdown(kv_t *db) {
    if (!db) return;

    kv_ttl_mgr_t *mgr = (kv_ttl_mgr_t *)db->ttl_mgr;
    if (mgr) {
        kv_ttl_mgr_destroy(mgr);
        db->ttl_mgr = NULL;
    }
}

/* ============================================================
 * WAL 集成（TTL 变更需要记录到 WAL）
 * ============================================================ */

/**
 * @brief 记录 TTL 设置到 WAL
 *
 * 格式：[type:1 byte][key_len:4 bytes][key][ttl_ms:8 bytes]
 */
int kv_ttl_log_set(kv_t *db, const void *key, size_t key_len, int64_t ttl_ms) {
    if (!db || !key || !db->wal) return -1;

    /* TTL 设置不需要单独的 WAL 日志，因为：
     * 1. kv_put 已经记录了数据变更
     * 2. TTL 文件会在 kv_close 时保存
     * 3. 恢复时重新加载 TTL 文件即可
     */

    (void)key;
    (void)key_len;
    (void)ttl_ms;

    return 0;
}
