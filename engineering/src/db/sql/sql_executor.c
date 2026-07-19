/**
 * @file sql_executor.c
 * @brief 查询执行器实现
 *
 * 基于 Volcano 迭代器模型：
 * - TupleTableSlot: 元组槽
 * - PlanState: 执行器状态
 * - exec_proc: 迭代函数指针
 */

#include "db/sql/sql_executor.h"
#include "db/sql/sql_planner.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 元组描述符操作
 * ======================================================================== */

ExecTupleDesc *exec_make_tuple_desc(int natts)
{
    if (natts <= 0) return NULL;

    ExecTupleDesc *desc = (ExecTupleDesc *)calloc(1, sizeof(ExecTupleDesc));
    if (!desc) return NULL;

    desc->natts = natts;
    desc->attrs = (struct AttInhData_s *)calloc((size_t)natts, sizeof(struct AttInhData_s));
    if (!desc->attrs) {
        /* attrs 分配失败时也要释放 desc */
        free(desc);
        return NULL;
    }

    return desc;
}

void exec_drop_tuple_desc(ExecTupleDesc *desc)
{
    if (!desc) return;
    if (desc->attrs) free(desc->attrs);
    free(desc);
}

/* ========================================================================
 * 元组槽操作
 * ======================================================================== */

TupleTableSlot *exec_make_tuple_slot(ExecTupleDesc *desc)
{
    if (!desc) return NULL;

    TupleTableSlot *slot = (TupleTableSlot *)calloc(1, sizeof(TupleTableSlot));
    if (!slot) return NULL;

    slot->tts_tupleDescriptor = desc;
    slot->tts_values = (void **)calloc((size_t)desc->natts, sizeof(void *));
    slot->tts_isnull = (bool *)calloc((size_t)desc->natts, sizeof(bool));
    slot->tts_nvalid = 0;
    slot->tts_type = TTS_VIRTUAL;

    return slot;
}

void exec_drop_tuple_slot(TupleTableSlot *slot)
{
    if (!slot) return;
    if (slot->tts_values) free(slot->tts_values);
    if (slot->tts_isnull) free(slot->tts_isnull);
    free(slot);
}

TupleTableSlot *exec_copy_tuple_slot(const TupleTableSlot *src)
{
    if (!src) return NULL;

    TupleTableSlot *dst = exec_make_tuple_slot(src->tts_tupleDescriptor);
    if (!dst) return NULL;

    dst->tts_nvalid = src->tts_nvalid;
    memcpy(dst->tts_values, src->tts_values, (size_t)src->tts_nvalid * sizeof(void *));
    memcpy(dst->tts_isnull, src->tts_isnull, (size_t)src->tts_nvalid * sizeof(bool));

    return dst;
}

void exec_clear_tuple_slot(TupleTableSlot *slot)
{
    if (!slot) return;

    slot->tts_nvalid = 0;
    if (slot->tts_isnull && slot->tts_tupleDescriptor) {
        memset(slot->tts_isnull, 0, (size_t)slot->tts_tupleDescriptor->natts * sizeof(bool));
    }
}

void *slot_get_value(TupleTableSlot *slot, int attnum, int *len)
{
    if (!slot || !slot->tts_tupleDescriptor) return NULL;
    if (attnum < 1 || attnum > slot->tts_tupleDescriptor->natts) return NULL;

    int idx = attnum - 1;
    if (slot->tts_isnull && slot->tts_isnull[idx]) {
        if (len) *len = -1;
        return NULL;
    }

    if (len) *len = slot->tts_tupleDescriptor->attrs[idx].len;
    return slot->tts_values[idx];
}

void slot_set_value(TupleTableSlot *slot, int attnum, const void *value, int len)
{
    if (!slot || !slot->tts_tupleDescriptor) return;
    if (attnum < 1 || attnum > slot->tts_tupleDescriptor->natts) return;

    int idx = attnum - 1;

    if (value == NULL) {
        if (slot->tts_isnull) slot->tts_isnull[idx] = true;
        if (slot->tts_values) slot->tts_values[idx] = NULL;
    } else {
        if (slot->tts_isnull) slot->tts_isnull[idx] = false;
        if (slot->tts_values) slot->tts_values[idx] = (void *)value;
        if (slot->tts_tupleDescriptor->attrs) {
            slot->tts_tupleDescriptor->attrs[idx].len = len;
        }
    }

    if ((size_t)idx >= slot->tts_nvalid) {
        slot->tts_nvalid = idx + 1;
    }
}

bool slot_attisnull(const TupleTableSlot *slot, int attnum)
{
    if (!slot || !slot->tts_tupleDescriptor) return true;
    if (attnum < 1 || attnum > slot->tts_tupleDescriptor->natts) return true;
    if (!slot->tts_isnull) return true;
    return slot->tts_isnull[attnum - 1];
}

void slot_set_null(TupleTableSlot *slot, int attnum)
{
    if (!slot || !slot->tts_tupleDescriptor) return;
    if (attnum < 1 || attnum > slot->tts_tupleDescriptor->natts) return;
    if (slot->tts_isnull) slot->tts_isnull[attnum - 1] = true;
    if (slot->tts_values) slot->tts_values[attnum - 1] = NULL;
}

/* ========================================================================
 * 表达式上下文
 * ======================================================================== */

ExprContext *exec_create_expr_context(void)
{
    ExprContext *ctx = (ExprContext *)calloc(1, sizeof(ExprContext));
    if (!ctx) return NULL;

    ctx->scandesc = (TupleTableSlot **)calloc(16, sizeof(TupleTableSlot *));
    ctx->numscans = 0;
    ctx->projection_depth = 0;

    return ctx;
}

void exec_destroy_expr_context(ExprContext *ctx)
{
    if (!ctx) return;
    if (ctx->scandesc) free(ctx->scandesc);
    free(ctx);
}

void exec_push_expr_context(ExprContext *ctx)
{
    (void)ctx;
}

void exec_pop_expr_context(ExprContext *ctx)
{
    (void)ctx;
}

/* ========================================================================
 * 执行器流水线
 * ======================================================================== */

/**
 * @brief 创建执行器
 */
void *executor_create(void) {
    ExecutorGlobal *exec = (ExecutorGlobal *)calloc(1, sizeof(ExecutorGlobal));
    if (exec) {
        exec->forward = true;
        exec->count = 0;
    }
    return exec;
}

/**
 * @brief 初始化执行器
 */
void executor_init(void *exec, void *plan, void *params) {
    if (!exec) return;

    ExecutorGlobal *e = (ExecutorGlobal *)exec;
    (void)params;

    /* 如果传入的是 PhysPlan，创建 PlanState */
    if (plan) {
        PhysPlan *phys_plan = (PhysPlan *)plan;
        e->estate = planner_create_plan_state(phys_plan);
    }
}

/**
 * @brief 执行查询
 */
long executor_run(void *exec, void *plan, ScanDirection direction, long count, void *dest) {
    if (!exec) return 0;

    ExecutorGlobal *e = (ExecutorGlobal *)exec;
    long tuples_returned = 0;

    e->forward = (direction == ForwardScanDirection);
    e->count = 0;

    /* 获取 PlanState（存储在 estate 中） */
    PlanState *state = (PlanState *)e->estate;

    /* Volcano 模型：迭代拉取元组 */
    while (tuples_returned < count || count <= 0) {
        TupleTableSlot *slot = NULL;

        if (state && state->exec_proc) {
            slot = state->exec_proc(state);
        }

        if (!slot) break;  /* 没有更多元组 */

        /* 发送到目的地 */
        if (dest) {
            DestReceiver *receiver = (DestReceiver *)dest;
            ReceiveTuple(receiver, slot);
        }

        tuples_returned++;
    }

    e->count = tuples_returned;
    return tuples_returned;
}

/**
 * @brief 继续执行
 */
bool executor_continue(void *exec) {
    if (!exec) return false;

    ExecutorGlobal *e = (ExecutorGlobal *)exec;
    PlanState *state = (PlanState *)e->estate;

    /* 继续执行直到没有更多元组 */
    while (state && state->exec_proc) {
        TupleTableSlot *slot = state->exec_proc(state);
        if (!slot) break;
        e->count++;
    }

    return e->count > 0;
}

/**
 * @brief 结束执行
 */
void executor_finish(void *exec) {
    if (!exec) return;

    ExecutorGlobal *e = (ExecutorGlobal *)exec;

    /* 清理 PlanState 树 */
    if (e->estate) {
        planner_destroy_plan_state((PlanState *)e->estate);
        e->estate = NULL;
    }
}

/**
 * @brief 取消执行
 */
void executor_cancel(void *exec) {
    if (!exec) return;
    executor_finish(exec);
}

/**
 * @brief 开始执行
 */
void executor_start(void *exec, void *plan, void *params) {
    executor_init(exec, plan, params);
}

/**
 * @brief 销毁执行器
 */
void executor_destroy(void *exec) {
    if (!exec) return;
    executor_finish(exec);
    free(exec);
}

/* ========================================================================
 * 表达式求值辅助函数（保留）
 * ======================================================================== */

void *eval_case(void *caseexpr, ExprContext *ctx)
{
    /* 桩实现：返回 NULL */
    (void)caseexpr;
    (void)ctx;
    return NULL;
}

bool eval_in(void *in, void *expr, ExprContext *ctx)
{
    /* 桩实现：返回 false */
    (void)in;
    (void)expr;
    (void)ctx;
    return false;
}

bool eval_nulltest(void *expr, bool is_not_null, ExprContext *ctx)
{
    /* 桩实现：返回 false */
    (void)expr;
    (void)is_not_null;
    (void)ctx;
    return false;
}

bool eval_bool_expr(void *expr, ExprContext *ctx)
{
    /* 桩实现：返回 true */
    (void)expr;
    (void)ctx;
    return true;
}

/* ========================================================================
 * 结果集管理
 * ======================================================================== */

DestReceiver *CreateDestReceiver(DestReceiverType type)
{
    DestReceiver *self = (DestReceiver *)calloc(1, sizeof(DestReceiver));
    if (!self) return NULL;

    self->dest = type;
    self->state = NULL;

    return self;
}

void ReceiveTuple(DestReceiver *self, TupleTableSlot *slot)
{
    if (!self || !slot) return;
    (void)self;
}

void DestroyDestReceiver(DestReceiver *self)
{
    if (self) {
        free(self);
    }
}

void *CreateTupleStore(void)
{
    /* 桩实现：返回 NULL */
    return NULL;
}

void tuple_store_put(void *store, TupleTableSlot *slot)
{
    (void)store;
    (void)slot;
}

TupleTableSlot *tuple_store_get(void *store, bool forward)
{
    (void)store;
    (void)forward;
    return NULL;
}

void tuple_store_close(void *store)
{
    (void)store;
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

int tuple_compare(const void *a, const void *b, void *arg)
{
    (void)a;
    (void)b;
    (void)arg;
    return 0;
}

void *tuple_copy(const void *src, int len)
{
    if (!src || len <= 0) return NULL;

    void *dst = malloc(len);
    if (dst) {
        memcpy(dst, src, len);
    }
    return dst;
}

void *tuple_alloc(int len)
{
    return malloc(len);
}

void tuple_free(void *tuple)
{
    free(tuple);
}

ExecTupleDesc *get_cached_rowtype(int type_id, int typlevel)
{
    (void)type_id;
    (void)typlevel;
    return NULL;
}

void ReleaseCachedRowtype(ExecTupleDesc *desc)
{
    exec_drop_tuple_desc(desc);
}

/* ========================================================================
 * 调试
 * ======================================================================== */

void executor_dump_state(PlanState *state)
{
    if (!state) return;
    printf("ExecutorState: type=%d\n", (int)state->type);
}

void print_tuple(const TupleTableSlot *slot)
{
    if (!slot) {
        printf("(null tuple)\n");
        return;
    }

    printf("Tuple: ");
    if (slot->tts_tupleDescriptor && slot->tts_values) {
        for (int i = 0; i < slot->tts_tupleDescriptor->natts; i++) {
            if (i > 0) printf(", ");
            if (slot->tts_isnull && slot->tts_isnull[i]) {
                printf("NULL");
            } else {
                printf("%p", slot->tts_values[i]);
            }
        }
    }
    printf("\n");
}

void print_tuple_data(const ExecTupleDesc *desc, const void *data)
{
    if (!desc || !data) return;
    (void)data;
    /* 桩实现：暂不打印 */
}
