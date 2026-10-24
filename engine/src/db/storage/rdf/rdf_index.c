/**
 * @file rdf_index.c
 * @brief RDF 索引实现
 *
 * 提供主语、谓语、宾语的哈希索引，加速三元组模式匹配。
 */
#include "rdf_engine.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

/* 索引节点 */
typedef struct index_node_s {
    uint64_t hash;                  /**< 键哈希值 */
    rdf_term_t term;               /**< 术语值 */
    int64_t triple_ids[1024];       /**< 关联的三元组 ID 列表 */
    int32_t count;                  /**< 关联数量 */
    struct index_node_s *next;      /**< 哈希链 */
} index_node_t;

/* 索引表 */
typedef struct rdf_index_s {
    index_node_t **buckets;         /**< 哈希桶数组 */
    int32_t num_buckets;            /**< 桶数量 */
    int32_t entry_count;            /**< 条目数量 */
} rdf_index_t;

/* ========================================================================
 * 索引操作
 * ======================================================================== */

/** 创建索引 */
static rdf_index_t *rdf_index_create(int32_t num_buckets) {
    rdf_index_t *index = (rdf_index_t *)calloc(1, sizeof(rdf_index_t));
    if (!index) return NULL;

    index->buckets = (index_node_t **)calloc(num_buckets, sizeof(index_node_t *));
    index->num_buckets = num_buckets;
    index->entry_count = 0;

    return index;
}

/** 销毁索引 */
static void rdf_index_destroy(rdf_index_t *index) {
    if (!index) return;

    for (int32_t i = 0; i < index->num_buckets; i++) {
        index_node_t *node = index->buckets[i];
        while (node) {
            index_node_t *next = node->next;
            free(node);
            node = next;
        }
    }
    free(index->buckets);
    free(index);
}

/** 插入条目 */
static int rdf_index_insert(rdf_index_t *index, const rdf_term_t *term, int64_t triple_id) {
    uint64_t hash = hash_term(term);
    int32_t bucket = hash % index->num_buckets;

    index_node_t *node = index->buckets[bucket];
    while (node) {
        if (rdf_term_equals(&node->term, term)) {
            /* 已存在，追加 triple_id */
            if (node->count < 1024) {
                node->triple_ids[node->count++] = triple_id;
            }
            return 0;
        }
        node = node->next;
    }

    /* 创建新节点 */
    node = (index_node_t *)calloc(1, sizeof(index_node_t));
    if (!node) return -1;

    node->hash = hash;
    node->term = *term;
    node->triple_ids[0] = triple_id;
    node->count = 1;
    node->next = index->buckets[bucket];
    index->buckets[bucket] = node;
    index->entry_count++;

    return 0;
}

/** 查询条目 */
static int rdf_index_search(rdf_index_t *index, const rdf_term_t *term,
                              int64_t *triple_ids, int32_t max_results,
                              int32_t *num_results) {
    if (!index || !term) return -1;

    uint64_t hash = hash_term(term);
    int32_t bucket = hash % index->num_buckets;

    index_node_t *node = index->buckets[bucket];
    while (node) {
        if (rdf_term_equals(&node->term, term)) {
            int32_t count = node->count < max_results ? node->count : max_results;
            memcpy(triple_ids, node->triple_ids, count * sizeof(int64_t));
            *num_results = count;
            return 0;
        }
        node = node->next;
    }

    *num_results = 0;
    return 0;
}

/* 导出给 rdf_engine.c 使用 */
#include "rdf_index.h"

/** 获取或创建全局索引 */
static rdf_index_t *g_subject_index = NULL;
static rdf_index_t *g_predicate_index = NULL;
static rdf_index_t *g_object_index = NULL;

int rdf_index_init(void) {
    g_subject_index = rdf_index_create(4096);
    g_predicate_index = rdf_index_create(4096);
    g_object_index = rdf_index_create(4096);

    if (!g_subject_index || !g_predicate_index || !g_object_index) {
        rdf_index_shutdown();
        return -1;
    }

    return 0;
}

void rdf_index_shutdown(void) {
    rdf_index_destroy(g_subject_index);
    rdf_index_destroy(g_predicate_index);
    rdf_index_destroy(g_object_index);
    g_subject_index = NULL;
    g_predicate_index = NULL;
    g_object_index = NULL;
}

int rdf_index_add_triple(int64_t triple_id, const rdf_triple_t *triple) {
    if (!triple) return -1;

    rdf_index_insert(g_subject_index, &triple->subject, triple_id);
    rdf_index_insert(g_predicate_index, &triple->predicate, triple_id);
    rdf_index_insert(g_object_index, &triple->object, triple_id);

    return 0;
}
