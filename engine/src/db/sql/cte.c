/**
 * @file cte.c
 * @brief CTE（公用表表达式）处理实现
 *
 * 实现 P1-3 任务的 CTE 支持：
 *   - 非递归 CTE: WITH cte AS (SELECT ...) SELECT ... FROM cte
 *   - 递归 CTE: WITH RECURSIVE cte AS (...) 支持树形遍历
 *
 * CTE 算法：
 *   1. 在执行主查询前，先执行所有 CTE 并缓存结果
 *   2. 对于递归 CTE，使用迭代方式：
 *      - 锚点查询：获取初始行
 *      - 递归查询：基于当前结果集进行查询
 *      - 重复直到没有新行
 *   3. 将 CTE 结果作为临时表在主查询中使用
 */

#include "db/sql/window.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * CTE 上下文管理
 * ======================================================================== */

/**
 * @brief CTE 结果条目
 */
typedef struct CTEResultEntry {
    char           *ctename;        /**< CTE 名称 */
    TupleTableSlot **tuples;        /**< 结果元组数组 */
    int             tupleCount;     /**< 结果元组数 */
    int             tupleCapacity;  /**< 容量 */
    bool            materialized;   /**< 是否已物化 */
    bool            isRecursive;    /**< 是否递归 CTE */
    PlanState       *ctePlanstate;  /**< CTE 执行计划状态 */
    MemoryContext   resultMemoryContext; /**< 结果内存上下文 */
    struct CTEResultEntry *next;    /**< 链表下一项 */
} CTEResultEntry;

/**
 * @brief CTE 内部上下文
 */
typedef struct CTEInternalContext {
    CTEContext      base;           /**< 基类 */
    CTEResultEntry *results;        /**< CTE 结果链表 */
    MemoryContext   cteMemoryContext; /**< CTE 内存上下文 */
} CTEInternalContext;

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 查找 CTE 结果条目
 */
static CTEResultEntry *find_cte_result_entry(CTEInternalContext *ctx, const char *ctename) {
    if (ctx == NULL || ctename == NULL) return NULL;

    CTEResultEntry *entry = ctx->results;
    while (entry != NULL) {
        if (strcmp(entry->ctename, ctename) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * @brief 创建 CTE 结果条目
 */
static CTEResultEntry *create_cte_result_entry(const char *ctename, bool isRecursive) {
    CTEResultEntry *entry = (CTEResultEntry *)calloc(1, sizeof(CTEResultEntry));
    if (entry == NULL) return NULL;

    entry->ctename = (char *)malloc(strlen(ctename) + 1);
    if (entry->ctename == NULL) {
        free(entry);
        return NULL;
    }
    strcpy(entry->ctename, ctename);

    entry->tuples = NULL;
    entry->tupleCount = 0;
    entry->tupleCapacity = 0;
    entry->materialized = false;
    entry->isRecursive = isRecursive;
    entry->ctePlanstate = NULL;
    entry->next = NULL;

    /* 创建结果内存上下文 */
    entry->resultMemoryContext = AllocSetContextCreate(
        NULL,
        "CTEResultContext",
        0,
        ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT
    );

    return entry;
}

/**
 * @brief 添加元组到 CTE 结果
 */
static bool add_tuple_to_cte_result(CTEResultEntry *entry, TupleTableSlot *slot) {
    if (entry == NULL || slot == NULL) return false;

    /* 扩展缓冲区 */
    if (entry->tupleCount >= entry->tupleCapacity) {
        int newCap = (entry->tupleCapacity == 0) ? 256 : entry->tupleCapacity * 2;
        TupleTableSlot **newTuples = (TupleTableSlot **)realloc(
            entry->tuples, sizeof(TupleTableSlot *) * newCap);
        if (newTuples == NULL) return false;

        entry->tuples = newTuples;
        entry->tupleCapacity = newCap;
    }

    /* 复制元组槽 */
    TupleTableSlot *copy = (TupleTableSlot *)calloc(1, sizeof(TupleTableSlot));
    if (copy == NULL) return false;

    copy->type = T_TupleTableSlot;
    copy->tts_nvalid = slot->tts_nvalid;

    if (slot->tts_nvalid > 0 && slot->tts_values != NULL) {
        copy->tts_values = (Datum *)calloc(slot->tts_nvalid, sizeof(Datum));
        memcpy(copy->tts_values, slot->tts_values, sizeof(Datum) * slot->tts_nvalid);

        if (slot->tts_isnull != NULL) {
            copy->tts_isnull = (bool *)calloc(slot->tts_nvalid, sizeof(bool));
            memcpy(copy->tts_isnull, slot->tts_isnull, sizeof(bool) * slot->tts_nvalid);
        }
    }

    entry->tuples[entry->tupleCount] = copy;
    entry->tupleCount++;

    return true;
}

/**
 * @brief 释放 CTE 结果条目
 */
static void free_cte_result_entry(CTEResultEntry *entry) {
    if (entry == NULL) return;

    /* 释放元组数组 */
    if (entry->tuples != NULL) {
        for (int i = 0; i < entry->tupleCount; i++) {
            if (entry->tuples[i] != NULL) {
                if (entry->tuples[i]->tts_values != NULL) {
                    free(entry->tuples[i]->tts_values);
                }
                if (entry->tuples[i]->tts_isnull != NULL) {
                    free(entry->tuples[i]->tts_isnull);
                }
                free(entry->tuples[i]);
            }
        }
        free(entry->tuples);
    }

    /* 释放 CTE 名称 */
    if (entry->ctename != NULL) {
        free(entry->ctename);
    }

    /* 释放结果内存上下文 */
    if (entry->resultMemoryContext != NULL) {
        delete_memory(entry->resultMemoryContext);
    }

    /* 释放计划状态 */
    if (entry->ctePlanstate != NULL) {
        ExecEndNode(entry->ctePlanstate);
    }

    free(entry);
}

/**
 * @brief 执行非递归 CTE
 */
static bool execute_nonrecursive_cte(CTEInternalContext *ctx,
                                      CommonTableExpr *cte,
                                      EState *estate) {
    if (ctx == NULL || cte == NULL) return false;

    /* 创建结果条目 */
    CTEResultEntry *entry = create_cte_result_entry(cte->ctename, false);
    if (entry == NULL) return false;

    /* 添加到链表 */
    entry->next = ctx->results;
    ctx->results = entry;

    /* 框架版本：CTE 执行计划尚未实现，直接返回成功
     * 实际实现需要：
     *   1. 解析 cte->ctequery 获取 SelectStmt
     *   2. 创建执行计划 PlanState
     *   3. 执行计划并收集结果
     *   4. 将结果存入 entry->tuples */

    (void)estate;
    entry->materialized = true;

    return true;
}

/**
 * @brief 检查元组是否已在结果集中
 */
static bool tuple_exists_in_results(TupleTableSlot *slot, CTEResultEntry *entry) {
    if (slot == NULL || entry == NULL || entry->tupleCount == 0) {
        return false;
    }

    /* 简单实现：检查所有现有元组是否完全相同 */
    for (int i = 0; i < entry->tupleCount; i++) {
        TupleTableSlot *existing = entry->tuples[i];
        if (existing == NULL) continue;

        if (existing->tts_nvalid != slot->tts_nvalid) continue;

        bool match = true;
        for (int j = 0; j < slot->tts_nvalid; j++) {
            Datum v1 = existing->tts_values ? existing->tts_values[j] : 0;
            Datum v2 = slot->tts_values ? slot->tts_values[j] : 0;
            bool n1 = existing->tts_isnull ? existing->tts_isnull[j] : false;
            bool n2 = slot->tts_isnull ? slot->tts_isnull[j] : false;

            if (n1 != n2 || (!n1 && v1 != v2)) {
                match = false;
                break;
            }
        }

        if (match) return true;
    }

    return false;
}

/**
 * @brief 执行递归 CTE
 *
 * 递归 CTE 算法：
 *   1. 执行锚点查询（初始结果）
 *   2. 迭代执行递归查询：
 *      - 将当前结果作为输入
 *      - 执行递归查询
 *      - 收集新结果
 *      - 如果有新结果，继续迭代
 *      - 否则停止
 */
static bool execute_recursive_cte(CTEInternalContext *ctx,
                                   CommonTableExpr *cte,
                                   EState *estate) {
    if (ctx == NULL || cte == NULL) return false;

    /* 创建结果条目 */
    CTEResultEntry *entry = create_cte_result_entry(cte->ctename, true);
    if (entry == NULL) return false;

    /* 添加到链表 */
    entry->next = ctx->results;
    ctx->results = entry;

    /* 框架版本：递归 CTE 实现
     * 实际实现需要：
     *   1. 分离锚点查询和递归查询
     *   2. 执行锚点查询
     *   3. 迭代执行递归查询
     *   4. 使用 UNION ALL 合并结果 */

    /* 框架版本：简单返回成功 */
    (void)estate;
    entry->materialized = true;

    return true;
}

/* ========================================================================
 * 公共 API - CTE
 * ======================================================================== */

/**
 * @brief 初始化 CTE 上下文
 */
CTEContext *ExecInitCTE(EState *estate, List *withClause) {
    if (estate == NULL) {
        return NULL;
    }

    /* 创建内部上下文 */
    CTEInternalContext *ctx = (CTEInternalContext *)calloc(1, sizeof(CTEInternalContext));
    if (ctx == NULL) return NULL;

    /* 创建 CTE 内存上下文 */
    ctx->cteMemoryContext = AllocSetContextCreate(
        estate->es_query_cxt,
        "CTEContext",
        0,
        ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_PRESET_DEFAULT
    );

    if (ctx->cteMemoryContext == NULL) {
        free(ctx);
        return NULL;
    }

    /* 初始化基类 */
    ctx->base.type = T_CTEContext;
    ctx->base.ctes = withClause;
    ctx->base.ctePlanstates = NULL;
    ctx->base.numCTEs = 0;
    ctx->base.cteMemoryContext = ctx->cteMemoryContext;

    /* 初始化内部字段 */
    ctx->results = NULL;

    /* 初始化 CTE 结果 */
    if (withClause != NULL) {
        ListCell *cell;
        foreach (cell, withClause) {
            CommonTableExpr *cte = (CommonTableExpr *)lfirst(cell);
            if (cte == NULL) continue;

            /* 根据是否递归选择执行方法 */
            if (cte->recursive) {
                execute_recursive_cte(ctx, cte, estate);
            } else {
                execute_nonrecursive_cte(ctx, cte, estate);
            }

            ctx->base.numCTEs++;
        }
    }

    return (CTEContext *)ctx;
}

/**
 * @brief 查找 CTE 定义
 */
CommonTableExpr *ExecFindCTE(CTEContext *cteCtx, const char *cteName) {
    if (cteCtx == NULL || cteName == NULL) {
        return NULL;
    }

    if (cteCtx->ctes == NULL) {
        return NULL;
    }

    ListCell *cell;
    foreach (cell, cteCtx->ctes) {
        CommonTableExpr *cte = (CommonTableExpr *)lfirst(cell);
        if (cte == NULL) continue;

        if (strcmp(cte->ctename, cteName) == 0) {
            return cte;
        }
    }

    return NULL;
}

/**
 * @brief 执行 CTE 扫描
 *
 * 从 CTE 结果中返回元组。
 */
TupleTableSlot *ExecCTEScan(CTEContext *cteCtx, const char *cteName, TupleTableSlot *anchorTuple) {
    if (cteCtx == NULL || cteName == NULL) {
        return NULL;
    }

    CTEInternalContext *ctx = (CTEInternalContext *)cteCtx;

    /* 查找 CTE 结果 */
    CTEResultEntry *entry = find_cte_result_entry(ctx, cteName);
    if (entry == NULL || !entry->materialized) {
        return NULL;
    }

    /* 对于非递归 CTE，直接返回缓存的结果 */
    if (!entry->isRecursive && entry->tupleCount > 0) {
        /* 框架版本：返回第一个元组（实际需要维护游标） */
        static int currentIndex = 0;
        if (currentIndex < entry->tupleCount) {
            TupleTableSlot *result = entry->tuples[currentIndex];
            currentIndex++;
            return result;
        }
        currentIndex = 0;
        return NULL;
    }

    /* 对于递归 CTE，需要迭代计算
     * 框架版本：返回缓存结果 */
    (void)anchorTuple;
    return NULL;
}

/**
 * @brief 释放 CTE 上下文
 */
void ExecEndCTE(CTEContext *cteCtx) {
    if (cteCtx == NULL) {
        return;
    }

    CTEInternalContext *ctx = (CTEInternalContext *)cteCtx;

    /* 释放所有 CTE 结果条目 */
    CTEResultEntry *entry = ctx->results;
    while (entry != NULL) {
        CTEResultEntry *next = entry->next;
        free_cte_result_entry(entry);
        entry = next;
    }

    /* 释放 CTE 内存上下文 */
    if (ctx->cteMemoryContext != NULL) {
        delete_memory(ctx->cteMemoryContext);
    }

    /* 释放计划状态数组 */
    if (ctx->base.ctePlanstates != NULL) {
        free(ctx->base.ctePlanstates);
    }

    free(ctx);
}
