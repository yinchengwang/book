/**
 * @file nodeHashagg.c
 * @brief HashAgg 执行器节点实现
 *
 * 实现 Task 0.5 的 HashAgg 哈希聚合节点：
 *   - ExecInitHashAgg: 初始化哈希表，连接子节点
 *   - ExecHashAgg: 两阶段执行（构建分组哈希表 → 输出分组结果）
 *   - ExecEndHashAgg: 释放哈希表和子节点
 *   - ExecReScanHashAgg: 重置状态
 *   - make_hashagg_plan: 构造函数
 *
 * 基于 Volcano 迭代器模型。
 */

#include "db/sql/nodes/nodeHashagg.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * 简化版元组槽操作（不依赖外部函数）
 * ======================================================================== */

/**
 * @brief 创建简化的元组槽
 */
static TupleTableSlot *make_simple_slot(void) {
    TupleTableSlot *slot = (TupleTableSlot *)malloc(sizeof(TupleTableSlot));
    if (slot) {
        memset(slot, 0, sizeof(TupleTableSlot));
        slot->tts_nvalid = 0;
    }
    return slot;
}

/**
 * @brief 释放简化的元组槽
 */
static void free_simple_slot(TupleTableSlot *slot) {
    if (slot) {
        if (slot->tts_values) free(slot->tts_values);
        if (slot->tts_isnull) free(slot->tts_isnull);
        free(slot);
    }
}

/**
 * @brief 创建简化的表达式上下文
 */
static ExprContext *make_simple_expr_context(void) {
    ExprContext *ctx = (ExprContext *)malloc(sizeof(ExprContext));
    if (ctx) {
        memset(ctx, 0, sizeof(ExprContext));
    }
    return ctx;
}

/* ========================================================================
 * 哈希表相关类型和常量
 * ======================================================================== */

/**
 * @brief 哈希表桶节点
 */
typedef struct HashAggBucket {
    void *group_key;              /**< 分组键值 */
    void *aggregates;             /**< 聚合结果 */
    struct HashAggBucket *next;   /**< 下一个桶 */
} HashAggBucket;

/**
 * @brief 哈希表
 */
typedef struct HashAggHashTable {
    HashAggBucket **buckets;      /**< 桶数组 */
    int num_buckets;              /**< 桶数量 */
    int num_entries;              /**< 条目数 */
    int key_size;                 /**< 键大小 */
    int value_size;              /**< 值大小 */
} HashAggHashTable;

/* ========================================================================
 * 哈希函数
 * ======================================================================== */

/**
 * @brief 简单的哈希函数
 *
 * @param key 键
 * @param key_size 键大小
 * @param num_buckets 桶数量
 * @return 哈希值
 */
static uint32_t hash_agg_hash(const void *key, int key_size, int num_buckets) {
    const unsigned char *p = (const unsigned char *)key;
    uint32_t h = 0;
    int i;

    /* 简单哈希：基于每个字节 */
    for (i = 0; i < key_size; i++) {
        h = h * 31 + p[i];
    }

    return h % num_buckets;
}

/**
 * @brief 比较两个键
 *
 * @param a 键 a
 * @param b 键 b
 * @param key_size 键大小
 * @return 是否相等
 */
static bool hash_agg_equal(const void *a, const void *b, int key_size) {
    return memcmp(a, b, key_size) == 0;
}

/* ========================================================================
 * 哈希表操作
 * ======================================================================== */

/**
 * @brief 创建哈希表
 *
 * @param num_group_cols 分组列数
 * @param key_size 键大小
 * @param value_size 值大小
 * @return 哈希表指针
 */
static HashAggHashTable *hash_agg_create_table(int num_group_cols, int key_size, int value_size) {
    HashAggHashTable *ht;

    (void)num_group_cols;  /* 未使用 */

    ht = (HashAggHashTable *)malloc(sizeof(HashAggHashTable));
    if (ht == NULL) {
        return NULL;
    }

    /* 初始化桶数组（初始大小 1024） */
    ht->num_buckets = 1024;
    ht->buckets = (HashAggBucket **)calloc(ht->num_buckets, sizeof(HashAggBucket *));
    if (ht->buckets == NULL) {
        free(ht);
        return NULL;
    }

    ht->num_entries = 0;
    ht->key_size = key_size;
    ht->value_size = value_size;

    return ht;
}

/**
 * @brief 销毁哈希表
 *
 * @param ht 哈希表指针
 */
static void hash_agg_destroy_table(HashAggHashTable *ht) {
    int i;
    HashAggBucket *bucket;
    HashAggBucket *next;

    if (ht == NULL) {
        return;
    }

    /* 释放所有桶节点 */
    for (i = 0; i < ht->num_buckets; i++) {
        bucket = ht->buckets[i];
        while (bucket != NULL) {
            next = bucket->next;
            free(bucket->group_key);
            free(bucket->aggregates);
            free(bucket);
            bucket = next;
        }
    }

    free(ht->buckets);
    free(ht);
}

/**
 * @brief 查找或插入分组
 *
 * @param ht 哈希表
 * @param key 分组键
 * @param found 输出：是否新插入
 * @return 聚合值指针
 */
static void *hash_agg_lookup_or_insert(HashAggHashTable *ht, const void *key, bool *found) {
    uint32_t h;
    HashAggBucket *bucket;

    h = hash_agg_hash(key, ht->key_size, ht->num_buckets);
    bucket = ht->buckets[h];

    /* 查找现有分组 */
    while (bucket != NULL) {
        if (hash_agg_equal(bucket->group_key, key, ht->key_size)) {
            *found = false;
            return bucket->aggregates;
        }
        bucket = bucket->next;
    }

    /* 新建分组 */
    bucket = (HashAggBucket *)malloc(sizeof(HashAggBucket));
    if (bucket == NULL) {
        return NULL;
    }

    bucket->group_key = malloc(ht->key_size);
    bucket->aggregates = malloc(ht->value_size);
    if (bucket->group_key == NULL || bucket->aggregates == NULL) {
        free(bucket->group_key);
        free(bucket->aggregates);
        free(bucket);
        return NULL;
    }

    memcpy(bucket->group_key, key, ht->key_size);
    memset(bucket->aggregates, 0, ht->value_size);

    /* 插入桶链表头部 */
    bucket->next = ht->buckets[h];
    ht->buckets[h] = bucket;
    ht->num_entries++;

    *found = true;
    return bucket->aggregates;
}

/**
 * @brief 获取哈希表遍历位置
 *
 * @param ht 哈希表
 * @param bucket_idx 桶索引
 * @return 下一个桶节点，如果没有更多则返回 NULL
 */
static HashAggBucket *hash_agg_next_entry(HashAggHashTable *ht, int bucket_idx) {
    HashAggBucket *bucket;

    while (bucket_idx < ht->num_buckets) {
        bucket = ht->buckets[bucket_idx];
        if (bucket != NULL) {
            return bucket;
        }
        bucket_idx++;
    }

    return NULL;
}

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 从元组槽提取分组键
 *
 * @param slot 元组槽
 * @param grp_col_idx 分组列索引
 * @param num_group_cols 分组列数
 * @param key_buf 输出键缓冲区
 */
static void extract_group_key(TupleTableSlot *slot, int *grp_col_idx,
                              int num_group_cols, void *key_buf) {
    char *buf = (char *)key_buf;
    int i;

    for (i = 0; i < num_group_cols; i++) {
        int col_idx = grp_col_idx[i] - 1;  /* 转换为 0 基索引 */
        void *value;
        int len;

        if (col_idx >= 0 && col_idx < slot->tts_nvalid) {
            value = slot->tts_values[col_idx];
            /* 使用固定大小 8 字节 */
            len = 8;
        } else {
            value = NULL;
            len = 8;
        }

        if (value != NULL) {
            memcpy(buf, value, len);
        } else {
            memset(buf, 0, len);
        }
        buf += 8;
    }
}

/* ========================================================================
 * HashAgg 节点执行函数
 * ======================================================================== */

/**
 * @brief HashAgg 执行函数
 *
 * 两阶段执行：
 * 1. 构建阶段：扫描子节点，将元组聚合到哈希表
 * 2. 输出阶段：遍历哈希表，输出每个分组的结果
 *
 * @param pstate PlanState（实际类型为 HashAggState）
 *
 * @return 结果元组槽；无更多分组时返回 NULL
 */
static TupleTableSlot *exec_hash_agg_impl(PlanState *pstate) {
    HashAggState *node = (HashAggState *)pstate;
    TupleTableSlot *slot;

    if (node == NULL) {
        return NULL;
    }

    /* 检查是否已完成 */
    if (node->agg_finished) {
        return NULL;
    }

    /* ========================================
     * 阶段 1：构建阶段 - 扫描输入并聚合
     * ======================================== */
    if (node->agg_build_phase) {
        PlanState *child = node->ps->left;

        /* 扫描子节点 */
        while (child != NULL && child->exec_proc != NULL) {
            slot = child->exec_proc(child);
            if (slot == NULL) {
                /* 输入扫描完成，切换到输出阶段 */
                break;
            }

            /* 提取分组键 */
            char key_buf[64];  /* 最多 8 列，每列 8 字节 */
            extract_group_key(slot, node->grp_col_idx,
                            node->num_group_cols, key_buf);

            /* 查找或插入分组 */
            bool found;
            HashAggHashTable *ht = (HashAggHashTable *)node->agg_hashTable;
            void *aggregates = hash_agg_lookup_or_insert(ht, key_buf, &found);

            if (aggregates != NULL) {
                /* 累积计数（简化实现：每组计数 +1） */
                int64_t *count = (int64_t *)aggregates;
                (*count)++;
            }

            node->agg_tuples++;
        }

        /* 切换到输出阶段 */
        node->agg_build_phase = false;
        node->agg_first_pass = false;
        node->agg_groups = ((HashAggHashTable *)node->agg_hashTable)->num_entries;

        /* 初始化遍历位置 */
        node->agg_currentGroup = (void *)0;  /* 从桶 0 开始 */
    }

    /* ========================================
     * 阶段 2：输出阶段 - 遍历哈希表输出
     * ======================================== */
    if (!node->agg_build_phase) {
        HashAggHashTable *ht = (HashAggHashTable *)node->agg_hashTable;
        HashAggBucket *bucket;
        int bucket_idx;

        bucket_idx = (int)(intptr_t)node->agg_currentGroup;
        bucket = hash_agg_next_entry(ht, bucket_idx);

        if (bucket == NULL) {
            /* 没有更多分组 */
            node->agg_finished = true;
            return NULL;
        }

        /* 更新遍历位置 */
        node->agg_currentGroup = (void *)(intptr_t)(bucket_idx + 1);

        /* 填充结果槽（简化：输出分组键 + 计数） */
        if (node->result_slot != NULL) {
            slot = node->result_slot;
            slot->tts_nvalid = node->num_group_cols + 1;

            /* 设置分组键 */
            int i;
            for (i = 0; i < node->num_group_cols && i < slot->tts_nvalid; i++) {
                slot->tts_values[i] = (char *)bucket->group_key + i * 8;
                slot->tts_isnull[i] = false;
            }

            /* 设置计数（第一个聚合值） */
            if (i < slot->tts_nvalid) {
                slot->tts_values[i] = bucket->aggregates;
                slot->tts_isnull[i] = false;
            }

            node->agg_output++;
            return slot;
        }

        return NULL;
    }

    return NULL;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 HashAgg 节点
 *
 * @param plan HashAgg 计划节点
 * @param estate 执行器状态
 * @param eflags 执行标志
 *
 * @return 初始化后的 HashAggState
 */
HashAggState *ExecInitHashAgg(HashAggPlan *plan, void *estate, int eflags) {
    HashAggState *state;
    int key_size;

    (void)eflags;  /* 未使用 */

    /* 参数检查 */
    if (plan == NULL) {
        return NULL;
    }

    /* 分配 HashAggState */
    state = (HashAggState *)malloc(sizeof(HashAggState));
    if (state == NULL) {
        return NULL;
    }
    memset(state, 0, sizeof(HashAggState));

    /* 分配并初始化基类 PlanState */
    state->ps = (PlanState *)malloc(sizeof(PlanState));
    if (state->ps == NULL) {
        free(state);
        return NULL;
    }
    memset(state->ps, 0, sizeof(PlanState));

    state->ps->type = EXEC_HASHAGG;
    state->ps->state = estate;
    state->ps->exec_proc = ExecHashAgg;
    state->ps->left = NULL;
    state->ps->right = NULL;

    /* 创建表达式上下文 */
    state->ps->expr_context = make_simple_expr_context();

    /* 创建结果槽（简化：使用简单槽） */
    state->result_slot = make_simple_slot();

    /* 复制分组信息 */
    state->num_group_cols = plan->numGroupCols;
    state->grp_col_idx = plan->grpColIdx;

    /* 创建哈希表 */
    key_size = plan->numGroupCols * 8;  /* 每列 8 字节 */
    state->agg_hashTable = hash_agg_create_table(plan->numGroupCols, key_size, sizeof(int64_t));
    if (state->agg_hashTable == NULL) {
        ExecEndHashAgg(state);
        return NULL;
    }

    /* 初始化状态标志 */
    state->agg_finished = false;
    state->agg_first_pass = true;
    state->agg_build_phase = true;

    /* 初始化统计信息 */
    state->agg_groups = 0;
    state->agg_tuples = 0;
    state->agg_output = 0;

    return state;
}

/**
 * @brief 执行 HashAgg
 *
 * @param pstate PlanState（实际类型为 HashAggState）
 *
 * @return 结果元组槽；无更多分组时返回 NULL
 */
TupleTableSlot *ExecHashAgg(PlanState *pstate) {
    return exec_hash_agg_impl(pstate);
}

/**
 * @brief 结束 HashAgg 节点
 *
 * @param node HashAggState
 */
void ExecEndHashAgg(HashAggState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放子节点 */
    if (node->ps != NULL && node->ps->left != NULL) {
        /* 假设子节点有 ExecEndNode，这里简化处理 */
        node->ps->left = NULL;
    }

    /* 释放哈希表 */
    if (node->agg_hashTable != NULL) {
        hash_agg_destroy_table((HashAggHashTable *)node->agg_hashTable);
        node->agg_hashTable = NULL;
    }

    /* 释放表达式上下文 */
    if (node->ps != NULL && node->ps->expr_context != NULL) {
        exec_destroy_expr_context(node->ps->expr_context);
        node->ps->expr_context = NULL;
    }

    /* 释放结果槽 */
    if (node->result_slot != NULL) {
        free_simple_slot(node->result_slot);
        node->result_slot = NULL;
    }

    /* 释放 PlanState */
    if (node->ps != NULL) {
        free(node->ps);
        node->ps = NULL;
    }

    /* 释放 HashAggState */
    free(node);
}

/**
 * @brief 重置 HashAgg 节点
 *
 * @param node HashAggState
 */
void ExecReScanHashAgg(HashAggState *node) {
    if (node == NULL) {
        return;
    }

    /* 重置哈希表 */
    if (node->agg_hashTable != NULL) {
        hash_agg_destroy_table((HashAggHashTable *)node->agg_hashTable);

        /* 重新创建哈希表 */
        int key_size = node->num_group_cols * 8;
        node->agg_hashTable = hash_agg_create_table(node->num_group_cols, key_size, sizeof(int64_t));
    }

    /* 重置状态标志 */
    node->agg_finished = false;
    node->agg_first_pass = true;
    node->agg_build_phase = true;

    /* 重置统计信息 */
    node->agg_groups = 0;
    node->agg_tuples = 0;
    node->agg_output = 0;

    /* 重置遍历位置 */
    node->agg_currentGroup = NULL;
}

/**
 * @brief 获取 HashAgg 子节点
 *
 * @param node HashAggState
 * @return PlanState 子节点指针
 */
PlanState *ExecGetHashAggChild(HashAggState *node) {
    if (node == NULL || node->ps == NULL) {
        return NULL;
    }
    return node->ps->left;
}

/**
 * @brief 设置 HashAgg 子节点
 *
 * @param node HashAggState
 * @param child 子节点 PlanState
 */
void ExecSetHashAggChild(HashAggState *node, PlanState *child) {
    if (node == NULL || node->ps == NULL) {
        return;
    }
    node->ps->left = child;
}

/**
 * @brief 创建 HashAgg 计划节点
 *
 * @param numGroupCols 分组列数
 * @param grpColIdx 分组列索引数组
 * @return HashAggPlan 指针
 */
HashAggPlan *make_hashagg_plan(int numGroupCols, int *grpColIdx) {
    HashAggPlan *plan;

    plan = (HashAggPlan *)malloc(sizeof(HashAggPlan));
    if (plan == NULL) {
        return NULL;
    }

    memset(plan, 0, sizeof(HashAggPlan));
    plan->type = EXEC_HASHAGG;
    plan->numGroupCols = numGroupCols;
    plan->grpColIdx = grpColIdx;
    plan->aggfnames = NULL;
    plan->targetlist = NULL;

    return plan;
}
