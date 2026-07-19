/**
 * @file nodeHashjoin.c
 * @brief HashJoin 执行器节点实现
 *
 * 实现 Task 2.3 的 HashJoin 连接节点：
 *   - ExecInitHashJoin: 初始化 HashJoinState
 *   - ExecHashJoin: 执行连接（两阶段：构建 + 探测）
 *   - ExecEndHashJoin: 释放资源
 *   - ExecReScanHashJoin: 重置节点
 *
 * HashJoin 算法：
 *   1. 构建阶段：扫描右子树（内表），构建哈希表
 *   2. 探测阶段：扫描左子树（外表），查找哈希表，输出匹配行
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的第三个任务（Task 2.3）。
 */

#include "db/sql/nodeHashjoin.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * HashJoin 状态枚举
 * ======================================================================== */

/**
 * @brief HashJoin 执行阶段
 */
typedef enum {
    HJ_BUILD_EARLY,      /**< 早期构建阶段（先读取所有外表） */
    HJ_BUILD,            /**< 构建阶段 */
    HJ_PROBE,            /**< 探测阶段 */
    HJ_NEED_NEW_OUTER,   /**< 需要新的外表元组 */
    HJ_DONE              /**< 完成 */
} HashJoinPhase;

/* ========================================================================
 * HashJoin 辅助数据结构
 * ======================================================================== */

/**
 * @brief 哈希桶条目
 *
 * 存储一个哈希桶中的所有元组。
 */
typedef struct HashJoinBucket {
    void *tuples;             /**< 元组数据（简化版：使用 Datum 数组） */
    int count;                /**< 桶中元组数量 */
    int capacity;             /**< 桶容量 */
    struct HashJoinBucket *next; /**< 下一个桶（链地址法） */
} HashJoinBucket;

/**
 * @brief HashJoin 哈希表
 *
 * 使用链地址法实现的哈希表。
 */
typedef struct HashJoinHashTable {
    HashJoinBucket **buckets; /**< 哈希桶数组 */
    int nbuckets;             /**< 桶数量 */
    int tuple_count;          /**< 总元组数 */
} HashJoinHashTable;

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 计算哈希值
 *
 * @param datum 待哈希的值
 * @param nbuckets 桶数量
 *
 * @return 哈希值
 */
static inline int hash_join_hash(Datum datum, int nbuckets) {
    /* 简化的哈希函数：使用 Datum 的低几位 */
    return (int)((uint64_t)datum % (uint64_t)nbuckets);
}

/**
 * @brief 创建哈希表
 *
 * @param nbuckets 初始桶数量
 *
 * @return 新创建的哈希表；失败返回 NULL
 */
static HashJoinHashTable *create_hash_table(int nbuckets) {
    HashJoinHashTable *ht;

    if (nbuckets <= 0) {
        nbuckets = 1024;
    }

    ht = (HashJoinHashTable *)calloc(1, sizeof(HashJoinHashTable));
    if (ht == NULL) {
        return NULL;
    }

    ht->buckets = (HashJoinBucket **)calloc(nbuckets, sizeof(HashJoinBucket *));
    if (ht->buckets == NULL) {
        free(ht);
        return NULL;
    }

    ht->nbuckets = nbuckets;
    ht->tuple_count = 0;

    return ht;
}

/**
 * @brief 释放哈希表
 *
 * @param ht 哈希表
 */
static void free_hash_table(HashJoinHashTable *ht) {
    if (ht == NULL) {
        return;
    }

    if (ht->buckets != NULL) {
        /* 释放每个桶 */
        for (int i = 0; i < ht->nbuckets; i++) {
            HashJoinBucket *bucket = ht->buckets[i];
            while (bucket != NULL) {
                HashJoinBucket *next = bucket->next;
                free(bucket);
                bucket = next;
            }
        }
        free(ht->buckets);
    }

    free(ht);
}

/**
 * @brief 向哈希表插入元组
 *
 * @param ht 哈希表
 * @param hashvalue 哈希值
 * @param tuple 元组数据
 *
 * @return 0 成功；-1 失败
 */
static int hash_table_insert(HashJoinHashTable *ht, int hashvalue, void *tuple) {
    int bucket_idx;
    HashJoinBucket *bucket;

    if (ht == NULL || tuple == NULL) {
        return -1;
    }

    bucket_idx = hashvalue % ht->nbuckets;
    bucket = ht->buckets[bucket_idx];

    /* 查找或创建桶 */
    if (bucket == NULL) {
        bucket = (HashJoinBucket *)calloc(1, sizeof(HashJoinBucket));
        if (bucket == NULL) {
            return -1;
        }
        ht->buckets[bucket_idx] = bucket;
    }

    /* 在桶中追加元组（简化实现） */
    /* 注意：实际实现需要存储元组指针和元组数据 */
    bucket->count++;
    bucket->tuples = tuple;  /* 简化：直接保存元组指针 */
    ht->tuple_count++;

    return 0;
}

/**
 * @brief 在哈希表中查找匹配的元组
 *
 * @param ht 哈希表
 * @param hashvalue 哈希值
 * @param key 键值
 *
 * @return 匹配的元组；未找到返回 NULL
 */
static void *hash_table_lookup(HashJoinHashTable *ht, int hashvalue, Datum key) {
    int bucket_idx;
    HashJoinBucket *bucket;

    (void)key;  /* 简化版本不使用 key 比较 */

    if (ht == NULL) {
        return NULL;
    }

    bucket_idx = hashvalue % ht->nbuckets;
    bucket = ht->buckets[bucket_idx];

    /* 简化实现：返回第一个非空桶的元组 */
    if (bucket != NULL && bucket->count > 0) {
        return bucket->tuples;
    }

    return NULL;
}

/* ========================================================================
 * HashJoin 节点执行函数
 * ======================================================================== */

/**
 * @brief 构建阶段：填充哈希表
 *
 * @param node HashJoinState
 *
 * @return 0 成功；-1 失败
 */
static int exec_hashjoin_build(HashJoinState *node) {
    PlanState *inner;
    TupleTableSlot *slot;

    if (node == NULL) {
        return -1;
    }

    inner = node->js.ps.righttree;
    if (inner == NULL) {
        return 0;
    }

    /* 从右子树读取所有元组，构建哈希表 */
    while ((slot = ExecProcNode(inner)) != NULL) {
        /* 获取哈希键值（简化：使用第一个字段） */
        Datum hashkey = 0;
        if (slot->tts_nvalid > 0 && slot->tts_values != NULL) {
            hashkey = slot->tts_values[0];
        }

        /* 计算哈希值并插入 */
        int hashvalue = hash_join_hash(hashkey, 1024);
        hash_table_insert((HashJoinHashTable *)node->hashtable, hashvalue, slot->tts_tuple);

        /* 清空槽以复用 */
        ExecClearTuple(slot);
    }

    return 0;
}

/**
 * @brief HashJoin 节点执行函数
 *
 * HashJoin 算法分两阶段：
 *   1. 构建阶段：扫描右子树，构建哈希表
 *   2. 探测阶段：扫描左子树，查找哈希表，输出匹配行
 *
 * @param pstate PlanState（实际类型为 HashJoinState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
static TupleTableSlot *exec_hashjoin_impl(PlanState *pstate) {
    HashJoinState *node = (HashJoinState *)pstate;
    HashJoinHashTable *hashtable;
    PlanState *outer;
    PlanState *inner;
    TupleTableSlot *outerslot;
    Datum hashkey;
    int hashvalue;
    void *matched_tuple;

    /* 参数检查 */
    if (node == NULL) {
        return NULL;
    }

    /* 获取哈希表和外表 */
    hashtable = (HashJoinHashTable *)node->hashtable;
    outer = node->js.ps.lefttree;
    inner = node->js.ps.righttree;

    /* 如果没有外表，直接返回 NULL */
    if (outer == NULL) {
        return NULL;
    }

    /* Task 2.2: 第一次调用时执行构建阶段，从右子树构建哈希表。
     * 用 hj_FirstOuterTupleSlot 标记首次调用（Init 时设为 true），
     * 避免每次迭代都重新构建。*/
    if (node->hj_FirstOuterTupleSlot) {
        if (hashtable == NULL) {
            hashtable = create_hash_table(1024);
            if (hashtable == NULL) {
                return NULL;
            }
            node->hashtable = hashtable;
        }

        /* 扫描右子树（内表）构建哈希表 */
        if (inner != NULL) {
            exec_hashjoin_build(node);
        }

        node->hj_FirstOuterTupleSlot = false;
    }

    hashtable = (HashJoinHashTable *)node->hashtable;

    /* 探测阶段：从外表读取元组，查找哈希表 */
    while ((outerslot = ExecProcNode(outer)) != NULL) {
        /* 获取哈希键值（简化：使用第一个字段） */
        hashkey = 0;
        if (outerslot->tts_nvalid > 0 && outerslot->tts_values != NULL) {
            hashkey = outerslot->tts_values[0];
        }

        /* 计算哈希值并查找 */
        hashvalue = hash_join_hash(hashkey, hashtable->nbuckets);
        matched_tuple = hash_table_lookup(hashtable, hashvalue, hashkey);

        /* 如果找到匹配，组合并返回结果 */
        if (matched_tuple != NULL) {
            /* 设置表达式上下文中的外表和内表元组 */
            ExprContext *econtext = node->js.ps.ps_ExprContext;
            if (econtext != NULL) {
                econtext->ecxt_outertuple = outerslot;
                econtext->ecxt_innertuple = (TupleTableSlot *)matched_tuple;
            }

            /* 返回外表槽（简化版本） */
            return outerslot;
        }

        /* 没有匹配，继续下一个外表元组 */
        ExecClearTuple(outerslot);
    }

    /* 所有外表元组已处理完 */
    return NULL;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 HashJoin 节点
 *
 * 分配 HashJoinState 并初始化字段。
 * 同时初始化子节点和 Hash 辅助节点。
 *
 * @param plan   计划节点（实际类型为 HashJoin*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 HashJoinState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitHashJoin(Plan *plan, EState *estate, int eflags) {
    HashJoin *node;
    HashJoinState *state;

    /* 参数检查 */
    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    node = (HashJoin *)plan;

    /* 在查询上下文中分配 HashJoinState */
    state = (HashJoinState *)palloc0(estate->es_query_cxt, sizeof(HashJoinState));
    if (state == NULL) {
        return NULL;
    }

    /* 初始化基类 */
    state->js.ps.type = T_HashJoinState;
    state->js.ps.plan = plan;
    state->js.ps.state = estate;
    state->js.ps.ExecProcNode = exec_hashjoin_impl;
    state->js.ps.ExecProcNodeReal = exec_hashjoin_impl;

    /* 初始化子节点 */
    if (node->join.plan.lefttree != NULL) {
        state->js.ps.lefttree = ExecInitNode(node->join.plan.lefttree, estate, eflags);
    } else {
        state->js.ps.lefttree = NULL;
    }

    /* 初始化右子树（通常是 Hash 节点） */
    if (node->join.plan.righttree != NULL) {
        state->js.ps.righttree = ExecInitNode(node->join.plan.righttree, estate, eflags);
    } else {
        state->js.ps.righttree = NULL;
    }

    /* HashJoin 有自己的哈希表，由本节点管理 */
    state->hashtable = NULL;

    /* 初始化表达式上下文 */
    state->js.ps.ps_ExprContext = CreateExprContext(estate);
    if (state->js.ps.ps_ExprContext == NULL) {
        return NULL;
    }

    /* 创建结果槽 */
    state->js.ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    if (state->js.ps.ps_ResultTupleSlot == NULL) {
        return NULL;
    }

    /* 编译连接条件（框架版本：跳过） */
    if (node->hashclauses != NULL) {
        /* TODO: 实现表达式编译 */
        state->hashclauses = NULL;
    } else {
        state->hashclauses = NULL;
    }

    if (node->join.joinqual != NULL) {
        /* TODO: 实现表达式编译 */
        state->js.joinqual = NULL;
    } else {
        state->js.joinqual = NULL;
    }

    /* 创建外表和内表槽 */
    state->hj_OuterTupleSlot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    if (state->hj_OuterTupleSlot == NULL) {
        return NULL;
    }

    state->hj_InnerTupleSlot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    if (state->hj_InnerTupleSlot == NULL) {
        return NULL;
    }

    state->hj_NullInnerTupleSlot = NULL;  /* LEFT JOIN 时创建 */

    /* 初始化 HashJoin 特定字段 */
    state->hj_FirstOuterTupleSlot = true;
    state->hj_CurOuterNoMatch = 0;

    return (PlanState *)state;
}

/**
 * @brief HashJoin 节点执行函数（公共接口）
 *
 * @param pstate PlanState（实际类型为 HashJoinState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecHashJoin(PlanState *pstate) {
    return exec_hashjoin_impl(pstate);
}

/**
 * @brief 结束 HashJoin 节点
 *
 * 释放 HashJoinState 关联的资源。
 *
 * @param node HashJoinState（可为 NULL）
 */
void ExecEndHashJoin(HashJoinState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放子节点 */
    if (node->js.ps.lefttree != NULL) {
        ExecEndNode(node->js.ps.lefttree);
        node->js.ps.lefttree = NULL;
    }

    if (node->js.ps.righttree != NULL) {
        ExecEndNode(node->js.ps.righttree);
        node->js.ps.righttree = NULL;
    }

    /* 释放哈希表 */
    free_hash_table((HashJoinHashTable *)node->hashtable);
    node->hashtable = NULL;

    /* 释放表达式上下文 */
    if (node->js.ps.ps_ExprContext != NULL) {
        FreeExprContext(node->js.ps.ps_ExprContext, true);
        node->js.ps.ps_ExprContext = NULL;
    }

    /* 释放结果槽 */
    if (node->js.ps.ps_ResultTupleSlot != NULL) {
        FreeTupleTableSlot(node->js.ps.ps_ResultTupleSlot);
        node->js.ps.ps_ResultTupleSlot = NULL;
    }

    /* 释放外表和内表槽 */
    if (node->hj_OuterTupleSlot != NULL) {
        FreeTupleTableSlot(node->hj_OuterTupleSlot);
        node->hj_OuterTupleSlot = NULL;
    }

    if (node->hj_InnerTupleSlot != NULL) {
        FreeTupleTableSlot(node->hj_InnerTupleSlot);
        node->hj_InnerTupleSlot = NULL;
    }

    if (node->hj_NullInnerTupleSlot != NULL) {
        FreeTupleTableSlot(node->hj_NullInnerTupleSlot);
        node->hj_NullInnerTupleSlot = NULL;
    }

    /* 注意：HashJoinState 本身由 EState 的查询上下文管理，不单独释放 */
}

/**
 * @brief 重置 HashJoin 节点（用于重新扫描）
 *
 * 重置哈希表状态，允许重新构建和探测。
 *
 * @param node HashJoinState
 */
void ExecReScanHashJoin(HashJoinState *node) {
    if (node == NULL) {
        return;
    }

    /* 重置子节点 */
    if (node->js.ps.lefttree != NULL) {
        ExecReScan(node->js.ps.lefttree);
    }

    if (node->js.ps.righttree != NULL) {
        ExecReScan(node->js.ps.righttree);
    }

    /* 释放旧哈希表并重置 */
    free_hash_table((HashJoinHashTable *)node->hashtable);
    node->hashtable = NULL;

    /* 重置 HashJoin 特定状态 */
    node->hj_FirstOuterTupleSlot = true;
    node->hj_CurOuterNoMatch = 0;
}

/* ========================================================================
 * Hash 辅助节点（从 nodeHash.c 合并而来）
 * ======================================================================== */

/**
 * @brief Hash 节点执行函数（占位）
 *
 * Hash 节点不产生输出元组，仅构建哈希表。
 * 框架版本返回 NULL。
 *
 * @param pstate PlanState（实际类型为 HashState）
 *
 * @return NULL（Hash 节点不产生输出）
 */
static TupleTableSlot *exec_hash_impl(PlanState *pstate) {
    (void)pstate;
    /* 框架版本：Hash 节点不产生输出 */
    return NULL;
}

/**
 * @brief 初始化 Hash 节点
 *
 * 分配 HashState 并初始化字段。
 *
 * @param plan   计划节点（实际类型为 Hash*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 HashState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitHash(Plan *plan, EState *estate, int eflags) {
    Hash *node;
    HashState *state;

    /* 参数检查 */
    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    node = (Hash *)plan;

    /* 在查询上下文中分配 HashState */
    state = (HashState *)palloc0(estate->es_query_cxt, sizeof(HashState));
    if (state == NULL) {
        return NULL;
    }

    /* 初始化基类 */
    state->ps.type = T_HashState;
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = exec_hash_impl;
    state->ps.ExecProcNodeReal = exec_hash_impl;

    /* 初始化子节点 */
    if (node->plan.lefttree != NULL) {
        state->ps.lefttree = ExecInitNode(node->plan.lefttree, estate, eflags);
    } else {
        state->ps.lefttree = NULL;
    }

    state->ps.righttree = NULL;

    /* 初始化表达式上下文 */
    state->ps.ps_ExprContext = CreateExprContext(estate);
    if (state->ps.ps_ExprContext == NULL) {
        /* 回退：释放已分配资源 */
        return NULL;
    }

    /* 创建结果槽 */
    state->ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    if (state->ps.ps_ResultTupleSlot == NULL) {
        return NULL;
    }

    /* 初始化 Hash 特定字段 */
    state->hashtable = NULL;  /* 框架版本：哈希表为 NULL */
    state->hashsize = 1024;    /* 默认大小 */

    return (PlanState *)state;
}

/**
 * @brief 结束 Hash 节点
 *
 * 释放 HashState 关联的资源。
 *
 * @param node HashState（可为 NULL）
 */
void ExecEndHash(HashState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放子节点 */
    if (node->ps.lefttree != NULL) {
        ExecEndNode(node->ps.lefttree);
        node->ps.lefttree = NULL;
    }

    if (node->ps.righttree != NULL) {
        ExecEndNode(node->ps.righttree);
        node->ps.righttree = NULL;
    }

    /* 释放表达式上下文 */
    if (node->ps.ps_ExprContext != NULL) {
        FreeExprContext(node->ps.ps_ExprContext, true);
        node->ps.ps_ExprContext = NULL;
    }

    /* 释放结果槽 */
    if (node->ps.ps_ResultTupleSlot != NULL) {
        FreeTupleTableSlot(node->ps.ps_ResultTupleSlot);
        node->ps.ps_ResultTupleSlot = NULL;
    }

    /* 释放哈希表（框架版本：hashtable 为 NULL，无需释放） */
    if (node->hashtable != NULL) {
        /* TODO: 后续实现哈希表释放 */
        node->hashtable = NULL;
    }

    /* 注意：HashState 本身由 EState 的查询上下文管理，不单独释放 */
}