/**
 * @file nodeWindow.c
 * @brief 窗口算子实现
 *
 * 实现 P1-3 任务的窗口函数执行器：
 *   - ExecInitWindowAgg: 初始化窗口聚合节点
 *   - ExecWindowAgg: 执行窗口函数计算
 *   - ExecEndWindowAgg: 释放资源
 *   - ExecReScanWindowAgg: 重置节点
 *
 * 窗口函数算法：
 *   1. 按分区列分组
 *   2. 在分区内按排序列排序（如果需要）
 *   3. 对每行计算窗口函数（考虑帧边界）
 *
 * 支持的窗口函数：
 *   - 排名函数: ROW_NUMBER, RANK, DENSE_RANK, NTILE
 *   - 偏移函数: LAG, LEAD, FIRST_VALUE, LAST_VALUE
 *   - 聚合函数: SUM, AVG, COUNT, MIN, MAX
 */

#include "db/sql/window.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define WINDOW_BUFFER_INITIAL_SIZE 1024  /**< 初始缓冲区大小 */
#define WINDOW_BUFFER_GROW_FACTOR 2      /**< 缓冲区增长因子 */

/* ========================================================================
 * 窗口函数执行状态管理
 * ======================================================================== */

/**
 * @brief 创建窗口函数状态
 */
static WindowFuncState *create_window_func_state(WindowFuncType wintype,
                                                  int argIndex, int argIndex2,
                                                  Datum defaultValue, bool defaultIsNull) {
    WindowFuncState *state = (WindowFuncState *)calloc(1, sizeof(WindowFuncState));
    if (state == NULL) return NULL;

    state->wintype = wintype;
    state->argIndex = argIndex;
    state->argIndex2 = argIndex2;
    state->defaultValue = defaultValue;
    state->defaultIsNull = defaultIsNull;

    /* 初始化聚合状态 */
    state->aggInitialized = false;
    state->aggCount = 0;
    state->aggSum = 0.0;
    state->aggMin = 0.0;
    state->aggMax = 0.0;

    /* 初始化排名状态 */
    state->rankValue = 0;
    state->rankOffset = 0;

    /* 初始化 NTILE 状态 */
    state->ntileBuckets = 0;
    state->ntileCount = 0;

    /* 初始化 LAG/LEAD 缓冲区 */
    state->lagOffset = 1;
    state->lagBuffer = NULL;
    state->lagBufferNull = NULL;
    state->lagBufferSize = 0;
    state->lagBufferPos = 0;

    return state;
}

/**
 * @brief 重置窗口函数状态
 */
static void reset_window_func_state(WindowFuncState *state) {
    if (state == NULL) return;

    state->rankValue = 0;
    state->rankOffset = 0;
    state->aggInitialized = false;
    state->aggCount = 0;
    state->aggSum = 0.0;
    state->ntileCount = 0;

    /* 清除 LAG/LEAD 缓冲区 */
    if (state->lagBuffer != NULL) {
        free(state->lagBuffer);
        state->lagBuffer = NULL;
    }
    if (state->lagBufferNull != NULL) {
        free(state->lagBufferNull);
        state->lagBufferNull = NULL;
    }
    state->lagBufferSize = 0;
    state->lagBufferPos = 0;
}

/**
 * @brief 释放窗口函数状态
 */
static void free_window_func_state(WindowFuncState *state) {
    if (state == NULL) return;

    if (state->lagBuffer != NULL) {
        free(state->lagBuffer);
    }
    if (state->lagBufferNull != NULL) {
        free(state->lagBufferNull);
    }
    free(state);
}

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 比较两个 Datum 值（整数）
 */
static int compare_datum_int(Datum a, Datum b, bool *aIsNull, bool *bIsNull) {
    if (*aIsNull && *bIsNull) return 0;
    if (*aIsNull) return -1;
    if (*bIsNull) return 1;

    int64_t ia = (int64_t)a;
    int64_t ib = (int64_t)b;

    if (ia < ib) return -1;
    if (ia > ib) return 1;
    return 0;
}

/**
 * @brief 比较两个 Datum 值（浮点数）
 */
static int compare_datum_float(Datum a, Datum b, bool *aIsNull, bool *bIsNull) {
    if (*aIsNull && *bIsNull) return 0;
    if (*aIsNull) return -1;
    if (*bIsNull) return 1;

    double fa = *(double *)&a;
    double fb = *(double *)&b;

    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

/**
 * @brief 检查两个元组在分区列上是否相等
 */
bool window_tuples_match_partition(TupleTableSlot *slot1, TupleTableSlot *slot2,
                                   int numCols, int *colIdx) {
    if (slot1 == NULL || slot2 == NULL) return false;
    if (numCols == 0) return true;  /* 无分区列，所有行都在同一分区 */

    Datum *vals1 = slot1->tts_values;
    Datum *vals2 = slot2->tts_values;
    bool *nulls1 = slot1->tts_isnull;
    bool *nulls2 = slot2->tts_isnull;

    for (int i = 0; i < numCols; i++) {
        int col = colIdx[i];
        bool n1 = (nulls1 != NULL) ? nulls1[col] : false;
        bool n2 = (nulls2 != NULL) ? nulls2[col] : false;

        if (n1 != n2) return false;
        if (!n1 && vals1[col] != vals2[col]) return false;
    }

    return true;
}

/**
 * @brief 计算帧边界
 */
void window_compute_frame_bounds(WindowAggState *state, int currentPos,
                                 int totalRows, WindowFrame *frame,
                                 int *head, int *tail) {
    if (state == NULL || frame == NULL || head == NULL || tail == NULL) {
        *head = 0;
        *tail = totalRows - 1;
        return;
    }

    int h = 0;
    int t = totalRows - 1;

    /* 计算起始边界 */
    switch (frame->startBound) {
        case FRAME_BOUND_UNBOUNDED_PRECEDING:
            h = 0;
            break;
        case FRAME_BOUND_PRECEDING:
            h = currentPos - frame->startOffset;
            if (h < 0) h = 0;
            break;
        case FRAME_BOUND_CURRENT_ROW:
            h = currentPos;
            break;
        case FRAME_BOUND_FOLLOWING:
            h = currentPos + frame->startOffset;
            if (h >= totalRows) h = totalRows - 1;
            break;
        case FRAME_BOUND_UNBOUNDED_FOLLOWING:
            h = totalRows - 1;
            break;
    }

    /* 计算结束边界 */
    switch (frame->endBound) {
        case FRAME_BOUND_UNBOUNDED_PRECEDING:
            t = 0;
            break;
        case FRAME_BOUND_PRECEDING:
            t = currentPos - frame->endOffset;
            if (t < 0) t = 0;
            break;
        case FRAME_BOUND_CURRENT_ROW:
            t = currentPos;
            break;
        case FRAME_BOUND_FOLLOWING:
            t = currentPos + frame->endOffset;
            if (t >= totalRows) t = totalRows - 1;
            break;
        case FRAME_BOUND_UNBOUNDED_FOLLOWING:
            t = totalRows - 1;
            break;
    }

    *head = h;
    *tail = t;
}

/**
 * @brief 评估排名窗口函数
 */
static Datum eval_rank_function(WindowFuncState *funcState, int rankPos, int equalCount) {
    switch (funcState->wintype) {
        case WINDOWFUNC_ROW_NUMBER:
            return (Datum)(uint64_t)(rankPos + 1);

        case WINDOWFUNC_RANK:
            return (Datum)(uint64_t)(funcState->rankValue);

        case WINDOWFUNC_DENSE_RANK:
            return (Datum)(uint64_t)(funcState->rankOffset + 1);

        case WINDOWFUNC_PERCENT_RANK: {
            if (funcState->rankValue <= 1) return (Datum)0;
            /* PERCENT_RANK = (rank - 1) / (total_rows - 1) */
            int64_t total = rankPos + equalCount;
            if (total <= 1) return (Datum)0;
            double pct = (double)(funcState->rankValue - 1) / (double)(total - 1);
            return *(Datum *)&pct;
        }

        case WINDOWFUNC_CUME_DIST: {
            /* CUME_DIST = number of rows <= current row / total rows */
            int64_t total = rankPos + equalCount;
            if (total <= 0) return (Datum)0;
            double cdist = (double)(funcState->rankValue + equalCount - 1) / (double)total;
            return *(Datum *)&cdist;
        }

        default:
            return (Datum)0;
    }
}

/**
 * @brief 评估偏移窗口函数
 */
static Datum eval_offset_function(WindowFuncState *funcState,
                                   TupleTableSlot **buffer, int bufferSize,
                                   int currentPos, bool *isNull) {
    int offset = funcState->lagOffset;
    int targetPos = currentPos - offset;

    if (targetPos < 0 || targetPos >= bufferSize || buffer[targetPos] == NULL) {
        *isNull = funcState->defaultIsNull;
        return funcState->defaultValue;
    }

    TupleTableSlot *targetSlot = buffer[targetPos];
    int argIdx = funcState->argIndex;

    if (targetSlot->tts_values == NULL || argIdx >= targetSlot->tts_nvalid) {
        *isNull = true;
        return (Datum)0;
    }

    *isNull = (targetSlot->tts_isnull != NULL) ? targetSlot->tts_isnull[argIdx] : false;
    return targetSlot->tts_values[argIdx];
}

/**
 * @brief 评估 FIRST_VALUE / LAST_VALUE
 */
static Datum eval_first_last_function(WindowFuncState *funcState,
                                       TupleTableSlot **buffer, int bufferSize,
                                       int head, int tail, bool *isNull) {
    if (head < 0 || head >= bufferSize || tail < 0 || tail >= bufferSize) {
        *isNull = true;
        return (Datum)0;
    }

    int argIdx = funcState->argIndex;
    Datum result = (Datum)0;
    bool found = false;

    if (funcState->wintype == WINDOWFUNC_FIRST_VALUE) {
        /* 遍历帧，找最小值 */
        for (int i = head; i <= tail; i++) {
            TupleTableSlot *slot = buffer[i];
            if (slot == NULL || slot->tts_values == NULL) continue;
            if (argIdx >= slot->tts_nvalid) continue;

            bool isnull = (slot->tts_isnull != NULL) ? slot->tts_isnull[argIdx] : false;
            if (isnull) continue;

            Datum val = slot->tts_values[argIdx];
            if (!found || compare_datum_int(val, result, &isnull, isNull) < 0) {
                result = val;
                *isNull = false;
                found = true;
            }
        }
    } else {
        /* LAST_VALUE: 遍历帧，找最大值 */
        for (int i = head; i <= tail; i++) {
            TupleTableSlot *slot = buffer[i];
            if (slot == NULL || slot->tts_values == NULL) continue;
            if (argIdx >= slot->tts_nvalid) continue;

            bool isnull = (slot->tts_isnull != NULL) ? slot->tts_isnull[argIdx] : false;
            if (isnull) continue;

            Datum val = slot->tts_values[argIdx];
            if (!found || compare_datum_int(val, result, &isnull, isNull) > 0) {
                result = val;
                *isNull = false;
                found = true;
            }
        }
    }

    if (!found) {
        *isNull = true;
        return funcState->defaultValue;
    }

    return result;
}

/**
 * @brief 评估聚合窗口函数
 */
static Datum eval_aggregate_window_function(WindowFuncState *funcState,
                                             TupleTableSlot **buffer, int bufferSize,
                                             int head, int tail, int argIdx, bool *isNull) {
    switch (funcState->wintype) {
        case WINDOWFUNC_COUNT: {
            int64_t count = 0;
            for (int i = head; i <= tail; i++) {
                TupleTableSlot *slot = buffer[i];
                if (slot == NULL) continue;
                if (slot->tts_values != NULL && slot->tts_nvalid > argIdx) {
                    bool nullFlag = (slot->tts_isnull != NULL) ? slot->tts_isnull[argIdx] : false;
                    if (!nullFlag) count++;
                }
            }
            *isNull = false;
            return (Datum)(uint64_t)count;
        }

        case WINDOWFUNC_SUM: {
            int64_t sum = 0;
            bool hasValue = false;
            for (int i = head; i <= tail; i++) {
                TupleTableSlot *slot = buffer[i];
                if (slot == NULL) continue;
                if (slot->tts_values != NULL && slot->tts_nvalid > argIdx) {
                    bool nullFlag = (slot->tts_isnull != NULL) ? slot->tts_isnull[argIdx] : false;
                    if (!nullFlag) {
                        sum += (int64_t)slot->tts_values[argIdx];
                        hasValue = true;
                    }
                }
            }
            *isNull = !hasValue;
            return (Datum)(uint64_t)sum;
        }

        case WINDOWFUNC_AVG: {
            int64_t sum = 0;
            int64_t count = 0;
            for (int i = head; i <= tail; i++) {
                TupleTableSlot *slot = buffer[i];
                if (slot == NULL) continue;
                if (slot->tts_values != NULL && slot->tts_nvalid > argIdx) {
                    bool nullFlag = (slot->tts_isnull != NULL) ? slot->tts_isnull[argIdx] : false;
                    if (!nullFlag) {
                        sum += (int64_t)slot->tts_values[argIdx];
                        count++;
                    }
                }
            }
            if (count == 0) {
                *isNull = true;
                return (Datum)0;
            }
            *isNull = false;
            double avg = (double)sum / (double)count;
            return *(Datum *)&avg;
        }

        case WINDOWFUNC_MIN: {
            bool found = false;
            int64_t minVal = 0;
            for (int i = head; i <= tail; i++) {
                TupleTableSlot *slot = buffer[i];
                if (slot == NULL) continue;
                if (slot->tts_values != NULL && slot->tts_nvalid > argIdx) {
                    bool nullFlag = (slot->tts_isnull != NULL) ? slot->tts_isnull[argIdx] : false;
                    if (!nullFlag) {
                        int64_t val = (int64_t)slot->tts_values[argIdx];
                        if (!found || val < minVal) {
                            minVal = val;
                            found = true;
                        }
                    }
                }
            }
            *isNull = !found;
            return (Datum)(uint64_t)minVal;
        }

        case WINDOWFUNC_MAX: {
            bool found = false;
            int64_t maxVal = 0;
            for (int i = head; i <= tail; i++) {
                TupleTableSlot *slot = buffer[i];
                if (slot == NULL) continue;
                if (slot->tts_values != NULL && slot->tts_nvalid > argIdx) {
                    bool nullFlag = (slot->tts_isnull != NULL) ? slot->tts_isnull[argIdx] : false;
                    if (!nullFlag) {
                        int64_t val = (int64_t)slot->tts_values[argIdx];
                        if (!found || val > maxVal) {
                            maxVal = val;
                            found = true;
                        }
                    }
                }
            }
            *isNull = !found;
            return (Datum)(uint64_t)maxVal;
        }

        default:
            *isNull = true;
            return (Datum)0;
    }
}

/**
 * @brief 评估窗口函数（主函数）
 */
Datum window_func_eval(WindowFuncState *funcState, Datum *args, bool *argsNull, int numArgs) {
    (void)args;
    (void)numArgs;
    /* 当前实现返回 0，窗口函数通过 buffer 在 exec_window_impl 中计算 */
    return (Datum)0;
}

/* ========================================================================
 * 窗口聚合节点执行函数
 * ======================================================================== */

/**
 * @brief 扩展分区缓冲区
 */
static int expand_partition_buffer(WindowAggState *state, int requiredSize) {
    if (state == NULL) return -1;

    int newSize = state->partitionBufferSize;
    if (newSize == 0) {
        newSize = WINDOW_BUFFER_INITIAL_SIZE;
    }

    while (newSize < requiredSize) {
        newSize *= WINDOW_BUFFER_GROW_FACTOR;
    }

    TupleTableSlot **newBuffer = (TupleTableSlot **)realloc(
        state->partitionBuffers, sizeof(TupleTableSlot *) * newSize);
    if (newBuffer == NULL) return -1;

    /* 清零新增空间 */
    for (int i = state->partitionBufferSize; i < newSize; i++) {
        newBuffer[i] = NULL;
    }

    state->partitionBuffers = newBuffer;
    state->partitionBufferSize = newSize;

    return newSize;
}

/**
 * @brief 释放分区缓冲区
 */
static void free_partition_buffer(WindowAggState *state) {
    if (state == NULL) return;

    if (state->partitionBuffers != NULL) {
        /* 缓冲区中的 slot 由外部管理，此处只释放指针数组 */
        free(state->partitionBuffers);
        state->partitionBuffers = NULL;
    }
    state->partitionBufferSize = 0;
    state->partitionBufferUsed = 0;
}

/**
 * @brief WindowAgg 节点执行函数（内部实现）
 */
static TupleTableSlot *exec_window_impl(PlanState *pstate) {
    WindowAggState *node = (WindowAggState *)pstate;

    if (node == NULL || node->allDone) {
        return NULL;
    }

    /* 检查是否有更多分区 */
    if (node->morePartitions) {
        /* 开始收集下一个分区 */
        node->partitionBufferUsed = 0;
        node->currentPos = 0;
        node->morePartitions = false;
    }

    /* 检查缓冲区是否需要扩展 */
    if (node->partitionBufferUsed >= node->partitionBufferSize) {
        if (expand_partition_buffer(node, node->partitionBufferUsed + 1) < 0) {
            return NULL;
        }
    }

    /* 从子节点获取下一个元组 */
    TupleTableSlot *slot = ExecProcNode(node->ps.lefttree);
    if (slot == NULL) {
        /* 当前分区结束 */
        node->allDone = true;
        return NULL;
    }

    /* 检查是否属于当前分区 */
    if (node->partitionBufferUsed > 0) {
        TupleTableSlot *firstSlot = node->partitionBuffers[0];
        if (!window_tuples_match_partition(slot, firstSlot,
                                           node->numCols, node->partColIdx)) {
            /* 新分区开始 */
            node->morePartitions = true;

            /* 重置窗口函数状态 */
            for (int i = 0; i < node->numFuncs; i++) {
                reset_window_func_state(&node->funcStates[i]);
            }

            /* 返回最后一个分区元组 */
            node->currentPos = node->partitionBufferUsed - 1;
            return node->partitionBuffers[node->currentPos];
        }
    }

    /* 复制元组槽到缓冲区 */
    TupleTableSlot *slotCopy = (TupleTableSlot *)calloc(1, sizeof(TupleTableSlot));
    if (slotCopy == NULL) return NULL;

    slotCopy->type = T_TupleTableSlot;
    slotCopy->tts_nvalid = slot->tts_nvalid;

    if (slot->tts_nvalid > 0 && slot->tts_values != NULL) {
        slotCopy->tts_values = (Datum *)calloc(slot->tts_nvalid, sizeof(Datum));
        memcpy(slotCopy->tts_values, slot->tts_values, sizeof(Datum) * slot->tts_nvalid);

        if (slot->tts_isnull != NULL) {
            slotCopy->tts_isnull = (bool *)calloc(slot->tts_nvalid, sizeof(bool));
            memcpy(slotCopy->tts_isnull, slot->tts_isnull, sizeof(bool) * slot->tts_nvalid);
        }
    }

    node->partitionBuffers[node->partitionBufferUsed] = slotCopy;
    node->partitionBufferUsed++;
    node->currentPos = node->partitionBufferUsed - 1;

    return slotCopy;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 WindowAgg 节点
 */
PlanState *ExecInitWindowAgg(Plan *plan, EState *estate, int eflags) {
    WindowAgg *node;
    WindowAggState *state;
    MemoryContext ctx;

    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    node = (WindowAgg *)plan;
    ctx = estate->es_query_cxt;

    /* 分配 WindowAggState */
    state = (WindowAggState *)palloc0(ctx, sizeof(WindowAggState));
    if (state == NULL) {
        return NULL;
    }

    /* 初始化基类 */
    state->ps.type = T_WindowAggState;
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = exec_window_impl;
    state->ps.ExecProcNodeReal = exec_window_impl;

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
        return NULL;
    }

    /* 创建结果槽 */
    state->ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(ctx);
    if (state->ps.ps_ResultTupleSlot == NULL) {
        return NULL;
    }

    /* 初始化 WindowAgg 特定字段 */
    state->windowtuple = MakeTupleTableSlotWithMCxt(ctx);
    state->firstPartitionSlot = NULL;
    state->allDone = false;
    state->morePartitions = false;

    /* 初始化分区信息 */
    state->numCols = node->numCols;
    state->partColIdx = NULL;
    if (node->numCols > 0 && node->planColIdx != NULL) {
        state->partColIdx = (int *)palloc(ctx, sizeof(int) * node->numCols);
        memcpy(state->partColIdx, node->planColIdx, sizeof(int) * node->numCols);
    }

    /* 初始化帧信息 */
    if (node->frame != NULL) {
        memcpy(&state->frame, node->frame, sizeof(WindowFrame));
    } else {
        /* 默认帧: RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW */
        state->frame.frameMode = FRAME_MODE_RANGE;
        state->frame.startBound = FRAME_BOUND_UNBOUNDED_PRECEDING;
        state->frame.startOffset = 0;
        state->frame.endBound = FRAME_BOUND_CURRENT_ROW;
        state->frame.endOffset = 0;
    }

    /* 初始化元组缓冲区 */
    state->partitionBuffers = (TupleTableSlot **)calloc(
        WINDOW_BUFFER_INITIAL_SIZE, sizeof(TupleTableSlot *));
    state->partitionBufferSize = WINDOW_BUFFER_INITIAL_SIZE;
    state->partitionBufferUsed = 0;
    state->currentPos = 0;

    /* 初始化窗口函数状态数组 */
    state->numFuncs = 3;  /* 框架版本：预分配 3 个 */
    state->funcStates = (WindowFuncState *)calloc(
        state->numFuncs, sizeof(WindowFuncState));
    if (state->funcStates == NULL) {
        return NULL;
    }

    /* 初始化默认窗口函数状态 */
    for (int i = 0; i < state->numFuncs; i++) {
        state->funcStates[i].wintype = WINDOWFUNC_ROW_NUMBER;
        state->funcStates[i].argIndex = -1;
        state->funcStates[i].defaultIsNull = true;
    }

    /* 初始化排序状态 */
    state->ordClause = node->ordClause;
    state->alreadySorted = false;

    /* 初始化其他字段 */
    state->ps.qual = NULL;
    state->ps.recheck = NULL;
    state->ps.ps_ProjInfo = NULL;
    state->ps.ps_ResultTupleDesc = NULL;
    state->ps.instrument = NULL;
    state->ps.needs_to_scan_queue = false;
    state->ps.chgParam = NULL;

    (void)eflags;
    return (PlanState *)state;
}

/**
 * @brief WindowAgg 节点执行函数（公共接口）
 */
TupleTableSlot *ExecWindowAgg(PlanState *pstate) {
    return exec_window_impl(pstate);
}

/**
 * @brief 结束 WindowAgg 节点
 */
void ExecEndWindowAgg(WindowAggState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放子节点 */
    if (node->ps.lefttree != NULL) {
        ExecEndNode(node->ps.lefttree);
        node->ps.lefttree = NULL;
    }

    /* 释放元组缓冲区 */
    free_partition_buffer(node);

    /* 释放窗口函数状态 */
    if (node->funcStates != NULL) {
        free(node->funcStates);
        node->funcStates = NULL;
    }

    /* 释放分区列索引 */
    if (node->partColIdx != NULL) {
        free(node->partColIdx);
        node->partColIdx = NULL;
    }

    /* 释放元组槽 */
    if (node->windowtuple != NULL) {
        FreeTupleTableSlot(node->windowtuple);
        node->windowtuple = NULL;
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
}

/**
 * @brief 重置 WindowAgg 节点
 */
void ExecReScanWindowAgg(WindowAggState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放分区缓冲区 */
    free_partition_buffer(node);

    /* 重新分配缓冲区 */
    node->partitionBuffers = (TupleTableSlot **)calloc(
        WINDOW_BUFFER_INITIAL_SIZE, sizeof(TupleTableSlot *));
    node->partitionBufferSize = WINDOW_BUFFER_INITIAL_SIZE;
    node->partitionBufferUsed = 0;
    node->currentPos = 0;

    /* 重置状态 */
    node->allDone = false;
    node->morePartitions = false;

    /* 重置窗口函数状态 */
    for (int i = 0; i < node->numFuncs; i++) {
        reset_window_func_state(&node->funcStates[i]);
    }

    /* 重置子节点 */
    if (node->ps.lefttree != NULL) {
        ExecReScan(node->ps.lefttree);
    }
}
