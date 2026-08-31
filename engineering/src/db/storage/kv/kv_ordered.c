/**
 * @file kv_ordered.c
 * @brief KV 有序集合实现 - 跳表版本
 *
 * 使用跳表实现有序集合，O(log n) 查找/插入/删除
 */

#include "db/storage/kv/kv_ordered.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ========================================================================
 * 跳表常量
 * ======================================================================== */

#define SKIP_LIST_MAX_LEVEL 32
#define SKIP_LIST_PROBABILITY 0.5

/* ========================================================================
 * 跳表节点
 * ======================================================================== */

typedef struct KvZsetSkipNode_s {
    char *member;                 /**< 成员字符串 */
    double score;                 /**< 分数 */
    struct KvZsetSkipNode_s *next[]; /**< 跳表指针数组 */
} KvZsetSkipNode;

/** 跳表结构 */
typedef struct {
    KvZsetSkipNode *head;         /**< 头节点 */
    int max_level;                /**< 当前最大层级 */
    size_t num_members;           /**< 成员数量 */
} KvZsetSkipList;

/** 跳表迭代器 */
typedef struct {
    KvZsetSkipNode *current;
    KvZsetSkipList *list;
} KvZsetSkipIter;

/* ========================================================================
 * 跳表工具函数
 * ======================================================================== */

static int random_level(void) {
    int level = 1;
    while (level < SKIP_LIST_MAX_LEVEL && ((rand() & 0xFFFF) < (SKIP_LIST_PROBABILITY * 0xFFFF))) {
        level++;
    }
    return level;
}

static KvZsetSkipNode *skip_node_create(const char *member, double score, int level) {
    KvZsetSkipNode *node = (KvZsetSkipNode *)malloc(
        sizeof(KvZsetSkipNode) + level * sizeof(KvZsetSkipNode *));
    if (!node) return NULL;

    node->member = strdup(member);
    node->score = score;

    for (int i = 0; i < level; i++) {
        node->next[i] = NULL;
    }
    return node;
}

static void skip_node_destroy(KvZsetSkipNode *node) {
    if (!node) return;
    free(node->member);
    free(node);
}

static KvZsetSkipList *skip_list_create(void) {
    KvZsetSkipList *sl = (KvZsetSkipList *)calloc(1, sizeof(KvZsetSkipList));
    if (!sl) return NULL;

    sl->max_level = 1;
    sl->head = skip_node_create("", 0.0, SKIP_LIST_MAX_LEVEL);
    if (!sl->head) {
        free(sl);
        return NULL;
    }

    return sl;
}

static void skip_list_destroy(KvZsetSkipList *sl) {
    if (!sl) return;

    KvZsetSkipNode *node = sl->head;
    while (node) {
        KvZsetSkipNode *next = node->next[0];
        skip_node_destroy(node);
        node = next;
    }
    free(sl);
}

static KvZsetSkipNode *skip_list_find(KvZsetSkipList *sl, const char *member, KvZsetSkipNode **prev) {
    if (!sl || !member) return NULL;

    KvZsetSkipNode *update[SKIP_LIST_MAX_LEVEL];
    KvZsetSkipNode *current = sl->head;

    /* 按 (score, member) 排序查找 */
    for (int i = sl->max_level - 1; i >= 0; i--) {
        while (current->next[i]) {
            int cmp = strcmp(current->next[i]->member, member);
            if (cmp == 0) {
                if (prev) *prev = current;
                return current->next[i];
            }
            if (cmp > 0) {
                break;
            }
            current = current->next[i];
        }
        update[i] = current;
    }

    if (prev) *prev = update[0];
    return NULL;
}

static int skip_list_insert(KvZsetSkipList *sl, const char *member, double score) {
    if (!sl || !member) return -1;

    KvZsetSkipNode *update[SKIP_LIST_MAX_LEVEL];
    KvZsetSkipNode *current = sl->head;

    /* 查找插入位置 */
    for (int i = sl->max_level - 1; i >= 0; i--) {
        while (current->next[i]) {
            int cmp = strcmp(current->next[i]->member, member);
            if (cmp == 0) {
                /* 已存在，更新分数 */
                current->next[i]->score = score;
                return 0;
            }
            if (cmp > 0) {
                break;
            }
            current = current->next[i];
        }
        update[i] = current;
    }

    /* 生成随机层数 */
    int level = random_level();

    /* 如果需要，增加层数 */
    while (sl->max_level < level) {
        sl->max_level++;
    }

    /* 创建新节点 */
    KvZsetSkipNode *new_node = skip_node_create(member, score, level);
    if (!new_node) return -1;

    /* 插入到各层 */
    for (int i = 0; i < level; i++) {
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
    }

    sl->num_members++;
    return 1;
}

static int skip_list_remove(KvZsetSkipList *sl, const char *member) {
    if (!sl || !member) return -1;

    KvZsetSkipNode *update[SKIP_LIST_MAX_LEVEL];
    KvZsetSkipNode *current = sl->head;

    /* 查找要删除的节点 */
    for (int i = sl->max_level - 1; i >= 0; i--) {
        while (current->next[i]) {
            int cmp = strcmp(current->next[i]->member, member);
            if (cmp == 0) {
                /* 找到，删除 */
                for (int j = i; j < sl->max_level; j++) {
                    if (update[j]->next[j] != current->next[i]) break;
                    update[j]->next[j] = current->next[i]->next[j];
                }
                skip_node_destroy(current->next[i]);
                sl->num_members--;
                return 0;
            }
            if (cmp > 0) {
                break;
            }
            current = current->next[i];
        }
        update[i] = current;
    }

    return -1; /* 未找到 */
}

static bool skip_list_get(KvZsetSkipList *sl, const char *member, double *out_score) {
    KvZsetSkipNode *prev = NULL;
    KvZsetSkipNode *node = skip_list_find(sl, member, &prev);
    if (node) {
        if (out_score) *out_score = node->score;
        return true;
    }
    return false;
}

static size_t skip_list_rank(KvZsetSkipList *sl, const char *member) {
    if (!sl || !member) return 0;

    size_t rank = 0;
    KvZsetSkipNode *current = sl->head->next[0];

    while (current) {
        if (strcmp(current->member, member) == 0) {
            return rank;
        }
        rank++;
        current = current->next[0];
    }
    return 0;
}

static size_t skip_list_count_range(KvZsetSkipList *sl, double min_score, double max_score) {
    if (!sl) return 0;

    size_t count = 0;
    KvZsetSkipNode *current = sl->head->next[0];

    while (current) {
        if (current->score >= min_score && current->score <= max_score) {
            count++;
        }
        current = current->next[0];
    }
    return count;
}

static KvZsetSkipIter *skip_list_create_iter(KvZsetSkipList *sl) {
    if (!sl) return NULL;
    KvZsetSkipIter *iter = (KvZsetSkipIter *)calloc(1, sizeof(KvZsetSkipIter));
    if (!iter) return NULL;
    iter->list = sl;
    iter->current = sl->head->next[0];
    return iter;
}

static bool skip_list_iter_next(KvZsetSkipIter *iter) {
    if (!iter || !iter->current) return false;
    iter->current = iter->current->next[0];
    return iter->current != NULL;
}

static void skip_list_iter_free(KvZsetSkipIter *iter) {
    free(iter);
}

/* ========================================================================
 * 有序集合实现
 * ======================================================================== */

KvZset *kv_zset_create(void *mem_pool) {
    (void)mem_pool;
    KvZset *zset = (KvZset *)calloc(1, sizeof(KvZset));
    if (zset) {
        zset->head = NULL;
        zset->num_members = 0;
        zset->max_level = 0;
        zset->mem_pool = skip_list_create();
    }
    return zset;
}

void kv_zset_destroy(KvZset *zset) {
    if (!zset) return;

    if (zset->mem_pool) {
        skip_list_destroy((KvZsetSkipList *)zset->mem_pool);
    }
    free(zset);
}

int kv_zset_add(KvZset *zset, const char *member, double score) {
    if (!zset || !member) return -1;

    if (!zset->mem_pool) {
        zset->mem_pool = skip_list_create();
        if (!zset->mem_pool) return -1;
    }

    int result = skip_list_insert((KvZsetSkipList *)zset->mem_pool, member, score);
    if (result >= 0) {
        zset->num_members = ((KvZsetSkipList *)zset->mem_pool)->num_members;
        zset->max_level = ((KvZsetSkipList *)zset->mem_pool)->max_level;
    }
    return result;
}

int kv_zset_remove(KvZset *zset, const char *member) {
    if (!zset || !member) return -1;

    int result = skip_list_remove((KvZsetSkipList *)zset->mem_pool, member);
    if (result == 0) {
        zset->num_members = ((KvZsetSkipList *)zset->mem_pool)->num_members;
    }
    return result;
}

bool kv_zset_score(KvZset *zset, const char *member, double *out_score) {
    if (!zset || !member) return false;
    return skip_list_get((KvZsetSkipList *)zset->mem_pool, member, out_score);
}

bool kv_zset_rank(KvZset *zset, const char *member, size_t *out_rank) {
    if (!zset || !member) return false;

    size_t rank = skip_list_rank((KvZsetSkipList *)zset->mem_pool, member);
    if (out_rank) *out_rank = rank;
    return rank > 0 || (zset->num_members > 0 && strcmp(((KvZsetSkipList *)zset->mem_pool)->head->next[0]->member, member) == 0);
}

bool kv_zset_rev_rank(KvZset *zset, const char *member, size_t *out_rank) {
    if (!zset || !member) return false;

    size_t rank = skip_list_rank((KvZsetSkipList *)zset->mem_pool, member);
    if (rank > 0 || (zset->num_members > 0 && strcmp(((KvZsetSkipList *)zset->mem_pool)->head->next[0]->member, member) == 0)) {
        if (out_rank) *out_rank = zset->num_members - 1 - rank;
        return true;
    }
    return false;
}

size_t kv_zset_card(KvZset *zset) {
    return zset ? zset->num_members : 0;
}

size_t kv_zset_count(KvZset *zset, double min_score, double max_score) {
    if (!zset) return 0;
    return skip_list_count_range((KvZsetSkipList *)zset->mem_pool, min_score, max_score);
}

/* ========================================================================
 * 范围查询
 * ======================================================================== */

KvZrangeResult *kv_zset_range_by_score(KvZset *zset,
                                       double min_score,
                                       double max_score,
                                       size_t offset,
                                       size_t limit,
                                       bool with_scores) {
    if (!zset) return NULL;

    KvZrangeResult *result = (KvZrangeResult *)calloc(1, sizeof(KvZrangeResult));
    if (!result) return NULL;

    result->members = (char **)malloc(limit * sizeof(char *));
    result->scores = with_scores ? (double *)malloc(limit * sizeof(double)) : NULL;

    KvZsetSkipList *sl = (KvZsetSkipList *)zset->mem_pool;
    KvZsetSkipNode *curr = sl ? sl->head->next[0] : NULL;
    size_t skip = 0;
    size_t collected = 0;

    while (curr && collected < limit) {
        if (curr->score >= min_score && curr->score <= max_score) {
            if (skip < offset) {
                skip++;
            } else {
                result->members[collected] = strdup(curr->member);
                if (with_scores) {
                    result->scores[collected] = curr->score;
                }
                collected++;
            }
        }
        curr = curr->next[0];
    }

    result->count = collected;
    return result;
}

KvZrangeResult *kv_zset_range(KvZset *zset,
                              size_t start_rank,
                              size_t stop_rank,
                              bool with_scores,
                              bool reverse) {
    if (!zset) return NULL;

    KvZrangeResult *result = (KvZrangeResult *)calloc(1, sizeof(KvZrangeResult));
    if (!result) return NULL;

    size_t count = stop_rank >= start_rank ? stop_rank - start_rank + 1 : 0;
    result->members = (char **)malloc(count * sizeof(char *));
    result->scores = with_scores ? (double *)malloc(count * sizeof(double)) : NULL;

    KvZsetSkipList *sl = (KvZsetSkipList *)zset->mem_pool;
    KvZsetSkipNode *curr = sl ? sl->head->next[0] : NULL;
    size_t idx = 0;

    while (curr && idx <= stop_rank) {
        if (idx >= start_rank) {
            size_t pos = reverse ? (count - 1 - (idx - start_rank)) : (idx - start_rank);
            result->members[pos] = strdup(curr->member);
            if (with_scores) {
                result->scores[pos] = curr->score;
            }
        }
        idx++;
        curr = curr->next[0];
    }

    result->count = (idx > start_rank) ? (idx > stop_rank ? stop_rank - start_rank + 1 : idx - start_rank) : 0;
    return result;
}

void kv_zset_result_free(KvZrangeResult *result) {
    if (!result) return;
    for (size_t i = 0; i < result->count; i++) {
        free(result->members[i]);
    }
    free(result->members);
    free(result->scores);
    free(result);
}

/* ========================================================================
 * 集合操作
 * ======================================================================== */

size_t kv_zset_union(KvZset **dest,
                    size_t num_keys,
                    KvZset **keys,
                    const double *weights,
                    int aggregate) {
    if (!keys || num_keys == 0) return 0;

    KvZset *result = kv_zset_create(NULL);
    if (!result) return 0;

    for (size_t k = 0; k < num_keys; k++) {
        if (!keys[k]) continue;

        double weight = weights ? weights[k] : 1.0;
        KvZsetSkipList *sl = (KvZsetSkipList *)keys[k]->mem_pool;
        if (!sl) continue;

        KvZsetSkipNode *curr = sl->head->next[0];
        while (curr) {
            double score = curr->score * weight;

            /* 查找是否已存在 */
            double existing_score;
            if (kv_zset_score(result, curr->member, &existing_score)) {
                switch (aggregate) {
                    case 1: score = (existing_score < score) ? existing_score : score; break; /* MIN */
                    case 2: score = (existing_score > score) ? existing_score : score; break; /* MAX */
                    default: score += existing_score; break; /* SUM */
                }
                kv_zset_remove(result, curr->member);
            }

            kv_zset_add(result, curr->member, score);
            curr = curr->next[0];
        }
    }

    size_t count = result->num_members;
    *dest = result;
    return count;
}

size_t kv_zset_inter(KvZset **dest,
                    size_t num_keys,
                    KvZset **keys,
                    const double *weights,
                    int aggregate) {
    if (!keys || num_keys == 0) return 0;

    KvZset *result = kv_zset_create(NULL);
    if (!result) return 0;

    /* 取第一个集合作为基础 */
    if (num_keys == 0 || !keys[0]) {
        kv_zset_destroy(result);
        return 0;
    }

    KvZsetSkipList *sl0 = (KvZsetSkipList *)keys[0]->mem_pool;
    if (!sl0) {
        kv_zset_destroy(result);
        return 0;
    }

    KvZsetSkipNode *curr = sl0->head->next[0];
    while (curr) {
        bool exists_in_all = true;
        double total_score = curr->score * (weights ? weights[0] : 1.0);

        for (size_t k = 1; k < num_keys; k++) {
            if (!keys[k]) {
                exists_in_all = false;
                break;
            }
            double score;
            if (kv_zset_score(keys[k], curr->member, &score)) {
                total_score += score * (weights ? weights[k] : 1.0);
            } else {
                exists_in_all = false;
                break;
            }
        }

        if (exists_in_all) {
            switch (aggregate) {
                case 1: total_score = total_score / num_keys; break; /* MIN */
                case 2: break; /* MAX - 保持原值 */
                default: total_score = total_score / num_keys; break; /* SUM - 平均 */
            }
            kv_zset_add(result, curr->member, total_score);
        }

        curr = curr->next[0];
    }

    size_t count = result->num_members;
    *dest = result;
    return count;
}

/* ========================================================================
 * 增量操作
 * ======================================================================== */

double kv_zset_incrby(KvZset *zset, const char *member, double increment,
                     double *out_new_score) {
    if (!zset || !member) return 0;

    double score = 0;
    bool exists = kv_zset_score(zset, member, &score);

    score += increment;
    kv_zset_add(zset, member, score);

    if (out_new_score) *out_new_score = score;
    return score;
}

/* ========================================================================
 * SQL 函数（简化实现）
 * ======================================================================== */

static KvZset *g_default_zset = NULL;

int kv_sql_zadd(const char *key, const char *member, double score) {
    (void)key;
    if (!g_default_zset) {
        g_default_zset = kv_zset_create(NULL);
    }
    return kv_zset_add(g_default_zset, member, score);
}

double kv_sql_zscore(const char *key, const char *member) {
    (void)key;
    double score = 0;
    if (g_default_zset) {
        kv_zset_score(g_default_zset, member, &score);
    }
    return score;
}

char *kv_sql_zrangebyscore(const char *key, double min, double max) {
    (void)key;
    KvZrangeResult *result = kv_zset_range_by_score(g_default_zset, min, max, 0, 100, true);
    if (!result) return strdup("[]");

    char *output = (char *)malloc(4096);
    char *p = output;
    p += sprintf(p, "[");

    for (size_t i = 0; i < result->count; i++) {
        if (i > 0) p += sprintf(p, ",");
        p += sprintf(p, "{\"member\":\"%s\",\"score\":%g}", result->members[i], result->scores[i]);
    }

    p += sprintf(p, "]");
    kv_zset_result_free(result);
    return output;
}

size_t kv_sql_zcard(const char *key) {
    (void)key;
    return g_default_zset ? kv_zset_card(g_default_zset) : 0;
}

size_t kv_sql_zcount(const char *key, double min, double max) {
    (void)key;
    return g_default_zset ? kv_zset_count(g_default_zset, min, max) : 0;
}