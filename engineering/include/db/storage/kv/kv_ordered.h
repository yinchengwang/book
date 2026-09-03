/**
 * @file kv_ordered.h
 * @brief KV 有序集合头文件
 *
 * 实现 Redis 风格的有序集合（Sorted Set）数据结构
 */
#ifndef DB_KV_ORDERED_H
#define DB_KV_ORDERED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 有序集合节点
 * ======================================================================== */

/** 有序集合节点 */
typedef struct KvZsetNode_s {
    char *member;                 /**< 成员字符串 */
    double score;                 /**< 分数 */
    struct KvZsetNode_s *next;    /**< 下一节点（链表） */
} KvZsetNode;

/** 有序集合（跳表实现） */
typedef struct KvZset_s {
    KvZsetNode *head;             /**< 链表头 */
    size_t num_members;           /**< 成员数量 */
    int max_level;                /**< 最大层级 */
    void *mem_pool;               /**< 内存池 */
} KvZset;

/** 有序集合范围查询结果 */
typedef struct KvZrangeResult_s {
    char **members;               /**< 成员数组 */
    double *scores;               /**< 分数数组 */
    size_t count;                 /**< 结果数量 */
} KvZrangeResult;

/* ========================================================================
 * 有序集合 API
 * ======================================================================== */

/**
 * @brief 创建有序集合
 */
KvZset *kv_zset_create(void *mem_pool);

/**
 * @brief 销毁有序集合
 */
void kv_zset_destroy(KvZset *zset);

/**
 * @brief 添加成员（ZADD）
 * @param zset 有序集合
 * @param member 成员
 * @param score 分数
 * @return 1 新增，0 更新，-1 失败
 */
int kv_zset_add(KvZset *zset, const char *member, double score);

/**
 * @brief 移除成员（ZREM）
 * @param zset 有序集合
 * @param member 成员
 * @return 0 成功，-1 未找到
 */
int kv_zset_remove(KvZset *zset, const char *member);

/**
 * @brief 获取成员分数（ZSCORE）
 * @param zset 有序集合
 * @param member 成员
 * @param out_score 输出分数
 * @return true 找到，false 未找到
 */
bool kv_zset_score(KvZset *zset, const char *member, double *out_score);

/**
 * @brief 获取成员排名（从小到大，ZRANK）
 * @param zset 有序集合
 * @param member 成员
 * @param out_rank 输出排名（0-based）
 * @return true 找到，false 未找到
 */
bool kv_zset_rank(KvZset *zset, const char *member, size_t *out_rank);

/**
 * @brief 获取成员排名（从大到小，ZREVRANK）
 */
bool kv_zset_rev_rank(KvZset *zset, const char *member, size_t *out_rank);

/**
 * @brief 获取成员数量（ZCARD）
 */
size_t kv_zset_card(KvZset *zset);

/**
 * @brief 获取分数范围内的成员数量（ZCOUNT）
 */
size_t kv_zset_count(KvZset *zset, double min_score, double max_score);

/* ========================================================================
 * 范围查询
 * ======================================================================== */

/**
 * @brief 按分数范围查询（ZRANGEBYSCORE）
 * @param zset 有序集合
 * @param min_score 最小分数
 * @param max_score 最大分数
 * @param offset 偏移
 * @param limit 限制数量
 * @param with_scores 是否包含分数
 * @return 查询结果
 */
KvZrangeResult *kv_zset_range_by_score(KvZset *zset,
                                       double min_score,
                                       double max_score,
                                       size_t offset,
                                       size_t limit,
                                       bool with_scores);

/**
 * @brief 按排名范围查询（ZRANGE/ZREVRANGE）
 * @param zset 有序集合
 * @param start_rank 起始排名
 * @param stop_rank 结束排名
 * @param with_scores 是否包含分数
 * @param reverse 是否反向（从大到小）
 * @return 查询结果
 */
KvZrangeResult *kv_zset_range(KvZset *zset,
                              size_t start_rank,
                              size_t stop_rank,
                              bool with_scores,
                              bool reverse);

/**
 * @brief 释放查询结果
 */
void kv_zset_result_free(KvZrangeResult *result);

/* ========================================================================
 * 集合操作
 * ======================================================================== */

/**
 * @brief 并集（ZUNIONSTORE）
 * @param dest 目标有序集合（可为 NULL，自动创建）
 * @param num_keys 来源数量
 * @param keys 来源键数组
 * @param weights 权重数组（可为 NULL）
 * @param aggregate 聚合方式（0=SUM, 1=MIN, 2=MAX）
 * @return 结果数量
 */
size_t kv_zset_union(KvZset **dest,
                    size_t num_keys,
                    KvZset **keys,
                    const double *weights,
                    int aggregate);

/**
 * @brief 交集（ZINTERSTORE）
 */
size_t kv_zset_inter(KvZset **dest,
                    size_t num_keys,
                    KvZset **keys,
                    const double *weights,
                    int aggregate);

/* ========================================================================
 * 增量操作
 * ======================================================================== */

/**
 * @brief 增加分数（ZINCRBY）
 * @param zset 有序集合
 * @param member 成员
 * @param increment 增量
 * @param out_new_score 新分数输出
 * @return 新分数
 */
double kv_zset_incrby(KvZset *zset, const char *member, double increment,
                     double *out_new_score);

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

/**
 * @brief ZADD SQL 函数
 *
 * ZADD(key, member, score)
 */
int kv_sql_zadd(const char *key, const char *member, double score);

/**
 * @brief ZSCORE SQL 函数
 *
 * ZSCORE(key, member)
 */
double kv_sql_zscore(const char *key, const char *member);

/**
 * @brief ZRANGEBYSCORE SQL 函数
 *
 * ZRANGEBYSCORE(key, min, max)
 */
char *kv_sql_zrangebyscore(const char *key, double min, double max);

/**
 * @brief ZCARD SQL 函数
 *
 * ZCARD(key)
 */
size_t kv_sql_zcard(const char *key);

/**
 * @brief ZCOUNT SQL 函数
 *
 * ZCOUNT(key, min, max)
 */
size_t kv_sql_zcount(const char *key, double min, double max);

#ifdef __cplusplus
}
#endif

#endif /* DB_KV_ORDERED_H */
