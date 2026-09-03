/**
 * @file lsm_tree.c
 * @brief LSM-Tree 存储引擎实现
 *
 * Phase12 - 实现 LSM-Tree 架构，追赶 RocksDB 水平。
 */

#include "db/storage/kv/lsm/lsm_tree.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
    #define mkdir(path, mode) mkdir(path)
#else
    #include <unistd.h>
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define LSM_MAGIC 0x4C534D54  /* "LSMT" */
#define LSM_VERSION 1
#define LSM_FILE_EXTENSION ".sst"
#define LSM_MAX_SSTABLE_SIZE (256 * 1024 * 1024)

/*** SSTable 文件格式 ***/
/*
 * Header (64 bytes):
 *   magic (4) + version (4) + min_key (8) + max_key (8) + min_seq (8) + max_seq (8)
 *   reserved (16) + bloom_size (4) + num_entries (4) + index_size (4) + data_size (4)
 * Data Section:
 *   entries... (key_size + value_size + op + seq)
 * Index Section:
 *   index entries... (key + offset)
 * Bloom Filter:
 *   bits...
 */

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 跳表节点 */
typedef struct skip_list_node {
    void *key;
    size_t key_size;
    void *value;
    size_t value_size;
    uint64_t seq;
    lsm_operation_t op;
    struct skip_list_node *next[];
} skip_list_node_t;

/** 跳表 */
typedef struct {
    skip_list_node_t *head;
    uint32_t max_level;
    uint32_t level;
    size_t size;
    pthread_mutex_t lock;
} skip_list_t;

/** MemTable */
typedef struct {
    skip_list_t *skiplist;
    size_t size;
    size_t max_size;
    uint64_t seq;
} memtable_t;

/** 层信息 */
typedef struct {
    uint32_t level;
    lsm_sstable_meta_t **tables;
    uint32_t table_count;
    size_t size;
    size_t size_limit;
} level_t;

/** LSM 树 */
struct lsm_tree {
    uint32_t magic;
    lsm_config_t config;
    lsm_state_t state;

    /* MemTable */
    memtable_t *memtable;

    /* 磁盘层 */
    level_t *levels;
    uint32_t num_levels;

    /* 元数据 */
    char data_dir[512];
    uint64_t next_file_id;
    pthread_mutex_t lock;

    /* 统计 */
    uint64_t total_size;
    uint64_t num_keys;
    uint64_t compaction_count;
    uint64_t flush_count;

    /* 缓存 */
    void *block_cache;
    size_t cache_size;
};

/** 迭代器 */
struct lsm_iterator {
    lsm_tree_t *tree;
    void *start_key;
    size_t start_size;
    void *end_key;
    size_t end_size;
    void *current_key;
    size_t current_key_size;
    void *current_value;
    size_t current_value_size;
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return 0;
    }
    return mkdir(path, 0755);
}

static uint64_t generate_file_id(void) {
    static uint64_t counter = 0;
    return (uint64_t)time(NULL) * 1000 + (counter++);
}

static int compare_keys(const void *a, size_t a_size,
                        const void *b, size_t b_size) {
    size_t min_size = a_size < b_size ? a_size : b_size;
    int cmp = memcmp(a, b, min_size);
    if (cmp != 0) return cmp;
    return (int)(a_size - b_size);
}

/* ========================================================================
 * 跳表实现
 * ======================================================================== */

#define SKIP_LIST_MAX_LEVEL 16

static skip_list_node_t *skip_list_node_create(const void *key, size_t key_size,
                                               const void *value, size_t value_size,
                                               uint64_t seq, lsm_operation_t op,
                                               uint32_t level) {
    skip_list_node_t *node = (skip_list_node_t *)malloc(
        sizeof(skip_list_node_t) + level * sizeof(skip_list_node_t *));
    if (!node) return NULL;

    node->key = malloc(key_size);
    if (!node->key) {
        free(node);
        return NULL;
    }
    memcpy(node->key, key, key_size);
    node->key_size = key_size;

    if (value && value_size > 0) {
        node->value = malloc(value_size);
        if (!node->value) {
            free(node->key);
            free(node);
            return NULL;
        }
        memcpy(node->value, value, value_size);
    } else {
        node->value = NULL;
    }
    node->value_size = value_size;
    node->seq = seq;
    node->op = op;

    return node;
}

static void skip_list_node_destroy(skip_list_node_t *node) {
    if (!node) return;
    free(node->key);
    free(node->value);
    free(node);
}

static skip_list_t *skip_list_create(void) {
    skip_list_t *sl = (skip_list_t *)calloc(1, sizeof(skip_list_t));
    if (!sl) return NULL;

    sl->max_level = SKIP_LIST_MAX_LEVEL;
    sl->head = (skip_list_node_t *)calloc(1, sizeof(skip_list_node_t) +
                                          SKIP_LIST_MAX_LEVEL * sizeof(skip_list_node_t *));
    if (!sl->head) {
        free(sl);
        return NULL;
    }

    pthread_mutex_init(&sl->lock, NULL);
    return sl;
}

static void skip_list_destroy(skip_list_t *sl) {
    if (!sl) return;

    skip_list_node_t *node = sl->head;
    while (node) {
        skip_list_node_t *next = node->next[0];
        skip_list_node_destroy(node);
        node = next;
    }

    pthread_mutex_destroy(&sl->lock);
    free(sl);
}

static int skip_list_put(skip_list_t *sl,
                         const void *key, size_t key_size,
                         const void *value, size_t value_size,
                         uint64_t seq, lsm_operation_t op) {
    pthread_mutex_lock(&sl->lock);

    /* 随机生成层数 */
    uint32_t level = 0;
    while (level < sl->max_level - 1 && (rand() % 2 == 0)) {
        level++;
    }

    /* 创建新节点 */
    skip_list_node_t *new_node = skip_list_node_create(key, key_size, value, value_size, seq, op, level + 1);
    if (!new_node) {
        pthread_mutex_unlock(&sl->lock);
        return -1;
    }

    /* 查找插入位置 */
    skip_list_node_t *update[SKIP_LIST_MAX_LEVEL];
    skip_list_node_t *current = sl->head;

    for (int i = sl->max_level - 1; i >= 0; i--) {
        while (current->next[i] &&
               compare_keys(current->next[i]->key, current->next[i]->key_size,
                          key, key_size) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    /* 删除已存在的相同键 */
    current = current->next[0];
    if (current && compare_keys(current->key, current->key_size, key, key_size) == 0) {
        for (int i = 0; i <= sl->max_level - 1; i++) {
            if (update[i]->next[i] != current) break;
            update[i]->next[i] = current->next[i];
        }
        skip_list_node_destroy(current);
    }

    /* 插入新节点 */
    for (uint32_t i = 0; i <= level; i++) {
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
    }

    sl->size++;
    pthread_mutex_unlock(&sl->lock);

    return 0;
}

static skip_list_node_t *skip_list_get(skip_list_t *sl,
                                       const void *key, size_t key_size) {
    pthread_mutex_lock(&sl->lock);

    skip_list_node_t *current = sl->head->next[0];
    while (current) {
        int cmp = compare_keys(current->key, current->key_size, key, key_size);
        if (cmp == 0) {
            pthread_mutex_unlock(&sl->lock);
            return current;
        }
        if (cmp > 0) {
            break;
        }
        current = current->next[0];
    }

    pthread_mutex_unlock(&sl->lock);
    return NULL;
}

static size_t skip_list_size(skip_list_t *sl) {
    pthread_mutex_lock(&sl->lock);
    size_t size = sl->size;
    pthread_mutex_unlock(&sl->lock);
    return size;
}

/* ========================================================================
 * 跳表迭代器
 * ======================================================================== */

typedef struct {
    skip_list_node_t *current;
    skip_list_t *list;
} skip_list_iter_t;

static skip_list_iter_t *skip_list_create_iter(skip_list_t *sl) {
    if (!sl) return NULL;
    skip_list_iter_t *iter = (skip_list_iter_t *)calloc(1, sizeof(skip_list_iter_t));
    if (!iter) return NULL;
    iter->list = sl;
    iter->current = sl->head->next[0];
    return iter;
}

static bool skip_list_iter_next(skip_list_iter_t *iter) {
    if (!iter || !iter->current) return false;
    iter->current = iter->current->next[0];
    return iter->current != NULL;
}

static void skip_list_iter_free(skip_list_iter_t *iter) {
    free(iter);
}

static void skip_list_clear(skip_list_t *sl) {
    if (!sl) return;
    skip_list_node_t *node = sl->head->next[0];
    while (node) {
        skip_list_node_t *next = node->next[0];
        skip_list_node_destroy(node);
        node = next;
    }
    sl->head->next[0] = NULL;
    sl->size = 0;
}

/* ========================================================================
 * MemTable 实现
 * ======================================================================== */

static memtable_t *memtable_create(size_t max_size) {
    memtable_t *mt = (memtable_t *)calloc(1, sizeof(memtable_t));
    if (!mt) return NULL;

    mt->skiplist = skip_list_create();
    if (!mt->skiplist) {
        free(mt);
        return NULL;
    }

    mt->max_size = max_size;
    mt->size = 0;
    mt->seq = 0;

    return mt;
}

static void memtable_destroy(memtable_t *mt) {
    if (!mt) return;
    if (mt->skiplist) {
        skip_list_destroy(mt->skiplist);
    }
    free(mt);
}

static int memtable_put(memtable_t *mt,
                        const void *key, size_t key_size,
                        const void *value, size_t value_size,
                        lsm_operation_t op) {
    if (!mt) return -1;

    int ret = skip_list_put(mt->skiplist, key, key_size, value, value_size, mt->seq, op);
    if (ret == 0) {
        mt->seq++;
        mt->size += key_size + value_size;
    }
    return ret;
}

static skip_list_node_t *memtable_get(memtable_t *mt,
                                       const void *key, size_t key_size) {
    if (!mt) return NULL;
    return skip_list_get(mt->skiplist, key, key_size);
}

static size_t memtable_size(memtable_t *mt) {
    return mt ? mt->size : 0;
}

static bool memtable_need_flush(memtable_t *mt) {
    return mt && mt->size >= mt->max_size;
}

static skip_list_node_t *memtable_find_le(memtable_t *mt,
                                         const void *key, size_t key_size) {
    if (!mt) return NULL;

    pthread_mutex_lock(&mt->skiplist->lock);
    skip_list_node_t *current = mt->skiplist->head->next[0];
    skip_list_node_t *result = NULL;

    while (current) {
        if (compare_keys(current->key, current->key_size, key, key_size) <= 0) {
            result = current;
        } else {
            break;
        }
        current = current->next[0];
    }

    pthread_mutex_unlock(&mt->skiplist->lock);
    return result;
}

/* ========================================================================
 * 布隆过滤器实现
 * ======================================================================== */

static uint64_t bloom_hash(const lsm_bloom_filter_t *bloom,
                           const void *key, size_t key_size,
                           uint32_t seed) {
    uint64_t h = seed;
    const uint8_t *data = (const uint8_t *)key;
    for (size_t i = 0; i < key_size; i++) {
        h = h * 31 + data[i];
    }
    return h % (bloom->size * 8);
}

lsm_bloom_filter_t *lsm_bloom_create(size_t size, size_t num_hashes) {
    lsm_bloom_filter_t *bloom = (lsm_bloom_filter_t *)calloc(1, sizeof(lsm_bloom_filter_t));
    if (!bloom) return NULL;

    bloom->size = size;
    bloom->num_hashes = num_hashes;
    bloom->bits = (uint8_t *)calloc(1, size);
    if (!bloom->bits) {
        free(bloom);
        return NULL;
    }

    return bloom;
}

void lsm_bloom_add(lsm_bloom_filter_t *bloom,
                   const void *key, size_t key_size) {
    if (!bloom || !key) return;

    for (size_t i = 0; i < bloom->num_hashes; i++) {
        uint64_t pos = bloom_hash(bloom, key, key_size, i * 0x9e3779b9);
        bloom->bits[pos / 8] |= (1 << (pos % 8));
    }
}

bool lsm_bloom_might_contain(const lsm_bloom_filter_t *bloom,
                             const void *key, size_t key_size) {
    if (!bloom || !key) return false;

    for (size_t i = 0; i < bloom->num_hashes; i++) {
        uint64_t pos = bloom_hash(bloom, key, key_size, i * 0x9e3779b9);
        if (!(bloom->bits[pos / 8] & (1 << (pos % 8)))) {
            return false;
        }
    }
    return true;
}

void lsm_bloom_destroy(lsm_bloom_filter_t *bloom) {
    if (!bloom) return;
    free(bloom->bits);
    free(bloom);
}

int lsm_bloom_serialize(const lsm_bloom_filter_t *bloom,
                         void **out_data, size_t *out_size) {
    if (!bloom || !out_data || !out_size) return -1;

    *out_size = sizeof(size_t) + sizeof(size_t) + bloom->size;
    *out_data = malloc(*out_size);
    if (!*out_data) return -1;

    uint8_t *ptr = (uint8_t *)*out_data;
    memcpy(ptr, &bloom->size, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(ptr, &bloom->num_hashes, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(ptr, bloom->bits, bloom->size);

    return 0;
}

lsm_bloom_filter_t *lsm_bloom_deserialize(const void *data, size_t size) {
    if (!data) return NULL;

    const uint8_t *ptr = (const uint8_t *)data;
    size_t bloom_size, num_hashes;
    memcpy(&bloom_size, ptr, sizeof(size_t));
    ptr += sizeof(size_t);
    memcpy(&num_hashes, ptr, sizeof(size_t));
    ptr += sizeof(size_t);

    lsm_bloom_filter_t *bloom = lsm_bloom_create(bloom_size, num_hashes);
    if (!bloom) return NULL;

    memcpy(bloom->bits, ptr, bloom_size);
    return bloom;
}

/* ========================================================================
 * LSM 树公共 API 实现
 * ======================================================================== */

lsm_tree_t *lsm_tree_create(const lsm_config_t *config) {
    if (!config) return NULL;

    lsm_tree_t *tree = (lsm_tree_t *)calloc(1, sizeof(lsm_tree_t));
    if (!tree) return NULL;

    tree->magic = LSM_MAGIC;
    tree->config = *config;
    tree->state = LSM_STATE_NORMAL;

    /* 初始化 MemTable */
    tree->memtable = memtable_create(config->memtable_size);
    if (!tree->memtable) {
        free(tree);
        return NULL;
    }

    /* 初始化层 */
    tree->num_levels = config->num_levels > 0 ? config->num_levels : LSM_MAX_LEVELS;
    tree->levels = (level_t *)calloc(tree->num_levels, sizeof(level_t));
    if (!tree->levels) {
        memtable_destroy(tree->memtable);
        free(tree);
        return NULL;
    }

    /* 计算每层大小限制 */
    size_t size_limit = config->sstable_size;
    for (uint32_t i = 0; i < tree->num_levels; i++) {
        tree->levels[i].level = i;
        tree->levels[i].size_limit = size_limit;
        size_limit *= config->size_ratio > 0 ? config->size_ratio : LSM_SIZE_RATIO;
    }

    /* 创建数据目录 */
    if (config->data_dir) {
        strncpy(tree->data_dir, config->data_dir, sizeof(tree->data_dir) - 1);
        ensure_dir(config->data_dir);
    }

    pthread_mutex_init(&tree->lock, NULL);

    return tree;
}

lsm_tree_t *lsm_tree_open(const char *data_dir) {
    lsm_config_t config = {
        .data_dir = data_dir,
        .memtable_size = LSM_DEFAULT_MEMTABLE_SIZE,
        .sstable_size = LSM_DEFAULT_SSTABLE_SIZE,
        .num_levels = LSM_MAX_LEVELS,
        .size_ratio = LSM_SIZE_RATIO,
        .enable_bloom_filter = true,
        .enable_cache = true,
        .cache_size = 64 * 1024 * 1024
    };
    return lsm_tree_create(&config);
}

void lsm_tree_close(lsm_tree_t *tree) {
    if (!tree) return;

    /* 刷 MemTable */
    if (tree->memtable && memtable_size(tree->memtable) > 0) {
        lsm_tree_flush(tree);
    }

    /* 清理资源 */
    if (tree->memtable) {
        memtable_destroy(tree->memtable);
    }

    if (tree->levels) {
        /* TODO: 清理 SSTable 元数据 */
        free(tree->levels);
    }

    pthread_mutex_destroy(&tree->lock);
    tree->magic = 0;
    free(tree);
}

const lsm_config_t *lsm_tree_get_config(const lsm_tree_t *tree) {
    return tree ? &tree->config : NULL;
}

lsm_state_t lsm_tree_get_state(const lsm_tree_t *tree) {
    return tree ? tree->state : LSM_STATE_ERROR;
}

/* ========================================================================
 * 写入操作
 * ======================================================================== */

int lsm_tree_put(lsm_tree_t *tree,
                 const void *key, size_t key_size,
                 const void *value, size_t value_size) {
    if (!tree || !key) return -1;

    pthread_mutex_lock(&tree->lock);

    int ret = memtable_put(tree->memtable, key, key_size, value, value_size, LSM_OP_PUT);
    if (ret == 0) {
        tree->num_keys++;
    }

    /* 检查是否需要刷盘 */
    if (memtable_need_flush(tree->memtable)) {
        /* TODO: 触发后台刷盘 */
    }

    pthread_mutex_unlock(&tree->lock);
    return ret;
}

int lsm_tree_delete(lsm_tree_t *tree,
                   const void *key, size_t key_size) {
    if (!tree || !key) return -1;

    pthread_mutex_lock(&tree->lock);

    int ret = memtable_put(tree->memtable, key, key_size, NULL, 0, LSM_OP_DELETE);

    pthread_mutex_unlock(&tree->lock);
    return ret;
}

size_t lsm_tree_batch_put(lsm_tree_t *tree,
                          const lsm_entry_t *entries,
                          size_t count) {
    if (!tree || !entries) return 0;

    size_t written = 0;
    pthread_mutex_lock(&tree->lock);

    for (size_t i = 0; i < count; i++) {
        int ret = memtable_put(tree->memtable,
                             entries[i].key, entries[i].key_size,
                             entries[i].value, entries[i].value_size,
                             entries[i].op);
        if (ret == 0) {
            written++;
            tree->num_keys++;
        }
    }

    pthread_mutex_unlock(&tree->lock);
    return written;
}

int lsm_tree_flush(lsm_tree_t *tree) {
    if (!tree) return -1;

    pthread_mutex_lock(&tree->lock);

    if (!tree->memtable || memtable_size(tree->memtable) == 0) {
        pthread_mutex_unlock(&tree->lock);
        return 0;
    }

    /* 生成 SSTable 文件路径 */
    char sst_path[512];
    uint64_t file_id = generate_file_id();
    snprintf(sst_path, sizeof(sst_path), "%s/%llu.sst",
             tree->data_dir[0] ? tree->data_dir : ".", (unsigned long long)file_id);

    /* 创建 SSTable 并写入 MemTable 数据 */
    FILE *fp = fopen(sst_path, "wb");
    if (!fp) {
        pthread_mutex_unlock(&tree->lock);
        return -1;
    }

    /* 写入 SSTable 头 */
    uint32_t magic = LSM_MAGIC;
    uint32_t version = LSM_VERSION;
    uint32_t num_entries = (uint32_t)skip_list_size(tree->memtable);
    fwrite(&magic, sizeof(uint32_t), 1, fp);
    fwrite(&version, sizeof(uint32_t), 1, fp);
    fwrite(&num_entries, sizeof(uint32_t), 1, fp);

    /* 写入所有条目 */
    skip_list_iter_t *iter = skip_list_create_iter(tree->memtable->skiplist);
    while (skip_list_iter_next(iter)) {
        skip_list_node_t *node = iter->current;
        uint32_t klen = (uint32_t)node->key_size;
        uint32_t vlen = (uint32_t)node->value_size;
        uint8_t op = (uint8_t)node->op;
        fwrite(&klen, sizeof(uint32_t), 1, fp);
        fwrite(node->key, 1, klen, fp);
        fwrite(&vlen, sizeof(uint32_t), 1, fp);
        if (vlen > 0 && node->value) {
            fwrite(node->value, 1, vlen, fp);
        }
        fwrite(&op, sizeof(uint8_t), 1, fp);
        fwrite(&node->seq, sizeof(uint64_t), 1, fp);
    }
    skip_list_iter_free(iter);

    fclose(fp);

    /* 重置 MemTable */
    skip_list_clear(tree->memtable->skiplist);
    tree->flush_count++;

    pthread_mutex_unlock(&tree->lock);
    return 0;
}

/* ========================================================================
 * 读取操作
 * ======================================================================== */

int lsm_tree_get(lsm_tree_t *tree,
                 const void *key, size_t key_size,
                 void **out_value, size_t *out_size) {
    if (!tree || !key) return -1;

    pthread_mutex_lock(&tree->lock);

    /* 先查 MemTable */
    skip_list_node_t *node = memtable_get(tree->memtable, key, key_size);
    if (node) {
        if (node->op == LSM_OP_DELETE) {
            pthread_mutex_unlock(&tree->lock);
            return 1; /* 已删除 */
        }
        if (out_value && out_size) {
            *out_value = malloc(node->value_size);
            if (*out_value) {
                memcpy(*out_value, node->value, node->value_size);
                *out_size = node->value_size;
            }
        }
        pthread_mutex_unlock(&tree->lock);
        return 0;
    }

    /* TODO: 查 SSTable */

    pthread_mutex_unlock(&tree->lock);
    return 1; /* 未找到 */
}

bool lsm_tree_exists(lsm_tree_t *tree,
                    const void *key, size_t key_size) {
    return lsm_tree_get(tree, key, key_size, NULL, NULL) == 0;
}

/* ========================================================================
 * 迭代器
 * ======================================================================== */

lsm_iterator_t *lsm_tree_create_iterator(lsm_tree_t *tree,
                                          const void *start_key, size_t start_size,
                                          const void *end_key, size_t end_size) {
    if (!tree) return NULL;

    lsm_iterator_t *iter = (lsm_iterator_t *)calloc(1, sizeof(lsm_iterator_t));
    if (!iter) return NULL;

    iter->tree = tree;
    iter->start_key = (void *)start_key;
    iter->start_size = start_size;
    iter->end_key = (void *)end_key;
    iter->end_size = end_size;

    return iter;
}

int lsm_iterator_next(lsm_iterator_t *iter) {
    /* TODO: 实现迭代器 */
    return 1;
}

const void *lsm_iterator_key(const lsm_iterator_t *iter) {
    return iter ? iter->current_key : NULL;
}

size_t lsm_iterator_key_size(const lsm_iterator_t *iter) {
    return iter ? iter->current_key_size : 0;
}

const void *lsm_iterator_value(const lsm_iterator_t *iter) {
    return iter ? iter->current_value : NULL;
}

size_t lsm_iterator_value_size(const lsm_iterator_t *iter) {
    return iter ? iter->current_value_size : 0;
}

void lsm_iterator_destroy(lsm_iterator_t *iter) {
    free(iter);
}

/* ========================================================================
 * Compaction
 * ======================================================================== */

/* ========================================================================
 * Compaction
 * ======================================================================== */

int lsm_tree_compact(lsm_tree_t *tree, uint32_t level) {
    if (!tree) return -1;

    pthread_mutex_lock(&tree->lock);

    tree->state = LSM_STATE_COMPACTING;

    /* 统计所有层的 SSTable 数量 */
    uint32_t total_tables = 0;
    for (uint32_t i = 0; i < tree->num_levels; i++) {
        total_tables += tree->levels[i].table_count;
    }

    if (total_tables == 0) {
        tree->state = LSM_STATE_NORMAL;
        pthread_mutex_unlock(&tree->lock);
        return 0;
    }

    /* 生成新的合并后 SSTable 文件路径 */
    char sst_path[512];
    uint64_t file_id = generate_file_id();
    snprintf(sst_path, sizeof(sst_path), "%s/%llu_compact.sst",
             tree->data_dir[0] ? tree->data_dir : ".", (unsigned long long)file_id);

    /* 创建合并后的 SSTable */
    FILE *fp = fopen(sst_path, "wb");
    if (!fp) {
        tree->state = LSM_STATE_ERROR;
        pthread_mutex_unlock(&tree->lock);
        return -1;
    }

    /* 写入 SSTable 头 */
    uint32_t magic = LSM_MAGIC;
    uint32_t version = LSM_VERSION;
    uint32_t num_entries = 0;

    /* 简单起见，收集所有层的节点并排序（实际生产用 k-way merge） */
    typedef struct {
        void *key;
        size_t key_size;
        void *value;
        size_t value_size;
        uint64_t seq;
        uint8_t op;
    } entry_t;

    entry_t *entries = NULL;
    size_t entry_capacity = 0;
    size_t entry_count = 0;

    /* 从各层收集数据 */
    for (uint32_t i = 0; i < tree->num_levels; i++) {
        for (uint32_t j = 0; j < tree->levels[i].table_count; j++) {
            /* 这里应该读取 SSTable 文件内容，简化处理为跳过 */
            (void)i; (void)j;
        }
    }

    /* 如果没有收集到条目，直接创建空 SSTable */
    if (entry_count == 0) {
        fwrite(&magic, sizeof(uint32_t), 1, fp);
        fwrite(&version, sizeof(uint32_t), 1, fp);
        fwrite(&num_entries, sizeof(uint32_t), 1, fp);
    }

    /* 释放收集的条目 */
    for (size_t e = 0; e < entry_count; e++) {
        free(entries[e].key);
        free(entries[e].value);
    }
    free(entries);

    fclose(fp);

    tree->state = LSM_STATE_NORMAL;
    tree->compaction_count++;

    pthread_mutex_unlock(&tree->lock);
    return 0;
}

bool lsm_tree_needs_compaction(const lsm_tree_t *tree) {
    if (!tree) return false;
    return tree->levels[0].table_count > 4;
}

uint32_t lsm_tree_get_level_count(const lsm_tree_t *tree, uint32_t level) {
    if (!tree || level >= tree->num_levels) return 0;
    return tree->levels[level].table_count;
}

size_t lsm_tree_get_total_size(const lsm_tree_t *tree) {
    return tree ? tree->total_size : 0;
}

uint64_t lsm_tree_get_num_keys(const lsm_tree_t *tree) {
    return tree ? tree->num_keys : 0;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

void lsm_tree_get_stats(const lsm_tree_t *tree, lsm_stats_t *stats) {
    if (!tree || !stats) return;

    memset(stats, 0, sizeof(lsm_stats_t));

    stats->num_keys = tree->num_keys;
    stats->memtable_size = memtable_size(tree->memtable);
    stats->total_size = tree->total_size;
    stats->num_levels = tree->num_levels;
    stats->compaction_count = tree->compaction_count;
    stats->flush_count = tree->flush_count;

    for (uint32_t i = 0; i < tree->num_levels; i++) {
        stats->num_sstables += tree->levels[i].table_count;
        if (i == 0) {
            stats->num_l0_sstables = tree->levels[i].table_count;
        }
    }
}
