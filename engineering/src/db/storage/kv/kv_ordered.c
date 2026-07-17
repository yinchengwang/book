/**
 * @file kv_ordered.c
 * @brief KV 有序集合实现
 */

#include "db/storage/kv/kv_ordered.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    }
    return zset;
}

void kv_zset_destroy(KvZset *zset) {
    if (!zset) return;

    KvZsetNode *node = zset->head;
    while (node) {
        KvZsetNode *next = node->next;
        free(node->member);
        free(node);
        node = next;
    }
    free(zset);
}

int kv_zset_add(KvZset *zset, const char *member, double score) {
    if (!zset || !member) return -1;

    /* 查找是否已存在 */
    KvZsetNode *prev = NULL;
    KvZsetNode *curr = zset->head;

    while (curr && strcmp(curr->member, member) < 0) {
        prev = curr;
        curr = curr->next;
    }

    if (curr && strcmp(curr->member, member) == 0) {
        /* 更新 */
        curr->score = score;
        return 0;
    }

    /* 新增 */
    KvZsetNode *new_node = (KvZsetNode *)calloc(1, sizeof(KvZsetNode));
    if (!new_node) return -1;

    new_node->member = strdup(member);
    new_node->score = score;

    if (prev) {
        new_node->next = prev->next;
        prev->next = new_node;
    } else {
        new_node->next = zset->head;
        zset->head = new_node;
    }

    zset->num_members++;
    return 1;
}

int kv_zset_remove(KvZset *zset, const char *member) {
    if (!zset || !member) return -1;

    KvZsetNode *prev = NULL;
    KvZsetNode *curr = zset->head;

    while (curr && strcmp(curr->member, member) < 0) {
        prev = curr;
        curr = curr->next;
    }

    if (!curr || strcmp(curr->member, member) != 0) {
        return -1;
    }

    if (prev) {
        prev->next = curr->next;
    } else {
        zset->head = curr->next;
    }

    free(curr->member);
    free(curr);
    zset->num_members--;
    return 0;
}

bool kv_zset_score(KvZset *zset, const char *member, double *out_score) {
    if (!zset || !member) return false;

    KvZsetNode *curr = zset->head;
    while (curr && strcmp(curr->member, member) < 0) {
        curr = curr->next;
    }

    if (curr && strcmp(curr->member, member) == 0) {
        if (out_score) *out_score = curr->score;
        return true;
    }
    return false;
}

bool kv_zset_rank(KvZset *zset, const char *member, size_t *out_rank) {
    if (!zset || !member) return false;

    size_t rank = 0;
    KvZsetNode *curr = zset->head;

    while (curr) {
        if (strcmp(curr->member, member) == 0) {
            if (out_rank) *out_rank = rank;
            return true;
        }
        rank++;
        curr = curr->next;
    }
    return false;
}

bool kv_zset_rev_rank(KvZset *zset, const char *member, size_t *out_rank) {
    if (!zset || !member) return false;

    size_t rank = 0;
    KvZsetNode *curr = zset->head;

    while (curr) {
        if (strcmp(curr->member, member) == 0) {
            if (out_rank) *out_rank = zset->num_members - 1 - rank;
            return true;
        }
        rank++;
        curr = curr->next;
    }
    return false;
}

size_t kv_zset_card(KvZset *zset) {
    return zset ? zset->num_members : 0;
}

size_t kv_zset_count(KvZset *zset, double min_score, double max_score) {
    if (!zset) return 0;

    size_t count = 0;
    KvZsetNode *curr = zset->head;

    while (curr) {
        if (curr->score >= min_score && curr->score <= max_score) {
            count++;
        }
        curr = curr->next;
    }
    return count;
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

    KvZsetNode *curr = zset->head;
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
        curr = curr->next;
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

    size_t idx = 0;
    KvZsetNode *curr = zset->head;

    while (curr && idx <= stop_rank) {
        if (idx >= start_rank) {
            size_t pos = reverse ? (count - 1 - (idx - start_rank)) : (idx - start_rank);
            result->members[pos] = strdup(curr->member);
            if (with_scores) {
                result->scores[pos] = curr->score;
            }
        }
        idx++;
        curr = curr->next;
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
        KvZsetNode *curr = keys[k]->head;

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
            curr = curr->next;
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

    KvZsetNode *curr = keys[0]->head;
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

        curr = curr->next;
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
