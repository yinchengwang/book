/*
 * bm25_private.h
 *
 * BM25 倒排表内部结构。
 * terms 是倒排字典；每个 term 下挂 postings，doc_lengths 保存文档长度统计。
 */

#ifndef BM25_PRIVATE_H
#define BM25_PRIVATE_H

#include <db/index/vector_index/BM25/bm25.h>

#include <algo-prod/dict/dict.h>

#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct bm25_posting {
    int32_t doc_id;      /* 命中文档 id。 */
    int32_t term_freq;   /* 该 term 在该文档中的词频。 */
} bm25_posting_t;

typedef struct bm25_posting_filter {
    int32_t start_offset;    /* 当前块在 postings 中的起始下标。 */
    int32_t end_offset;      /* 当前块在 postings 中的结束下标（开区间）。 */
    int32_t last_doc_id;     /* 当前块最后一个 posting 的 doc_id。 */
    int32_t max_term_freq;   /* 当前块内最大的 term frequency。 */
    float max_score;         /* 当前块内单词项最大 BM25 分数上界。 */
} bm25_posting_filter_t;

typedef struct bm25_term_slot {
    char *term;                     /* 词项文本。 */
    uint64_t hash_value;            /* 词项哈希值。 */
    int32_t doc_freq;               /* 出现该词项的文档数。 */
    int32_t total_term_freq;        /* 该词项在全库中的总词频。 */
    bm25_posting_t *postings;       /* 倒排 posting 列表。 */
    int32_t posting_count;          /* 当前 posting 数。 */
    int32_t posting_capacity;       /* posting 数组容量。 */
    int32_t *posting_filter_list;   /* 每个过滤块对应的 posting 起始下标。 */
    bm25_posting_filter_t *posting_filters; /* posting filter 元数据数组。 */
    int32_t posting_filter_count;   /* 过滤块数量。 */
    int32_t posting_filter_capacity;/* posting filter 数组容量。 */
} bm25_term_slot_t;

/* 正排索引：记录某文档包含的 token 信息。 */
typedef struct bm25_doc_forward_item {
    int32_t term_slot_index;              /* term_slots 中的槽位下标。 */
    uint64_t token_hash;                  /* token 的哈希值。 */
} bm25_doc_forward_item_t;

typedef struct bm25_doc_forward_list {
    bm25_doc_forward_item_t *items;       /* 该文档的正排项数组。 */
    int32_t count;                        /* 当前 item 数目。 */
    int32_t capacity;                     /* items 数组容量。 */
} bm25_doc_forward_list_t;

typedef struct bm25_term_bucket_entry {
    char *term;                           /* 指向 term slot 内部字符串。 */
    uint64_t hash_value;                  /* 词项哈希值。 */
    int32_t term_slot_index;              /* 指向 term_slots 的槽位下标。 */
    struct bm25_term_bucket_entry *next;  /* 拉链指针。 */
} bm25_term_bucket_entry_t;

typedef struct bm25_term_freq_entry {
    char *term;                          /* 文档/查询内词项。 */
    uint64_t hash_value;                 /* 词项哈希值。 */
    int32_t freq;                        /* 当前词项出现次数。 */
    struct bm25_term_freq_entry *next;   /* 拉链指针。 */
} bm25_term_freq_entry_t;

typedef struct bm25_term_freq_map {
    bm25_term_freq_entry_t **buckets;
    bm25_term_freq_entry_t **items;
    int32_t bucket_count;
    int32_t item_count;
    int32_t item_capacity;
} bm25_term_freq_map_t;

typedef struct bm25_scored_doc {
    int32_t doc_id;   /* 文档 id。 */
    float score;      /* BM25 得分。 */
} bm25_scored_doc_t;

struct bm25 {
    bm25_term_bucket_entry_t **term_hash_buckets; /* 词项哈希桶数组。 */
    int32_t term_hash_bucket_count;               /* 哈希桶数量。 */
    bm25_term_slot_t *term_slots;                 /* term 槽位数组。 */
    int32_t term_slot_count;                      /* 已使用槽位数。 */
    int32_t term_slot_capacity;                   /* 槽位数组容量。 */
    dict_t *tokenizer;     /* 文本输入使用的分词器。 */
    bool owns_tokenizer;        /* tokenizer 是否由索引内部创建并负责释放。 */
    int32_t *doc_lengths;       /* 每篇文档长度。 */
    bm25_doc_forward_list_t *doc_forwards; /* 正排索引：每篇文档的 token 列表。 */
    int32_t doc_count;          /* 当前活跃文档总数。 */
    int32_t doc_capacity;       /* doc_lengths / doc_forwards 已分配容量。 */
    int32_t *free_doc_ids;      /* 已删除 docId 复用池。 */
    int32_t free_doc_count;     /* 复用池大小。 */
    int32_t free_doc_capacity;  /* 复用池容量。 */
    int64_t total_terms;        /* 全库词项总数，用于平均文档长度计算。 */
    bm25_params_t params;       /* BM25 超参数。 */
    bm25_search_stats_t last_search_stats; /* 最近一次搜索的内部统计。 */
};

/* 填充默认 k1/b 参数。 */
void bm25_set_default_params(bm25_params_t *params);
int bm25_params_are_valid(const bm25_params_t *params);
void bm25_normalize_params(bm25_params_t *params);

/* 确保存在一个默认分词器。 */
int bm25_ensure_default_tokenizer(bm25_t *index);

/* 字符串拷贝工具。 */
char *bm25_strdup(const char *value);

/* FNV-1a 64-bit term hash。 */
uint64_t bm25_hash_term(const char *value);

/* 保证文档长度数组容量足够。 */
int bm25_ensure_doc_capacity(bm25_t *index, int32_t target);

/* 保证 term 槽位数组与哈希桶数组容量足够。 */
int bm25_ensure_term_slot_capacity(bm25_t *index, int32_t target);
int bm25_ensure_term_hash_table(bm25_t *index, int32_t target_term_count);

/* term 哈希表与槽位数组操作。 */
int32_t bm25_find_term_slot_index(const bm25_t *index, const char *term, uint64_t hash_value);
bm25_term_slot_t *bm25_get_term_slot(const bm25_t *index, int32_t term_slot_index);
int32_t bm25_insert_term_slot(bm25_t *index, const char *term, uint64_t hash_value);
int bm25_remove_term_slot(bm25_t *index, int32_t term_slot_index);

/* 保证某个词项的 posting 数组容量足够。 */
int bm25_reserve_postings(bm25_term_slot_t *slot, int32_t target);
int bm25_reserve_posting_filters(bm25_term_slot_t *slot, int32_t target);
void bm25_rebuild_posting_filters(const bm25_t *index, bm25_term_slot_t *slot);

/* 正排索引用法。 */
int bm25_doc_forward_init(bm25_t *index, int32_t doc_id);
int bm25_doc_forward_add_token(bm25_t *index, int32_t doc_id, int32_t term_slot_index, uint64_t hash_value);
const bm25_doc_forward_list_t *bm25_doc_forward_get_tokens(const bm25_t *index, int32_t doc_id);
void bm25_doc_forward_release(bm25_t *index, int32_t doc_id);
void bm25_doc_forward_free_all(bm25_t *index);

/* Posting 有序插入与删除工具。 */
int bm25_posting_insert_ordered(bm25_term_slot_t *slot, int32_t doc_id, int32_t term_freq);
int bm25_posting_remove_at(bm25_term_slot_t *slot, int32_t posting_index);

/* 确保 posting filter 有效，dirty 时重建。 */
void bm25_ensure_posting_filters(const bm25_t *index, bm25_term_slot_t *slot);

/* DocId 分配与回收。 */
int32_t bm25_allocate_doc_id(bm25_t *index);
void bm25_free_doc_id(bm25_t *index, int32_t doc_id);

/* 释放倒排词典。 */
void bm25_free_terms(bm25_t *index);

/* 文档/查询临时词频哈希表。 */
int bm25_term_freq_map_init(bm25_term_freq_map_t *map, int32_t expected_terms);
int bm25_term_freq_map_increment(bm25_term_freq_map_t *map, char *term, uint64_t hash_value);
void bm25_term_freq_map_destroy(bm25_term_freq_map_t *map);

/* 使用 tokenizer 把原始文本切成 token。 */
int bm25_tokenize_text(const bm25_t *index, const char *text, token_t **tokens, int32_t *token_count);

/* 计算 BM25 的 IDF 分量。 */
float bm25_calculate_idf(const bm25_t *index, int32_t doc_freq);

/* 计算某个词项对某篇文档的贡献分。 */
float bm25_calculate_term_score(const bm25_t *index, int32_t doc_freq, int32_t term_freq, int32_t doc_length);

/* 搜索结果按分数降序排序。 */
int bm25_compare_scored_docs_desc(const void *lhs, const void *rhs);

/* 搜索统计工具。 */
void bm25_reset_search_stats(bm25_search_stats_t *stats, bm25_search_algorithm_t algorithm);

#endif