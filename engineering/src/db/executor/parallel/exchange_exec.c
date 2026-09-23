/* exchange_exec.c - Exchange 算子 LOCAL 模式（Gap#4） */
#include "db/executor/exec_exchange.h"
#include "db/executor/executor_framework.h"
#include "db/executor/px_queue.h"
#include "db/executor/px_scheduler.h"
#include "db/optimizer/optimizer.h"
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    px_subtree_fn make_subtree;
    void *ctx;
    int dop;
    px_queue_t *queue;        /* open 时创建 */
    volatile int cancel;      /* 协作式取消标志 */
    int opened;
    ExecNode *single;         /* 直传模式（dop<=1） */
    /* per-Exchange 完成计数：close 只等自己的 worker，不等全局调度器，
     * 避免嵌套/共存 Exchange 场景下 wait_idle 自死锁（审查 I-2） */
    pthread_mutex_t mu;
    pthread_cond_t done_cond;
    int workers_outstanding;
    int sync_init;            /* mu/done_cond 已初始化，close 时幂等销毁 */
} ExchangeState;

typedef struct {
    px_subtree_fn make_subtree;
    void *ctx;
    px_queue_t *queue;
    ExchangeState *st;        /* 完成计数所属 Exchange */
} PxWorkerArg;

/* worker 退出前的最后一步：所有 queue 操作（abort/producer_done）必须
 * 在此之前完成——close() 在计数归零后即销毁 queue */
static void px_worker_done(PxWorkerArg *arg) {
    ExchangeState *st = arg->st;
    pthread_mutex_lock(&st->mu);
    st->workers_outstanding--;
    pthread_cond_signal(&st->done_cond);
    pthread_mutex_unlock(&st->mu);
    free(arg);
}

static void px_exchange_worker(void *varg, volatile int *cancel_flag) {
    PxWorkerArg *arg = (PxWorkerArg *)varg;
    /* 提交时传 NULL 取消标志（px_scheduler 对 NULL 不做排队跳过，复审 C-1），
     * 参数恒为 NULL，禁止解引用；取消一律从 ExchangeState 读取 */
    (void)cancel_flag;
    volatile int *cancel = &arg->st->cancel;

    /* 快速路径：拾取时已取消，跳过 make_subtree，但仍须 producer_done
     * 一次以维持 EOF 计数不变式 */
    if (*cancel) {
        px_queue_producer_done(arg->queue);
        px_worker_done(arg);
        return;
    }

    ExecNode *sub = arg->make_subtree(arg->ctx);
    if (!sub) { px_queue_abort(arg->queue); px_worker_done(arg); return; }

    if (sub->open(sub) != 0) {
        px_queue_abort(arg->queue);
        exec_destroy(sub);
        px_worker_done(arg);
        return;
    }

    VectorBlock *b;
    while (!*cancel && (b = sub->next(sub)) != NULL) {
        if (px_queue_push(arg->queue, b) != 0) {
            vector_block_destroy(b);   /* abort：push 失败所有权未移交 */
            break;
        }
    }

    sub->close(sub);
    exec_destroy(sub);
    px_queue_producer_done(arg->queue);
    px_worker_done(arg);
}

/* 等本 Exchange 的 worker 全部退出（调用前须已 abort queue） */
static void exchange_wait_workers(ExchangeState *st) {
    pthread_mutex_lock(&st->mu);
    while (st->workers_outstanding > 0)
        pthread_cond_wait(&st->done_cond, &st->mu);
    pthread_mutex_unlock(&st->mu);
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
        if (!arg) goto submit_fail;
        arg->make_subtree = st->make_subtree;
        arg->ctx = st->ctx;
        arg->queue = st->queue;
        arg->st = st;
        /* 计数先于 submit，防竞态；submit 队列满时会阻塞，绝不能持 st->mu
         * 调用，否则与 worker 的计数递减互锁（复审 M-4）。
         * cancel 标志传 NULL：调度器对 NULL 不做排队跳过，本 Exchange 的
         * 任务必定执行，worker 从 st->cancel 读取消（复审 C-1） */
        pthread_mutex_lock(&st->mu);
        st->workers_outstanding++;
        pthread_mutex_unlock(&st->mu);
        int rc = px_scheduler_submit(sched, px_exchange_worker, arg, NULL);
        if (rc != 0) {
            pthread_mutex_lock(&st->mu);
            st->workers_outstanding--;
            pthread_mutex_unlock(&st->mu);
            free(arg);
            goto submit_fail;
        }
    }
    st->opened = 1;
    return 0;

submit_fail:
    /* 已提交的 worker 会看到 abort、自清理并递减计数；
     * 等计数归零后销毁 queue，避免泄漏 queue 与缓冲块（审查 I-1） */
    st->cancel = 1;
    px_queue_abort(st->queue);
    exchange_wait_workers(st);
    px_queue_destroy(st->queue);
    st->queue = NULL;
    return -1;
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
    if (!st) return;

    if (st->opened) {
        if (st->dop <= 1) {
            st->single->close(st->single);
            exec_destroy(st->single);
            st->single = NULL;
        } else {
            st->cancel = 1;                        /* 协作式取消 */
            px_queue_abort(st->queue);             /* 唤醒阻塞的 push/pop */
            exchange_wait_workers(st);             /* 只等本 Exchange 的 worker */
            px_queue_destroy(st->queue);           /* 残余块在此释放 */
            st->queue = NULL;
        }
        st->opened = 0;
    }

    if (st->sync_init) {                           /* 幂等销毁同步对象 */
        pthread_mutex_destroy(&st->mu);
        pthread_cond_destroy(&st->done_cond);
        st->sync_init = 0;
    }
}

ExecNode *exec_create_exchange(px_subtree_fn make_subtree, void *ctx, int dop) {
    if (!make_subtree) return NULL;
    ExchangeState *st = (ExchangeState *)calloc(1, sizeof(ExchangeState));
    if (!st) return NULL;
    st->make_subtree = make_subtree;
    st->ctx = ctx;
    st->dop = dop < 1 ? 1 : dop;
    if (pthread_mutex_init(&st->mu, NULL) != 0) { free(st); return NULL; }
    if (pthread_cond_init(&st->done_cond, NULL) != 0) {
        pthread_mutex_destroy(&st->mu);
        free(st);
        return NULL;
    }
    st->workers_outstanding = 0;
    st->sync_init = 1;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        pthread_mutex_destroy(&st->mu);
        pthread_cond_destroy(&st->done_cond);
        free(st);
        return NULL;
    }
    node->node_type = PLAN_EXCHANGE;
    node->state = st;
    node->open = exchange_open;
    node->next = exchange_next;
    node->reset = exchange_reset;
    node->close = exchange_close;
    return node;
}
