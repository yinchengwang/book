/*
 * gin.c
 *
 * GIN (Generalized Inverted Index) 通用倒排索引实现
 *
 * 核心数据结构：
 * - Posting List：同一 key 的文档 ID 有序链表
 * - Posting Array：列表过长时转换为有序数组（支持二分查找）
 * - Entry：B-Tree 中的条目，key → postings 映射
 *
 * 设计要点：
 * 1. 使用动态数组实现简单的 B-Tree 结构
 * 2. 小列表用链表，大列表用有序数组 + 二分查找
 * 3. JSONB 展平使用递归解析
 */

#include <db/index/gin/gin.h>
#include <db/index/gin/gin_internal.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── 内部宏定义 ── */
#define GIN_INITIAL_CAPACITY 16
#define GIN_POSTING_LIST_THRESHOLD 128  /* 超过此长度转换为有序数组 */

/* ── 静态函数声明 ── */
static int _gin_entry_insert_doc(gin_entry_t *entry, int doc_id);
static int _gin_entry_delete_doc(gin_entry_t *entry, int doc_id);
static void _gin_entry_destroy(gin_entry_t *entry);
static void _gin_entry_traverse(const gin_entry_t *entry, int *results, int *count);

static void _posting_list_append(posting_list_t **head, int doc_id);
static void _posting_list_destroy(posting_list_t *list);
static posting_list_t *_posting_list_create(int doc_id);

static int _compare_int(const void *a, const void *b);
static int _binary_search_entries(gin_entry_t *entries, int count, const char *key);

/* ── 核心 API 实现 ── */

gin_index_t *gin_create(int max_entries)
{
    gin_index_t *idx = (gin_index_t *)calloc(1, sizeof(gin_index_t));
    if (!idx) {
        return NULL;
    }

    idx->capacity = GIN_INITIAL_CAPACITY;
    idx->max_entries = max_entries > 0 ? max_entries : 1024;
    idx->posting_list_threshold = GIN_POSTING_LIST_THRESHOLD;

    idx->entries = (gin_entry_t *)calloc(idx->capacity, sizeof(gin_entry_t));
    if (!idx->entries) {
        free(idx);
        return NULL;
    }

    return idx;
}

int gin_insert(gin_index_t *idx, const char *key, int doc_id)
{
    if (!idx || !key) {
        return -1;
    }

    /* 查找是否已存在该 key */
    int pos = _binary_search_entries(idx->entries, idx->size, key);

    if (pos >= 0) {
        /* 已存在，追加到 posting */
        return _gin_entry_insert_doc(&idx->entries[pos], doc_id);
    }

    /* 需要插入新 entry */
    if (idx->size >= idx->capacity) {
        /* 扩容 */
        int new_capacity = idx->capacity * 2;
        gin_entry_t *new_entries = (gin_entry_t *)realloc(
            idx->entries, new_capacity * sizeof(gin_entry_t));
        if (!new_entries) {
            return -1;
        }
        idx->entries = new_entries;
        idx->capacity = new_capacity;
    }

    /* 插入位置（binary_search 返回 -(pos+1)） */
    int insert_pos = -(pos + 1);

    /* 移动后续元素腾出位置 */
    for (int i = idx->size; i > insert_pos; i--) {
        idx->entries[i] = idx->entries[i - 1];
    }

    /* 初始化新 entry */
    memset(&idx->entries[insert_pos], 0, sizeof(gin_entry_t));
    idx->entries[insert_pos].key = strdup(key);
    if (!idx->entries[insert_pos].key) {
        /* 回退移动 */
        for (int i = insert_pos; i < idx->size; i++) {
            idx->entries[i] = idx->entries[i + 1];
        }
        return -1;
    }

    /* 创建新的 posting list */
    idx->entries[insert_pos].postings.list = _posting_list_create(doc_id);
    if (!idx->entries[insert_pos].postings.list) {
        free(idx->entries[insert_pos].key);
        for (int i = insert_pos; i < idx->size; i++) {
            idx->entries[i] = idx->entries[i + 1];
        }
        return -1;
    }

    idx->entries[insert_pos].count = 1;
    idx->entries[insert_pos].is_array = 0;
    idx->size++;
    idx->total_docs++;

    return 0;
}

int gin_insert_batch(gin_index_t *idx, const char **keys, const int *doc_ids, int count)
{
    if (!idx || !keys || !doc_ids) {
        return -1;
    }

    int success = 0;
    for (int i = 0; i < count; i++) {
        if (gin_insert(idx, keys[i], doc_ids[i]) == 0) {
            success++;
        }
    }
    return success;
}

int gin_search(const gin_index_t *idx, const char *query, int *results, int *count)
{
    if (!idx || !query || !results || !count) {
        return -1;
    }

    *count = 0;

    int pos = _binary_search_entries(idx->entries, idx->size, query);
    if (pos < 0) {
        return 0;  /* 未找到 */
    }

    _gin_entry_traverse(&idx->entries[pos], results, count);
    return 0;
}

int gin_range_query(const gin_index_t *idx, const char *start, const char *end,
                    int *results, int *count)
{
    if (!idx || !start || !end || !results || !count) {
        return -1;
    }

    *count = 0;

    /* 找到起始位置 */
    int start_pos = -( _binary_search_entries(idx->entries, idx->size, start) + 1);
    if (start_pos < 0) start_pos = 0;

    /* 遍历范围内的所有 entry */
    for (int i = start_pos; i < idx->size; i++) {
        if (strcmp(idx->entries[i].key, end) > 0) {
            break;
        }
        _gin_entry_traverse(&idx->entries[i], results, count);
    }

    return 0;
}

int gin_delete(gin_index_t *idx, const char *key, int doc_id)
{
    if (!idx || !key) {
        return -1;
    }

    int pos = _binary_search_entries(idx->entries, idx->size, key);
    if (pos < 0) {
        return -1;  /* 未找到 */
    }

    return _gin_entry_delete_doc(&idx->entries[pos], doc_id);
}

int gin_count(const gin_index_t *idx, const char *key)
{
    if (!idx || !key) {
        return -1;
    }

    int pos = _binary_search_entries(idx->entries, idx->size, key);
    if (pos < 0) {
        return 0;
    }

    return idx->entries[pos].count;
}

void gin_stats(const gin_index_t *idx, int *out_key_count, int *out_doc_count)
{
    if (!idx) {
        if (out_key_count) *out_key_count = 0;
        if (out_doc_count) *out_doc_count = 0;
        return;
    }

    if (out_key_count) *out_key_count = idx->size;
    if (out_doc_count) *out_doc_count = idx->total_docs;
}

void gin_destroy(gin_index_t *idx)
{
    if (!idx) {
        return;
    }

    /* 释放所有 entry */
    for (int i = 0; i < idx->size; i++) {
        _gin_entry_destroy(&idx->entries[i]);
    }

    free(idx->entries);
    free(idx);
}

/* ── JSONB/数组支持实现 ── */

int gin_insert_json(gin_index_t *idx, const char *json, int doc_id)
{
    if (!idx || !json) {
        return -1;
    }

    int count = 0;
    return _gin_parse_json_object(json, "", idx, doc_id, &count);
}

int gin_insert_array(gin_index_t *idx, const char **values, int count, int doc_id)
{
    if (!idx || !values) {
        return -1;
    }

    int success = 0;
    for (int i = 0; i < count; i++) {
        if (gin_insert(idx, values[i], doc_id) == 0) {
            success++;
        }
    }
    return success;
}

/* ── 内部函数实现 ── */

static int _gin_entry_insert_doc(gin_entry_t *entry, int doc_id)
{
    if (entry->is_array) {
        /* Posting Array 模式：追加到数组 */
        posting_array_t *arr = entry->postings.array;
        if (arr->count >= arr->capacity) {
            arr->capacity *= 2;
            int *new_ids = (int *)realloc(arr->doc_ids, arr->capacity * sizeof(int));
            if (!new_ids) {
                return -1;
            }
            arr->doc_ids = new_ids;
        }
        arr->doc_ids[arr->count++] = doc_id;
        entry->count++;
    } else {
        /* Posting List 模式：追加到链表 */
        _posting_list_append(&entry->postings.list, doc_id);
        entry->count++;

        /* 超过阈值，转换为有序数组 */
        if (entry->count > GIN_POSTING_LIST_THRESHOLD) {
            /* 转换为 posting array */
            posting_array_t *arr = (posting_array_t *)malloc(sizeof(posting_array_t));
            if (!arr) {
                return -1;
            }

            /* 收集所有 doc_id 到数组 */
            arr->capacity = entry->count;
            arr->count = 0;
            arr->doc_ids = (int *)malloc(arr->capacity * sizeof(int));

            if (!arr->doc_ids) {
                free(arr);
                return -1;
            }

            /* 遍历链表收集 */
            posting_list_t *list = entry->postings.list;
            while (list) {
                arr->doc_ids[arr->count++] = list->doc_id;
                list = list->next;
            }

            /* 销毁链表 */
            _posting_list_destroy(entry->postings.list);

            /* 排序 */
            qsort(arr->doc_ids, arr->count, sizeof(int), _compare_int);

            /* 去重 */
            int write = 0;
            for (int read = 1; read < arr->count; read++) {
                if (arr->doc_ids[read] != arr->doc_ids[write]) {
                    arr->doc_ids[++write] = arr->doc_ids[read];
                }
            }
            arr->count = write + 1;

            entry->postings.array = arr;
            entry->is_array = 1;
        }
    }

    return 0;
}

static int _gin_entry_delete_doc(gin_entry_t *entry, int doc_id)
{
    if (entry->is_array) {
        /* Posting Array 模式 */
        posting_array_t *arr = entry->postings.array;
        for (int i = 0; i < arr->count; i++) {
            if (arr->doc_ids[i] == doc_id) {
                memmove(&arr->doc_ids[i], &arr->doc_ids[i+1],
                        (arr->count - i - 1) * sizeof(int));
                arr->count--;
                entry->count--;
                return 0;
            }
        }
    } else {
        /* Posting List 模式 */
        posting_list_t **pp = &entry->postings.list;
        posting_list_t *p = *pp;
        while (p) {
            if (p->doc_id == doc_id) {
                *pp = p->next;
                free(p);
                entry->count--;
                return 0;
            }
            pp = &p->next;
            p = p->next;
        }
    }
    return -1;
}

static void _gin_entry_destroy(gin_entry_t *entry)
{
    if (!entry) return;

    free(entry->key);

    if (entry->is_array) {
        posting_array_t *arr = entry->postings.array;
        if (arr) {
            free(arr->doc_ids);
            free(arr);
        }
    } else {
        _posting_list_destroy(entry->postings.list);
    }
}

static void _gin_entry_traverse(const gin_entry_t *entry, int *results, int *count)
{
    if (!entry || !results || !count) return;

    if (entry->is_array) {
        posting_array_t *arr = entry->postings.array;
        for (int i = 0; i < arr->count; i++) {
            results[(*count)++] = arr->doc_ids[i];
        }
    } else {
        posting_list_t *list = entry->postings.list;
        while (list) {
            results[(*count)++] = list->doc_id;
            list = list->next;
        }
    }
}

/* ── Posting List 辅助函数 ── */

static posting_list_t *_posting_list_create(int doc_id)
{
    posting_list_t *list = (posting_list_t *)malloc(sizeof(posting_list_t));
    if (!list) return NULL;
    list->doc_id = doc_id;
    list->next = NULL;
    return list;
}

static void _posting_list_append(posting_list_t **head, int doc_id)
{
    posting_list_t *new_node = _posting_list_create(doc_id);
    if (!new_node) return;

    if (!*head) {
        *head = new_node;
        return;
    }

    /* 追加到链表末尾（保持有序） */
    posting_list_t *prev = NULL;
    posting_list_t *curr = *head;

    while (curr && curr->doc_id < doc_id) {
        prev = curr;
        curr = curr->next;
    }

    if (!prev) {
        /* 插入到头部 */
        new_node->next = *head;
        *head = new_node;
    } else {
        new_node->next = curr;
        prev->next = new_node;
    }
}

static void _posting_list_destroy(posting_list_t *list)
{
    while (list) {
        posting_list_t *next = list->next;
        free(list);
        list = next;
    }
}

/* ── 比较和搜索函数 ── */

static int _compare_int(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return ia - ib;
}

static int _binary_search_entries(gin_entry_t *entries, int count, const char *key)
{
    int left = 0, right = count - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(entries[mid].key, key);

        if (cmp == 0) {
            return mid;
        } else if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    /* 返回插入位置（负数形式） */
    return -(left + 1);
}

/* ── JSON 展平实现 ── */

int _gin_parse_json_object(const char *json, const char *prefix,
                           gin_index_t *idx, int doc_id, int *count)
{
    if (!json || !idx) return -1;

    const char *p = json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    if (*p == '{') {
        /* JSON 对象 */
        p++;
        while (*p && *p != '}') {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
            if (*p == '}') break;

            if (*p == '\"') {
                /* 读取 key */
                p++;
                const char *key_start = p;
                while (*p && *p != '\"') p++;
                size_t key_len = p - key_start;
                char key[256] = {0};
                if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
                strncpy(key, key_start, key_len);
                key[key_len] = '\0';

                /* 跳过引号和冒号 */
                while (*p && *p != ':') p++;
                if (*p == ':') p++;
                while (*p == ' ' || *p == '\t') p++;

                /* 构建完整路径 */
                char full_key[512] = {0};
                if (prefix && prefix[0]) {
                    snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key);
                } else {
                    strncpy(full_key, key, sizeof(full_key) - 1);
                }

                /* 读取 value */
                if (*p == '\"') {
                    p++;
                    const char *val_start = p;
                    while (*p && *p != '\"') p++;
                    size_t val_len = p - val_start;
                    if (val_len < sizeof(key)) {
                        strncpy(key, val_start, val_len);
                        key[val_len] = '\0';
                        if (gin_insert(idx, key, doc_id) == 0) {
                            (*count)++;
                        }
                    }
                    if (*p == '\"') p++;
                } else if (*p == '{') {
                    int depth = 1;
                    const char *obj_start = p;
                    while (*p && depth > 0) {
                        p++;
                        if (*p == '{') depth++;
                        else if (*p == '}') depth--;
                    }
                    p++;
                    int sub_count = 0;
                    _gin_parse_json_object(obj_start, full_key, idx, doc_id, &sub_count);
                    (*count) += sub_count;
                } else if (*p == '[') {
                    const char *arr_start = p;
                    int depth = 1;
                    while (*p && depth > 0) {
                        p++;
                        if (*p == '[') depth++;
                        else if (*p == ']') depth--;
                    }
                    p++;
                    char arr_val[1024] = {0};
                    size_t arr_len = p - arr_start;
                    if (arr_len < sizeof(arr_val)) {
                        strncpy(arr_val, arr_start, arr_len);
                        arr_val[arr_len] = '\0';
                        if (gin_insert(idx, arr_val, doc_id) == 0) {
                            (*count)++;
                        }
                    }
                }
            }
        }
    } else if (*p == '[') {
        p++;
        int elem_idx = 0;
        while (*p && *p != ']') {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
            if (*p == ']') break;

            if (*p == '\"') {
                p++;
                const char *val_start = p;
                while (*p && *p != '\"') p++;
                size_t val_len = p - val_start;
                char val[256] = {0};
                if (val_len < sizeof(val)) {
                    strncpy(val, val_start, val_len);
                    val[val_len] = '\0';
                    if (gin_insert(idx, val, doc_id) == 0) {
                        (*count)++;
                    }
                }
                if (*p == '\"') p++;
            } else if (*p == '{') {
                int depth = 1;
                const char *obj_start = p;
                while (*p && depth > 0) {
                    p++;
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                }
                p++;
                char prefix_buf[64] = {0};
                snprintf(prefix_buf, sizeof(prefix_buf), "%s[%d]",
                         prefix && prefix[0] ? prefix : "", elem_idx);
                int sub_count = 0;
                _gin_parse_json_object(obj_start, prefix_buf, idx, doc_id, &sub_count);
                (*count) += sub_count;
            }
            elem_idx++;
        }
    }

    return 0;
}
