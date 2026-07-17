#ifndef DICT_PRIVATE_H
#define DICT_PRIVATE_H

#include <algo-prod/dict/dict.h>

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <uthash/uthash.h>

typedef struct dict_node dict_node_t;

/* 词典词条：存储词语及其频�?*/
typedef struct dict_word_entry {
    char *word;
    int32_t freq;
    struct dict_word_entry *next;
} dict_word_entry_t;

/* FNV-1a 64-bit 哈希桶节点，用于停用词集合和同义词映射 */
typedef struct dict_hash_bucket {
    char *word;
    uint64_t hash_value;
    struct dict_hash_bucket *next;
} dict_hash_bucket_t;

/* 停用词哈希集合 */
typedef struct dict_hash_set {
    dict_hash_bucket_t **buckets;
    int32_t bucket_count;
    int32_t item_count;
} dict_hash_set_t;

/* 同义词映射桶节点：比普通桶多一个 canonical_word 字段 */
typedef struct dict_hash_map_bucket {
    char *word;
    uint64_t hash_value;
    char *canonical_word;
    struct dict_hash_map_bucket *next;
} dict_hash_map_bucket_t;

/* 同义词哈希映射：word → canonical_word */
typedef struct dict_hash_map {
    dict_hash_map_bucket_t **buckets;
    int32_t bucket_count;
    int32_t item_count;
} dict_hash_map_t;

/* HMM 状态枚举：B=词首 E=词尾 M=词中 S=单字成词 */
typedef enum dict_hmm_state {
    DICT_HMM_STATE_B = 0,
    DICT_HMM_STATE_E = 1,
    DICT_HMM_STATE_M = 2,
    DICT_HMM_STATE_S = 3,
    DICT_HMM_STATE_COUNT = 4,
} dict_hmm_state_t;

/* HMM 模型：start/trans 概率以 log 形式存�? */
typedef struct dict_hmm_model {
    float start_prob[4];
    float trans_prob[4][4];
} dict_hmm_model_t;

/* Trie 根节点直接索引数组大小，覆盖 BMP 全部码点 */
#ifndef DICT_TRIE_ROOT_SIZE
#define DICT_TRIE_ROOT_SIZE 65536
#endif

typedef struct dict_edge {
    uint32_t codepoint;        /* 当前 UTF-8 rune�?*/
    dict_node_t *child;   /* 对应子节点�?*/
    UT_hash_handle hh;         /* uthash 句柄，用 codepoint 做键�?*/
} dict_edge_t;

struct dict_node {
    bool is_word;                /* 根到当前节点路径是否构成完整词语�?*/
    int32_t freq;                /* 该词语的频率；仅 is_word �?true 时有效�?*/
    dict_edge_t *children;  /* Trie 的出边集合 (uthash)�?*/
    dict_edge_t **root_children; /* root 节点的 codepoint→edge 直接索引缓存，非 root 节点为 NULL */
};

struct dict {
    dict_node_t *root;               /* Trie 根节点�?*/
    int32_t word_count;                   /* 已加载词条数�?*/
    int64_t total_freq;                   /* 全部词频总和，供分词打分使用�?*/
    dict_word_entry_t *words;        /* 词典链表，便�?reset / drop 统一释放�?*/
    dict_hash_set_t stop_words;      /* 停用词哈希集合�?*/
    dict_hash_map_t synonyms;        /* 同义词哈希映射�?*/
    bool hmm_enabled;                /* 是否启用 HMM 未登录词识�?*/
    dict_hmm_model_t hmm_model;      /* HMM 模型参数 */
};

/* UTF-8 文本拆分后的 rune 视图�?*/
typedef struct dict_rune {
    uint32_t codepoint;
    int32_t byte_start;
    int32_t byte_length;
} dict_rune_t;

/* 动态规划切词时的最优路径信息�?*/
typedef struct dict_route {
    double score;
    int32_t next_index;
} dict_route_t;

dict_node_t *dict_node_create(void);
void dict_node_destroy(dict_node_t *node);
dict_edge_t *dict_find_edge(const dict_node_t *node, uint32_t codepoint);
dict_edge_t *dict_get_or_create_edge(dict_node_t *node, uint32_t codepoint);
char *dict_strdup_slice(const char *text, int32_t byte_start, int32_t byte_length);
char *dict_strdup(const char *text);
int dict_utf8_decode_one(const char *text, int32_t remaining, uint32_t *codepoint, int32_t *byte_length);
bool dict_is_cjk(uint32_t codepoint);
bool dict_is_ascii_word_char(uint32_t codepoint);
bool dict_is_delimiter(uint32_t codepoint);
int dict_append_token(token_t **tokens,
                           int32_t *token_count,
                           int32_t *token_capacity,
                           const char *text,
                           int32_t byte_start,
                           int32_t byte_length,
                           bool is_ascii);
int dict_append_normalized_token(const dict_t *dict,
                                      token_t **tokens,
                                      int32_t *token_count,
                                      int32_t *token_capacity,
                                      const char *text,
                                      int32_t byte_start,
                                      int32_t byte_length,
                                      bool is_ascii);
const char *dict_embedded_jieba_dict(void);
int32_t dict_load_jieba_buffer(dict_t *dict, const char *buffer);

/* CutForSearch 子词扩展 */
int dict_expand_subwords(const dict_t *dict,
                         const char *text,
                         const dict_rune_t *runes,
                         int32_t rune_count,
                         token_t **tokens,
                         int32_t *token_count,
                         int32_t *token_capacity);
int dict_segment_cjk_for_search(const dict_t *dict,
                                const char *text,
                                int32_t byte_start,
                                int32_t byte_end,
                                token_t **tokens,
                                int32_t *token_count,
                                int32_t *token_capacity);

/* FNV-1a 哈希与哈希表操作 */
uint64_t dict_hash_fnv1a(const char *value);
void dict_hash_set_init(dict_hash_set_t *set);
void dict_hash_set_destroy(dict_hash_set_t *set);
int dict_hash_set_add(dict_hash_set_t *set, const char *word);
bool dict_hash_set_contains(const dict_hash_set_t *set, const char *word);
void dict_hash_map_init(dict_hash_map_t *map);
void dict_hash_map_destroy(dict_hash_map_t *map);
int dict_hash_map_put(dict_hash_map_t *map, const char *word, const char *canonical_word);
const char *dict_hash_map_get(const dict_hash_map_t *map, const char *word);

/* HMM 未登录词识�?*/
void dict_hmm_model_init_default(dict_hmm_model_t *model);
float dict_hmm_emit_prob(uint32_t codepoint, dict_hmm_state_t state);
int dict_hmm_viterbi(const dict_hmm_model_t *model, const dict_rune_t *runes,
                     int32_t rune_count, int32_t *labels);
int dict_hmm_segment(const char *text, const dict_rune_t *runes,
                     const int32_t *labels, int32_t rune_count,
                     token_t **tokens, int32_t *token_count, int32_t *token_capacity);

#endif