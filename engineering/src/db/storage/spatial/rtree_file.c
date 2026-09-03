/**
 * @file rtree_file.c
 * @brief R-Tree 持久化存储实现
 */
#include "db/storage/spatial/rtree_file.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

static uint32_t cache_hash(rtree_file_t *file, int32_t page_id) {
    return ((uint32_t)page_id) % RTREE_CACHE_MAX_PAGES;
}

static void lru_move_to_head(rtree_file_t *file, rtree_cache_entry_t *entry) {
    if (entry == file->lru_head) return;

    /* 从原位置移除 */
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    if (entry == file->lru_tail) file->lru_tail = entry->prev;

    /* 移到头部 */
    entry->next = file->lru_head;
    entry->prev = NULL;
    if (file->lru_head) file->lru_head->prev = entry;
    file->lru_head = entry;
    if (file->lru_tail == NULL) file->lru_tail = entry;
}

static rtree_cache_entry_t *cache_lookup(rtree_file_t *file, int32_t page_id) {
    uint32_t h = cache_hash(file, page_id);
    rtree_cache_entry_t *entry = &file->cache[h];

    while (entry && entry->data != NULL) {
        if (entry->page_id == page_id) {
            file->cache_hits++;
            lru_move_to_head(file, entry);
            return entry;
        }
        entry = entry->next;
    }

    file->cache_misses++;
    return NULL;
}

static void cache_evict(rtree_file_t *file) {
    if (file->lru_tail == NULL) return;

    rtree_cache_entry_t *entry = file->lru_tail;
    if (entry->dirty && file->fp) {
        /* 刷盘 */
        long offset = (long)(entry->page_id + 1) * file->header.page_size;
        fseek(file->fp, offset, SEEK_SET);
        fwrite(entry->data, file->header.page_size, 1, file->fp);
    }

    /* 从链表中移除 */
    if (entry->prev) entry->prev->next = NULL;
    file->lru_tail = entry->prev;
    if (file->lru_tail == NULL) file->lru_head = NULL;

    /* 释放数据 */
    free(entry->data);
    entry->data = NULL;
    entry->page_id = -1;
    entry->dirty = false;
}

static int cache_add(rtree_file_t *file, int32_t page_id, const void *data) {
    /* 如果缓存已满，先驱逐 */
    while (file->cache_size >= RTREE_CACHE_MAX_PAGES) {
        cache_evict(file);
    }

    uint32_t h = cache_hash(file, page_id);
    rtree_cache_entry_t *entry = &file->cache[h];

    /* 找到空槽 */
    while (entry->data != NULL) {
        if (entry->next == NULL) {
            entry->next = (rtree_cache_entry_t *)calloc(1, sizeof(rtree_cache_entry_t));
            entry = entry->next;
        } else {
            entry = entry->next;
        }
    }

    entry->page_id = page_id;
    entry->data = malloc(file->header.page_size);
    if (entry->data == NULL) return -1;
    memcpy(entry->data, data, file->header.page_size);
    entry->dirty = false;

    /* 移到 LRU 头部 */
    lru_move_to_head(file, entry);
    file->cache_size++;

    return 0;
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

rtree_file_t *rtree_file_create(const char *path, uint32_t page_size) {
    if (path == NULL) return NULL;
    if (page_size == 0) page_size = RTREE_PAGE_SIZE;

    rtree_file_t *file = (rtree_file_t *)calloc(1, sizeof(rtree_file_t));
    if (file == NULL) return NULL;

    strncpy(file->path, path, sizeof(file->path) - 1);
    file->header.page_size = page_size;
    file->header.magic = RTREE_FILE_MAGIC;
    file->header.version = RTREE_FILE_VERSION;
    file->header.root_offset = 0;
    file->header.root_level = 0;
    file->header.node_count = 0;
    file->header.item_count = 0;

    /* 分配缓存 */
    file->cache = (rtree_cache_entry_t *)calloc(RTREE_CACHE_MAX_PAGES,
                                                 sizeof(rtree_cache_entry_t));
    if (file->cache == NULL) {
        free(file);
        return NULL;
    }

    /* 创建 R-Tree */
    file->tree = rtree_create(16);  /* 默认 max_entries=16 */
    if (file->tree == NULL) {
        free(file->cache);
        free(file);
        return NULL;
    }

    /* 打开文件 */
    file->fp = fopen(path, "w+b");
    if (file->fp == NULL) {
        rtree_free(file->tree);
        free(file->cache);
        free(file);
        return NULL;
    }

    /* 写入文件头 */
    fwrite(&file->header, sizeof(rtree_file_header_t), 1, file->fp);

    LOG_INFO("R-Tree 文件创建成功: %s, page_size=%u", path, page_size);
    return file;
}

rtree_file_t *rtree_file_open(const char *path) {
    if (path == NULL) return NULL;

    FILE *fp = fopen(path, "r+b");
    if (fp == NULL) {
        LOG_WARN("R-Tree 文件不存在: %s", path);
        return NULL;
    }

    rtree_file_t *file = (rtree_file_t *)calloc(1, sizeof(rtree_file_t));
    if (file == NULL) {
        fclose(fp);
        return NULL;
    }

    strncpy(file->path, path, sizeof(file->path) - 1);
    file->fp = fp;

    /* 读取文件头 */
    if (fread(&file->header, sizeof(rtree_file_header_t), 1, fp) != 1) {
        fclose(fp);
        free(file);
        return NULL;
    }

    if (file->header.magic != RTREE_FILE_MAGIC ||
        file->header.version != RTREE_FILE_VERSION) {
        LOG_ERROR("R-Tree 文件格式无效");
        fclose(fp);
        free(file);
        return NULL;
    }

    /* 分配缓存 */
    file->cache = (rtree_cache_entry_t *)calloc(RTREE_CACHE_MAX_PAGES,
                                                 sizeof(rtree_cache_entry_t));

    /* 创建 R-Tree */
    file->tree = rtree_create(16);
    if (file->tree == NULL) {
        free(file->cache);
        fclose(fp);
        free(file);
        return NULL;
    }

    /* TODO: 从文件加载 R-Tree 结构 */

    LOG_INFO("R-Tree 文件打开成功: %s, items=%u", path, file->header.item_count);
    return file;
}

int rtree_file_close(rtree_file_t *file) {
    if (file == NULL) return -1;

    /* 刷缓存 */
    rtree_cache_entry_t *entry = file->lru_head;
    while (entry) {
        if (entry->dirty) {
            /* TODO: 实现页面刷盘 */
        }
        entry = entry->next;
    }

    /* 更新文件头 */
    fseek(file->fp, 0, SEEK_SET);
    if (file->tree) {
        rtree_stats_t stats;
        rtree_stats(file->tree, &stats);
        file->header.node_count = stats.num_nodes;
        file->header.item_count = stats.num_items;
    }
    fwrite(&file->header, sizeof(rtree_file_header_t), 1, file->fp);

    fclose(file->fp);
    file->fp = NULL;

    LOG_INFO("R-Tree 文件关闭: %s, nodes=%u, items=%u",
             file->path, file->header.node_count, file->header.item_count);
    return 0;
}

void rtree_file_free(rtree_file_t *file) {
    if (file == NULL) return;

    if (file->fp) {
        rtree_file_close(file);
    }

    if (file->tree) {
        rtree_free(file->tree);
    }

    if (file->cache) {
        rtree_cache_entry_t *entry = file->lru_head;
        while (entry) {
            rtree_cache_entry_t *next = entry->next;
            if (entry->data) free(entry->data);
            if (entry != &file->cache[cache_hash(file, entry->page_id)]) {
                free(entry);
            }
            entry = next;
        }
        free(file->cache);
    }

    free(file);
}

/* ========================================================================
 * 操作 API 实现
 * ======================================================================== */

rtree_t *rtree_file_get_tree(rtree_file_t *file) {
    return file ? file->tree : NULL;
}

int rtree_file_insert(rtree_file_t *file, uint64_t id, const bbox_t *bbox) {
    if (file == NULL || file->tree == NULL) return -1;
    return rtree_insert(file->tree, id, bbox);
}

int rtree_file_remove(rtree_file_t *file, uint64_t id, const bbox_t *bbox) {
    if (file == NULL || file->tree == NULL) return -1;
    return rtree_remove(file->tree, id, bbox);
}

int rtree_file_search(rtree_file_t *file, const bbox_t *query,
                      rtree_result_t *results, int max_results) {
    if (file == NULL || file->tree == NULL) return 0;
    return rtree_search(file->tree, query, results, max_results);
}

int rtree_file_knn(rtree_file_t *file, const point_t *point, int k,
                   rtree_result_t *results, int max_results) {
    if (file == NULL || file->tree == NULL) return 0;
    return rtree_knn(file->tree, point, k, results, max_results);
}

/* ========================================================================
 * 统计 API 实现
 * ======================================================================== */

void rtree_file_get_stats(rtree_file_t *file,
                          uint32_t *out_nodes, uint32_t *out_items,
                          uint32_t *out_cache_hits, uint32_t *out_cache_misses) {
    if (file == NULL) return;

    if (out_nodes) *out_nodes = file->header.node_count;
    if (out_items) *out_items = file->header.item_count;
    if (out_cache_hits) *out_cache_hits = file->cache_hits;
    if (out_cache_misses) *out_cache_misses = file->cache_misses;
}

/* ========================================================================
 * 文件完整性校验 API 实现
 * ======================================================================== */

static const char *g_corruption_reason = NULL;

const char *rtree_file_get_corruption_reason(const char *path) {
    (void)path;
    return g_corruption_reason ? g_corruption_reason : "Unknown error";
}

static void set_corruption_reason(const char *reason) {
    static char reason_buf[256];
    strncpy(reason_buf, reason, sizeof(reason_buf) - 1);
    reason_buf[sizeof(reason_buf) - 1] = '\0';
    g_corruption_reason = reason_buf;
}

bool rtree_file_verify(const char *path) {
    if (path == NULL) {
        set_corruption_reason("Path is NULL");
        return false;
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        set_corruption_reason("Cannot open file");
        return false;
    }

    /* 检查文件大小 */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < (long)sizeof(rtree_file_header_t)) {
        fclose(fp);
        set_corruption_reason("File too small for header");
        return false;
    }

    /* 读取并验证魔数 */
    rtree_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        set_corruption_reason("Cannot read header");
        return false;
    }

    if (header.magic != RTREE_FILE_MAGIC) {
        fclose(fp);
        set_corruption_reason("Invalid magic number");
        return false;
    }

    if (header.version != RTREE_FILE_VERSION) {
        fclose(fp);
        set_corruption_reason("Unsupported version");
        return false;
    }

    if (header.page_size == 0 || header.page_size > 65536) {
        fclose(fp);
        set_corruption_reason("Invalid page size");
        return false;
    }

    /* 验证文件大小是否合理 */
    if (file_size < (long)(sizeof(rtree_file_header_t) + header.page_size)) {
        fclose(fp);
        set_corruption_reason("File too small for minimum content");
        return false;
    }

    /* 验证节点偏移是否合理 */
    if (header.root_offset > (uint64_t)file_size) {
        fclose(fp);
        set_corruption_reason("Invalid root node offset");
        return false;
    }

    /* 验证节点数合理性 */
    if (header.node_count > 10000000) {  /* 1000 万个节点上限 */
        fclose(fp);
        set_corruption_reason("Suspiciously high node count");
        return false;
    }

    fclose(fp);
    g_corruption_reason = NULL;
    return true;
}

int rtree_file_repair(const char *path, const char *backup_path) {
    if (path == NULL) return -1;

    /* 如果提供了备份路径，先备份文件 */
    if (backup_path != NULL) {
        FILE *src = fopen(path, "rb");
        if (src == NULL) return -1;

        FILE *dst = fopen(backup_path, "wb");
        if (dst == NULL) {
            fclose(src);
            return -1;
        }

        /* 复制文件 */
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
            fwrite(buf, 1, n, dst);
        }

        fclose(src);
        fclose(dst);
    }

    /* 打开文件进行修复检查 */
    FILE *fp = fopen(path, "r+b");
    if (fp == NULL) return -1;

    /* 读取文件头 */
    rtree_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    int modified = 0;

    /* 修复魔数（如果损坏） */
    if (header.magic != RTREE_FILE_MAGIC) {
        header.magic = RTREE_FILE_MAGIC;
        modified = 1;
    }

    /* 修复版本（如果损坏） */
    if (header.version != RTREE_FILE_VERSION) {
        header.version = RTREE_FILE_VERSION;
        modified = 1;
    }

    /* 修复页面大小（如果为 0） */
    if (header.page_size == 0) {
        header.page_size = RTREE_PAGE_SIZE;
        modified = 1;
    }

    /* 写回修复后的头 */
    if (modified) {
        fseek(fp, 0, SEEK_SET);
        fwrite(&header, sizeof(header), 1, fp);
    }

    fclose(fp);
    return modified ? 0 : 1;  /* 0 = 修复成功，1 = 无需修复 */
}
