/*
 * brin.c
 *
 * BRIN (Block Range Index) 块范围索引实现
 *
 * 核心原理：
 * - 将表按物理块划分为多个范围（Range）
 * - 每个范围维护一个摘要（Summary），包含该范围内所有键的 min/max
 * - 查询时，利用摘要快速跳过不相关的范围
 *
 * 设计要点：
 * 1. 使用动态数组管理范围
 * 2. 简单字符串比较作为默认键比较函数
 * 3. 内存友好，适合超大规模数据
 */

#include <db/index/brin/brin.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── 默认键大小 ── */
#define BRIN_DEFAULT_KEY_SIZE 64

/* ── 内部结构 ── */

/* 单条记录 */
typedef struct brin_record {
    int page_num;
    void *key;
    int doc_id;
} brin_record_t;

/* 页面摘要 */
typedef struct brin_summary {
    int range_start;            /* 起始页面 */
    int range_end;              /* 结束页面 */
    int n_pages;               /* 页面数量 */
    void *min_key;             /* 最小键 */
    void *max_key;             /* 最大键 */
    int n_records;             /* 记录数 */
} brin_summary_t;

/* 范围结构 */
typedef struct brin_range {
    brin_summary_t summary;
    brin_record_t *records;     /* 该范围内的所有记录 */
    int n_records;
    int capacity;
} brin_range_t;

/* BRIN 索引结构 */
struct brin_index {
    int page_size;             /* 页面大小 */
    int pages_per_range;        /* 每范围页面数 */
    int range_size;             /* 范围大小（字节） */

    brin_range_t *ranges;       /* 范围数组 */
    int n_ranges;               /* 范围数量 */
    int range_capacity;         /* 范围容量 */

    int key_size;               /* 键大小 */
    void *min_key_buffer;       /* 用于比较的临时缓冲区 */
    void *max_key_buffer;

    /* 比较函数 */
    brin_compare_fn compare;
    void *compare_ctx;
};

/* ── 静态函数声明 ── */
static int _brin_default_compare(const void *a, const void *b, void *ctx);
static int _brin_find_range(const brin_index_t *idx, int page_num);
static int _brin_update_range_summary(brin_index_t *idx, brin_range_t *range);
static int _brin_key_between(const brin_index_t *idx, const void *key,
                              const void *min, const void *max);
static void _brin_range_destroy(brin_range_t *range);
static int _brin_ensure_range_capacity(brin_range_t *range, int min_capacity);

/* ── 核心 API 实现 ── */

brin_index_t *brin_create(int page_size, int pages_per_range)
{
    if (page_size <= 0 || pages_per_range <= 0) {
        return NULL;
    }

    brin_index_t *idx = (brin_index_t *)calloc(1, sizeof(brin_index_t));
    if (!idx) return NULL;

    idx->page_size = page_size;
    idx->pages_per_range = pages_per_range;
    idx->range_size = page_size * pages_per_range;
    idx->n_ranges = 0;
    idx->range_capacity = 16;
    idx->key_size = BRIN_DEFAULT_KEY_SIZE;

    idx->ranges = (brin_range_t *)calloc(idx->range_capacity, sizeof(brin_range_t));
    if (!idx->ranges) {
        free(idx);
        return NULL;
    }

    /* 初始化默认比较函数 */
    idx->compare = _brin_default_compare;
    idx->compare_ctx = NULL;

    /* 分配临时缓冲区 */
    idx->min_key_buffer = malloc(BRIN_DEFAULT_KEY_SIZE);
    idx->max_key_buffer = malloc(BRIN_DEFAULT_KEY_SIZE);

    if (!idx->min_key_buffer || !idx->max_key_buffer) {
        free(idx->min_key_buffer);
        free(idx->max_key_buffer);
        free(idx->ranges);
        free(idx);
        return NULL;
    }

    return idx;
}

void brin_set_compare(brin_index_t *idx, brin_compare_fn compare, void *ctx)
{
    if (!idx || !compare) return;
    idx->compare = compare;
    idx->compare_ctx = ctx;
}

int brin_insert(brin_index_t *idx, int page_num, const void *key, int doc_id)
{
    if (!idx || !key) return -1;

    /* 找到或创建对应的范围 */
    int range_idx = _brin_find_range(idx, page_num);

    if (range_idx < 0) {
        /* 需要创建新范围 */
        if (idx->n_ranges >= idx->range_capacity) {
            int new_cap = idx->range_capacity * 2;
            brin_range_t *new_ranges = (brin_range_t *)realloc(
                idx->ranges, new_cap * sizeof(brin_range_t));
            if (!new_ranges) return -1;
            idx->ranges = new_ranges;
            idx->range_capacity = new_cap;
        }

        range_idx = idx->n_ranges;
        idx->n_ranges++;

        /* 初始化新范围 */
        memset(&idx->ranges[range_idx], 0, sizeof(brin_range_t));
        idx->ranges[range_idx].summary.range_start = page_num;
        idx->ranges[range_idx].summary.range_end = page_num;
        idx->ranges[range_idx].summary.n_pages = 1;
        idx->ranges[range_idx].summary.min_key = malloc(BRIN_DEFAULT_KEY_SIZE);
        idx->ranges[range_idx].summary.max_key = malloc(BRIN_DEFAULT_KEY_SIZE);

        if (!idx->ranges[range_idx].summary.min_key ||
            !idx->ranges[range_idx].summary.max_key) {
            free(idx->ranges[range_idx].summary.min_key);
            free(idx->ranges[range_idx].summary.max_key);
            idx->n_ranges--;
            return -1;
        }

        /* 复制键值 */
        memcpy(idx->ranges[range_idx].summary.min_key, key, BRIN_DEFAULT_KEY_SIZE);
        memcpy(idx->ranges[range_idx].summary.max_key, key, BRIN_DEFAULT_KEY_SIZE);
    }

    /* 添加记录到范围 */
    brin_range_t *range = &idx->ranges[range_idx];
    if (_brin_ensure_range_capacity(range, range->n_records + 1) != 0) {
        return -1;
    }

    /* 更新范围信息 */
    if (page_num > range->summary.range_end) {
        range->summary.range_end = page_num;
    }
    if (page_num < range->summary.range_start) {
        range->summary.range_start = page_num;
    }

    /* 找出该页面所在的范围 */
    int this_range_idx = page_num / idx->pages_per_range;
    if (range_idx != this_range_idx) {
        /* 重新定位范围 */
        range_idx = this_range_idx;
        if (range_idx >= idx->n_ranges) {
            return -1;
        }
        range = &idx->ranges[range_idx];
    }

    /* 更新 min/max */
    if (idx->compare(key, range->summary.min_key, idx->compare_ctx) < 0) {
        memcpy(range->summary.min_key, key, BRIN_DEFAULT_KEY_SIZE);
    }
    if (idx->compare(range->summary.max_key, key, idx->compare_ctx) < 0) {
        memcpy(range->summary.max_key, key, BRIN_DEFAULT_KEY_SIZE);
    }

    /* 添加记录 */
    brin_record_t *rec = &range->records[range->n_records];
    rec->page_num = page_num;
    rec->key = malloc(BRIN_DEFAULT_KEY_SIZE);
    if (!rec->key) return -1;
    memcpy(rec->key, key, BRIN_DEFAULT_KEY_SIZE);
    rec->doc_id = doc_id;
    range->n_records++;
    range->summary.n_records++;

    return 0;
}

int brin_insert_batch(brin_index_t *idx, const int *page_nums,
                     const void **keys, const int *doc_ids, int count)
{
    if (!idx || !page_nums || !keys || !doc_ids) return -1;

    int success = 0;
    for (int i = 0; i < count; i++) {
        if (brin_insert(idx, page_nums[i], keys[i], doc_ids[i]) == 0) {
            success++;
        }
    }
    return success;
}

int brin_range_query(const brin_index_t *idx, const void *min_key, const void *max_key,
                    int *results, int *count)
{
    if (!idx || !min_key || !max_key || !results || !count) return -1;

    *count = 0;

    /* 遍历所有范围，检查是否与查询范围重叠 */
    for (int i = 0; i < idx->n_ranges; i++) {
        brin_range_t *range = &idx->ranges[i];

        /* 快速跳过：如果范围的 max < 查询 min，或 min > 查询 max */
        if (idx->compare(range->summary.max_key, min_key, idx->compare_ctx) < 0) continue;
        if (idx->compare(max_key, range->summary.min_key, idx->compare_ctx) < 0) continue;

        /* 范围与查询范围有交集，遍历记录 */
        for (int j = 0; j < range->n_records; j++) {
            brin_record_t *rec = &range->records[j];
            if (_brin_key_between(idx, rec->key, min_key, max_key)) {
                results[(*count)++] = rec->doc_id;
            }
        }
    }

    return 0;
}

int brin_scan(const brin_index_t *idx, int start_page, int end_page,
              int *results, int *count)
{
    if (!idx || !results || !count) return -1;
    if (start_page > end_page) return -1;

    *count = 0;

    for (int i = 0; i < idx->n_ranges; i++) {
        brin_range_t *range = &idx->ranges[i];

        /* 跳过不重叠的范围 */
        if (range->summary.range_end < start_page) continue;
        if (range->summary.range_start > end_page) continue;

        /* 范围与扫描范围有交集 */
        for (int j = 0; j < range->n_records; j++) {
            brin_record_t *rec = &range->records[j];
            if (rec->page_num >= start_page && rec->page_num <= end_page) {
                results[(*count)++] = rec->doc_id;
            }
        }
    }

    return 0;
}

int brin_update_summary(brin_index_t *idx, int page_num)
{
    if (!idx) return -1;

    int range_idx = _brin_find_range(idx, page_num);
    if (range_idx < 0) return -1;

    return _brin_update_range_summary(idx, &idx->ranges[range_idx]);
}

void brin_stats(const brin_index_t *idx, int *out_n_ranges, int *out_n_pages, int *out_total_pages)
{
    if (!idx) {
        if (out_n_ranges) *out_n_ranges = 0;
        if (out_n_pages) *out_n_pages = 0;
        if (out_total_pages) *out_total_pages = 0;
        return;
    }

    if (out_n_ranges) *out_n_ranges = idx->n_ranges;

    int total_pages = 0;
    int total_records = 0;
    for (int i = 0; i < idx->n_ranges; i++) {
        total_pages += idx->ranges[i].summary.n_pages;
        total_records += idx->ranges[i].n_records;
    }

    if (out_n_pages) *out_n_pages = total_pages;
    if (out_total_pages) *out_total_pages = total_records;
}

void brin_destroy(brin_index_t *idx)
{
    if (!idx) return;

    for (int i = 0; i < idx->n_ranges; i++) {
        _brin_range_destroy(&idx->ranges[i]);
    }

    free(idx->ranges);
    free(idx->min_key_buffer);
    free(idx->max_key_buffer);
    free(idx);
}

/* ── 内部函数实现 ── */

static int _brin_default_compare(const void *a, const void *b, void *ctx)
{
    (void)ctx;
    return memcmp(a, b, BRIN_DEFAULT_KEY_SIZE);
}

static int _brin_find_range(const brin_index_t *idx, int page_num)
{
    for (int i = 0; i < idx->n_ranges; i++) {
        if (page_num >= idx->ranges[i].summary.range_start &&
            page_num <= idx->ranges[i].summary.range_end) {
            return i;
        }
    }
    return -1;
}

static int _brin_update_range_summary(brin_index_t *idx, brin_range_t *range)
{
    if (!range || range->n_records == 0) return -1;

    /* 遍历记录找 min/max */
    memcpy(range->summary.min_key, range->records[0].key, BRIN_DEFAULT_KEY_SIZE);
    memcpy(range->summary.max_key, range->records[0].key, BRIN_DEFAULT_KEY_SIZE);

    for (int i = 1; i < range->n_records; i++) {
        if (idx->compare(range->records[i].key, range->summary.min_key, idx->compare_ctx) < 0) {
            memcpy(range->summary.min_key, range->records[i].key, BRIN_DEFAULT_KEY_SIZE);
        }
        if (idx->compare(range->summary.max_key, range->records[i].key, idx->compare_ctx) < 0) {
            memcpy(range->summary.max_key, range->records[i].key, BRIN_DEFAULT_KEY_SIZE);
        }
    }

    return 0;
}

static int _brin_key_between(const brin_index_t *idx, const void *key,
                              const void *min, const void *max)
{
    return !(idx->compare(key, min, idx->compare_ctx) < 0) &&
           !(idx->compare(max, key, idx->compare_ctx) < 0);
}

static void _brin_range_destroy(brin_range_t *range)
{
    if (!range) return;

    for (int i = 0; i < range->n_records; i++) {
        free(range->records[i].key);
    }
    free(range->records);
    free(range->summary.min_key);
    free(range->summary.max_key);
}

static int _brin_ensure_range_capacity(brin_range_t *range, int min_capacity)
{
    if (range->n_records < range->capacity) return 0;

    int new_cap = range->capacity > 0 ? range->capacity * 2 : 16;
    if (new_cap < min_capacity) new_cap = min_capacity;

    brin_record_t *new_recs = (brin_record_t *)realloc(
        range->records, new_cap * sizeof(brin_record_t));
    if (!new_recs) return -1;

    range->records = new_recs;
    range->capacity = new_cap;

    return 0;
}
