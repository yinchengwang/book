/**
 * @file ts_tag_index.c
 * @brief 时序数据 Tag 倒排索引实现
 */
#include "ts_tag_index.h"
#include <stdlib.h>
#include <string.h>
#include <uthash/uthash.h>

/* ========================================================================
 * Tag 集合
 * ======================================================================== */

struct TagSet_s {
    Tag *tags;
    uint32_t count;
    uint32_t capacity;
};

TagSet *tagset_create(uint32_t capacity) {
    TagSet *set = (TagSet *)calloc(1, sizeof(TagSet));
    if (!set) return NULL;

    set->capacity = capacity > 0 ? capacity : 16;
    set->tags = (Tag *)calloc(set->capacity, sizeof(Tag));
    if (!set->tags) {
        free(set);
        return NULL;
    }

    return set;
}

static int tagset_ensure_capacity(TagSet *set) {
    if (set->count < set->capacity) return 0;

    set->capacity *= 2;
    Tag *new_tags = (Tag *)realloc(set->tags, set->capacity * sizeof(Tag));
    if (!new_tags) return -1;
    set->tags = new_tags;
    return 0;
}

int tagset_add(TagSet *set, const char *key, const char *str_value) {
    if (!set || !key) return -1;

    if (tagset_ensure_capacity(set) != 0) return -1;

    Tag *tag = &set->tags[set->count];
    strncpy(tag->key, key, sizeof(tag->key) - 1);
    tag->value_type = TAG_STRING;
    tag->value.str_val.str = strdup(str_value);
    tag->value.str_val.len = strlen(str_value);
    set->count++;

    return 0;
}

int tagset_add_int(TagSet *set, const char *key, int64_t int_value) {
    if (!set || !key) return -1;

    if (tagset_ensure_capacity(set) != 0) return -1;

    Tag *tag = &set->tags[set->count];
    strncpy(tag->key, key, sizeof(tag->key) - 1);
    tag->value_type = TAG_INT;
    tag->value.int_val = int_value;
    set->count++;

    return 0;
}

int tagset_add_float(TagSet *set, const char *key, double float_value) {
    if (!set || !key) return -1;

    if (tagset_ensure_capacity(set) != 0) return -1;

    Tag *tag = &set->tags[set->count];
    strncpy(tag->key, key, sizeof(tag->key) - 1);
    tag->value_type = TAG_FLOAT;
    tag->value.float_val = float_value;
    set->count++;

    return 0;
}

const Tag *tagset_get(const TagSet *set, const char *key) {
    if (!set || !key) return NULL;

    for (uint32_t i = 0; i < set->count; i++) {
        if (strcmp(set->tags[i].key, key) == 0) {
            return &set->tags[i];
        }
    }

    return NULL;
}

bool tagset_contains(const TagSet *set, const char *key) {
    return tagset_get(set, key) != NULL;
}

void tagset_free(TagSet *set) {
    if (!set) return;

    for (uint32_t i = 0; i < set->count; i++) {
        if (set->tags[i].value_type == TAG_STRING) {
            free(set->tags[i].value.str_val.str);
        }
    }

    free(set->tags);
    free(set);
}

/* ========================================================================
 * 倒排索引内部结构
 * ======================================================================== */

/* 使用 UTHash 实现简单的内存倒排索引 */

typedef struct IndexEntry_s {
    char key[64];              /**< Tag 键 */
    UT_hash_handle hh;         /**< 哈希句柄 */
} IndexKeyEntry;

typedef struct IndexValueEntry_s {
    char value[256];           /**< Tag 值（字符串化） */
    int64_t *series_ids;       /**< 匹配的序列 ID */
    uint32_t count;            /**< 数量 */
    uint32_t capacity;         /**< 容量 */
    UT_hash_handle hh;         /**< 哈希句柄 */
} IndexValueEntry;

struct TagIndex_s {
    char data_dir[512];
    void *index_root;          /**< 索引根节点：IndexKeyEntry */
    uint64_t total_series;
    uint64_t total_tags;
    uint64_t index_size;
    uint64_t query_count;
    uint32_t num_keys;
};

/* ========================================================================
 * 倒排索引实现
 * ======================================================================== */

TagIndex *tag_index_create(const char *data_dir) {
    TagIndex *index = (TagIndex *)calloc(1, sizeof(TagIndex));
    if (!index) return NULL;

    if (data_dir) {
        strncpy(index->data_dir, data_dir, sizeof(index->data_dir) - 1);
    }

    index->index_root = NULL;
    return index;
}

TagIndex *tag_index_open(const char *data_dir) {
    /* 简化实现：返回 NULL，实际应该从文件加载 */
    (void)data_dir;
    return NULL;
}

int tag_index_save(TagIndex *index) {
    if (!index) return -1;
    /* 简化实现：返回成功 */
    return 0;
}

void tag_index_destroy(TagIndex *index) {
    if (!index) return;

    /* 释放所有哈希表 */
    IndexKeyEntry *key_entry, *key_tmp;
    HASH_ITER(hh, (IndexKeyEntry *)index->index_root, key_entry, key_tmp) {
        /* 释放值哈希表 */
        /* 简化：直接释放 */
        HASH_DEL((IndexKeyEntry *)index->index_root, key_entry);
        free(key_entry);
    }

    free(index);
}

int tag_index_register_series(TagIndex *index, int64_t series_id, const TagSet *tags) {
    if (!index || !tags) return -1;

    for (uint32_t i = 0; i < tags->count; i++) {
        const Tag *tag = &tags->tags[i];

        /* 查找或创建键条目 */
        IndexKeyEntry *key_entry = NULL;
        char key_lower[64];
        strncpy(key_lower, tag->key, sizeof(key_lower) - 1);

        HASH_FIND_STR((IndexKeyEntry *)index->index_root, key_lower, key_entry);

        if (!key_entry) {
            key_entry = (IndexKeyEntry *)calloc(1, sizeof(IndexKeyEntry));
            if (!key_entry) continue;
            strncpy(key_entry->key, key_lower, sizeof(key_entry->key) - 1);
            HASH_ADD_STR(index->index_root, key, key_entry);
            index->num_keys++;
        }

        index->total_tags++;
    }

    index->total_series++;
    return 0;
}

int tag_index_unregister_series(TagIndex *index, int64_t series_id) {
    if (!index) return -1;
    /* 简化实现 */
    index->total_series--;
    return 0;
}

int tag_index_update_series(TagIndex *index, int64_t series_id, const TagSet *new_tags) {
    if (!index) return -1;
    /* 简化实现：先注销再注册 */
    tag_index_unregister_series(index, series_id);
    return tag_index_register_series(index, series_id, new_tags);
}

int tag_index_record_point(TagIndex *index, int64_t series_id, int64_t timestamp) {
    (void)index;
    (void)series_id;
    (void)timestamp;
    /* 简化实现：不需要额外操作 */
    return 0;
}

int tag_index_record_points_batch(TagIndex *index,
                                  const int64_t *series_ids,
                                  const int64_t *timestamps,
                                  uint32_t count) {
    if (!index || !series_ids) return -1;

    for (uint32_t i = 0; i < count; i++) {
        tag_index_record_point(index, series_ids[i],
                               timestamps ? timestamps[i] : 0);
    }

    return 0;
}

/* ========================================================================
 * 查询操作
 * ======================================================================== */

TagQuery *tag_query_create(const char *key, TagQueryOp op) {
    if (!key) return NULL;

    TagQuery *query = (TagQuery *)calloc(1, sizeof(TagQuery));
    if (!query) return NULL;

    strncpy(query->key, key, sizeof(query->key) - 1);
    query->op = op;
    query->value_type = TAG_STRING;

    return query;
}

int tag_query_add_in_value(TagQuery *query, const char *value) {
    if (!query || !value) return -1;

    /* 简化实现 */
    return 0;
}

void tag_query_free(TagQuery *query) {
    if (query) {
        if (query->in_values) {
            for (uint32_t i = 0; i < query->in_count; i++) {
                free(query->in_values[i]);
            }
            free(query->in_values);
        }
        free(query->regex_pattern);
        free(query);
    }
}

int tag_index_query(const TagIndex *index, const TagQuery *query, TagQueryResult *result) {
    if (!index || !query || !result) return -1;

    /* 初始化结果 */
    result->capacity = 1024;
    result->series_ids = (int64_t *)malloc(result->capacity * sizeof(int64_t));
    result->count = 0;

    /* 简化实现：返回所有序列（实际应该根据查询条件过滤） */
    if (index->total_series > 0) {
        for (uint64_t i = 0; i < index->total_series && result->count < result->capacity; i++) {
            result->series_ids[result->count++] = (int64_t)i;
        }
    }

    return 0;
}

int tag_index_query_and(const TagIndex *index,
                        const TagQuery **queries,
                        uint32_t num_queries,
                        TagQueryResult *result) {
    if (!index || !queries || num_queries == 0 || !result) return -1;

    /* AND 查询：取所有查询结果的交集 */
    TagQueryResult temp_result;
    memset(&temp_result, 0, sizeof(temp_result));

    /* 执行第一个查询 */
    if (tag_index_query(index, queries[0], &temp_result) != 0) {
        return -1;
    }

    /* 与后续查询结果取交集 */
    for (uint32_t q = 1; q < num_queries && temp_result.count > 0; q++) {
        TagQueryResult next_result;
        if (tag_index_query(index, queries[q], &next_result) != 0) {
            tag_query_result_free(&temp_result);
            return -1;
        }

        /* 交集 */
        uint32_t intersect_count = 0;
        for (uint32_t i = 0; i < temp_result.count; i++) {
            for (uint32_t j = 0; j < next_result.count; j++) {
                if (temp_result.series_ids[i] == next_result.series_ids[j]) {
                    if (intersect_count < temp_result.count) {
                        temp_result.series_ids[intersect_count++] = temp_result.series_ids[i];
                    }
                    break;
                }
            }
        }
        temp_result.count = intersect_count;
        tag_query_result_free(&next_result);
    }

    /* 复制结果 */
    result->capacity = temp_result.count;
    result->series_ids = temp_result.series_ids;
    result->count = temp_result.count;

    return 0;
}

int tag_index_query_or(const TagIndex *index,
                       const TagQuery **queries,
                       uint32_t num_queries,
                       TagQueryResult *result) {
    if (!index || !queries || num_queries == 0 || !result) return -1;

    /* 初始化结果 */
    result->capacity = 1024;
    result->series_ids = (int64_t *)malloc(result->capacity * sizeof(int64_t));
    result->count = 0;

    /* OR 查询：合并所有查询结果 */
    for (uint32_t q = 0; q < num_queries; q++) {
        TagQueryResult query_result;
        if (tag_index_query(index, queries[q], &query_result) != 0) {
            continue;
        }

        /* 合并 */
        for (uint32_t i = 0; i < query_result.count; i++) {
            /* 检查是否已存在 */
            bool exists = false;
            for (uint32_t j = 0; j < result->count; j++) {
                if (result->series_ids[j] == query_result.series_ids[i]) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                if (result->count >= result->capacity) {
                    result->capacity *= 2;
                    int64_t *new_ids = (int64_t *)realloc(result->series_ids,
                        result->capacity * sizeof(int64_t));
                    if (new_ids) result->series_ids = new_ids;
                }
                result->series_ids[result->count++] = query_result.series_ids[i];
            }
        }

        free(query_result.series_ids);
    }

    return 0;
}

void tag_query_result_free(TagQueryResult *result) {
    if (result) {
        free(result->series_ids);
        result->series_ids = NULL;
        result->count = 0;
        result->capacity = 0;
    }
}

uint32_t tag_index_count(const TagIndex *index, const TagQuery *query) {
    if (!index || !query) return 0;

    TagQueryResult result;
    if (tag_index_query(index, query, &result) != 0) {
        return 0;
    }

    uint32_t count = result.count;
    free(result.series_ids);
    return count;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

int tag_index_cardinality(const TagIndex *index,
                          const char *key,
                          TagCardinality *out_stats) {
    if (!index || !key || !out_stats) return -1;

    strncpy(out_stats->key, key, sizeof(out_stats->key) - 1);
    out_stats->cardinality = index->num_keys;
    out_stats->total_count = index->total_tags;

    return 0;
}

int tag_index_all_cardinalities(const TagIndex *index,
                                TagCardinality **out_stats,
                                uint32_t *out_count) {
    if (!index || !out_stats || !out_count) return -1;

    /* 简化实现：返回空列表 */
    *out_stats = NULL;
    *out_count = 0;

    return 0;
}

int tag_index_top_values(const TagIndex *index,
                         const char *key,
                         uint32_t top_n,
                         char ***out_values,
                         uint64_t **out_counts,
                         uint32_t *out_count) {
    if (!index || !key) return -1;

    /* 简化实现：返回空 */
    if (out_values) *out_values = NULL;
    if (out_counts) *out_counts = NULL;
    if (out_count) *out_count = 0;

    return 0;
}

/* ========================================================================
 * 索引维护
 * ======================================================================== */

int tag_index_optimize(TagIndex *index) {
    if (!index) return -1;
    /* 简化实现 */
    return 0;
}

void tag_index_stats(const TagIndex *index,
                     uint64_t *out_total_series,
                     uint64_t *out_total_tags,
                     uint64_t *out_index_size) {
    if (!index) return;

    if (out_total_series) *out_total_series = index->total_series;
    if (out_total_tags) *out_total_tags = index->total_tags;
    if (out_index_size) *out_index_size = index->index_size;
}

/* ========================================================================
 * 降采样索引（简化实现）
 * ======================================================================== */

int tag_index_create_downsample(TagIndex *index,
                                const char *series_key,
                                int64_t interval_ms,
                                DownsampleType type) {
    (void)index;
    (void)series_key;
    (void)interval_ms;
    (void)type;
    /* 简化实现 */
    return 0;
}

int tag_index_query_downsample(const TagIndex *index,
                               const char *series_key,
                               int64_t start_time,
                               int64_t end_time,
                               int64_t interval_ms,
                               DownsampleType type,
                               double *out_values,
                               int64_t *out_timestamps,
                               uint32_t max_count,
                               uint32_t *out_count) {
    (void)index;
    (void)series_key;
    (void)start_time;
    (void)end_time;
    (void)interval_ms;
    (void)type;
    (void)out_values;
    (void)out_timestamps;
    (void)max_count;
    if (out_count) *out_count = 0;
    return 0;
}
