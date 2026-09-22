/* exchange_exec.c - Exchange 算子 LOCAL 模式（Gap#4） */
#include "db/executor/exec_exchange.h"
#include "db/executor/executor_framework.h"
#include "db/executor/px_queue.h"
#include "db/executor/px_scheduler.h"
#include "db/optimizer/optimizer.h"
#include <stdlib.h>

typedef struct {
    px_subtree_fn make_subtree;
    void *ctx;
    int dop;
    px_queue_t *queue;        /* open 时创建 */
    volatile int cancel;      /* 协作式取消标志 */
    int opened;
    ExecNode *single;         /* 直传模式（dop<=1） */
} ExchangeState;

typedef struct {
    px_subtree_fn make_subtree;
    void *ctx;
    px_queue_t *queue;
} PxWorkerArg;

static void px_exchange_worker(void *varg, volatile int *cancel_flag) {
    PxWorkerArg *arg = (PxWorkerArg *)varg;

    ExecNode *sub = arg->make_subtree(arg->ctx);
    if (!sub) { px_queue_abort(arg->queue); free(arg); return; }

    if (sub->open(sub) != 0) {
        px_queue_abort(arg->queue);
        exec_destroy(sub);
        free(arg);
        return;
    }

    VectorBlock *b;
    while (!*cancel_flag && (b = sub->next(sub)) != NULL) {
        if (px_queue_push(arg->queue, b) != 0) {
            vector_block_destroy(b);   /* abort：push 失败所有权未移交 */
            break;
        }
    }

    sub->close(sub);
    exec_destroy(sub);
    px_queue_producer_done(arg->queue);
    free(arg);
}

static int exchange_open(ExecNode *node) {
    ExchangeState *st = (ExchangeState *)node->state;
    if (!st || !st->make_subtree) return -1;
    st->cancel = 0;

    if (st->dop <= 1) {           /* 直传：不开线程 */
        st->single = st->make_subtree(st->ctx);
        if (!st->single) return -1;
        st->opened = 1;
        return st->single->open(st->single);
    }

    st->queue = px_queue_create(64, st->dop);
    if (!st->queue) return -1;

    px_scheduler_t *sched = px_scheduler_default();
    for (int i = 0; i < st->dop; i++) {
        PxWorkerArg *arg = (PxWorkerArg *)calloc(1, sizeof(PxWorkerArg));
        if (!arg) { px_queue_abort(st->queue); return -1; }
        arg->make_subtree = st->make_subtree;
        arg->ctx = st->ctx;
        arg->queue = st->queue;
        if (px_scheduler_submit(sched, px_exchange_worker, arg, &st->cancel) != 0) {
            free(arg);
            px_queue_abort(st->queue);
            return -1;
        }
    }
    st->opened = 1;
    return 0;
}

static VectorBlock *exchange_next(ExecNode *node) {
    ExchangeState *st = (ExchangeState *)node->state;
    if (!st || !st->opened) return NULL;
    if (st->dop <= 1) return st->single->next(st->single);
    return px_queue_pop(st->queue);   /* NULL=EOF 或 abort */
}

static void exchange_reset(ExecNode *node) {
    /* 并行迭代不支持原地重置；要求 close 后重新 open */
    (void)node;
}

static void exchange_close(ExecNode *node) {
    ExchangeState *st = (ExchangeState *)node->state;
    if (!st || !st->opened) return;

    if (st->dop <= 1) {
        st->single->close(st->single);
        exec_destroy(st->single);
        st->single = NULL;
    } else {
        st->cancel = 1;                            /* 协作式取消 */
        px_queue_abort(st->queue);                 /* 唤醒阻塞的 push/pop */
        px_scheduler_wait_idle(px_scheduler_default());
        px_queue_destroy(st->queue);               /* 残余块在此释放 */
        st->queue = NULL;
    }
    st->opened = 0;
}

ExecNode *exec_create_exchange(px_subtree_fn make_subtree, void *ctx, int dop) {
    if (!make_subtree) return NULL;
    ExchangeState *st = (ExchangeState *)calloc(1, sizeof(ExchangeState));
    if (!st) return NULL;
    st->make_subtree = make_subtree;
    st->ctx = ctx;
    st->dop = dop < 1 ? 1 : dop;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) { free(st); return NULL; }
    node->node_type = PLAN_EXCHANGE;
    node->state = st;
    node->open = exchange_open;
    node->next = exchange_next;
    node->reset = exchange_reset;
    node->close = exchange_close;
    return node;
}
