/* hashjoin_px_exec.c - 并行 Hash Join（Gap#4）
 *
 * worker 生命周期与失败路径整体沿用 exchange_exec.c 的复审后结构
 * （NULL 取消标志提交、per-节点完成计数、abort 唤醒消费者），
 * join 特有部分为：open 第一阶段串行排干 build 侧建表，
 * worker 循环体内对只读共享 vecx_hashjoin_t 并发 probe。 */
#include "db/executor/exec_hashjoin_px.h"
#include "db/executor/px_queue.h"
#include "db/executor/px_scheduler.h"
#include "db/executor/executor_framework.h"
#include "db/vectorized/vectorized.h"
#include "db/optimizer/optimizer.h"
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    ExecNode *build_child;
    px_subtree_fn make_probe;
    void *probe_ctx;
    int dop;
    int build_key_col, probe_key_col;
    vecx_hashjoin_t *hj;          /* open 建表；build 完成后只读 */
    px_queue_t *queue;
    volatile int cancel;
    int opened;
    /* per-节点完成计数：close 只等自己的 worker，不等全局调度器，
     * 避免嵌套/共存并行算子场景下 wait_idle 自死锁（Exchange 复审 I-2） */
    pthread_mutex_t mu;
    pthread_cond_t done_cond;
    int workers_outstanding;
    int sync_init;            /* mu/done_cond 已初始化，close 时幂等销毁 */
} HashJoinPxState;

typedef struct {
    ExecNode *probe_sub;          /* open 内同步创建（Task 4 修订语义） */
    vecx_hashjoin_t *hj;
    px_queue_t *queue;
    HashJoinPxState *st;          /* 取消读取与完成计数所属节点 */
} HjProbeArg;

/* worker 退出前的最后一步：所有 queue 操作（abort/producer_done）必须
 * 在此之前完成——close() 在计数归零后即销毁 queue */
static void hj_worker_done(HjProbeArg *arg) {
    HashJoinPxState *st = arg->st;
    pthread_mutex_lock(&st->mu);
    st->workers_outstanding--;
    pthread_cond_signal(&st->done_cond);
    pthread_mutex_unlock(&st->mu);
    free(arg);
}

static void hj_probe_worker(void *varg, volatile int *cancel_flag) {
    HjProbeArg *arg = (HjProbeArg *)varg;
    /* 提交时传 NULL 取消标志（px_scheduler 对 NULL 不做排队跳过，复审 C-1），
     * 参数恒为 NULL，禁止解引用；取消一律从 HashJoinPxState 读取 */
    (void)cancel_flag;
    volatile int *cancel = &arg->st->cancel;
    ExecNode *sub = arg->probe_sub;   /* open 内同步创建，归本 worker 所有 */

    /* 快速路径：拾取时已取消，跳过 open/循环；子树从未 open，
     * 只 exec_destroy（不可对未 open 的子树调 close），仍须
     * producer_done 一次以维持 EOF 计数不变式 */
    if (*cancel) {
        exec_destroy(sub);
        px_queue_producer_done(arg->queue);
        hj_worker_done(arg);
        return;
    }

    /* 必须用 exec_open（框架递归 children-first）而非裸 vtable：
     * 组合子树（filter→scan 等）的子节点只在 exec_open 时初始化 */
    if (exec_open(sub) != 0) {
        /* abort 唤醒消费者；此路径不再 producer_done */
        px_queue_abort(arg->queue);
        exec_destroy(sub);
        hj_worker_done(arg);
        return;
    }

    VectorBlock *pb;
    while (!*cancel && (pb = sub->next(sub)) != NULL) {
        VectorBlock *out = NULL;
        int n = vecx_hashjoin_probe(arg->hj, pb, &out);   /* 只读共享 hj */
        vector_block_destroy(pb);
        if (n < 0) {
            px_queue_abort(arg->queue);
            break;
        }
        if (n > 0 && out) {
            if (px_queue_push(arg->queue, out) != 0) {
                vector_block_destroy(out);   /* abort：push 失败所有权未移交 */
                break;
            }
        }
    }

    exec_close(sub);   /* 与 exec_open 配对：框架递归关闭整棵子树 */
    exec_destroy(sub);
    px_queue_producer_done(arg->queue);
    hj_worker_done(arg);
}

/* 等本节点的 worker 全部退出（调用前须已 abort queue） */
static void hjpx_wait_workers(HashJoinPxState *st) {
    pthread_mutex_lock(&st->mu);
    while (st->workers_outstanding > 0)
        pthread_cond_wait(&st->done_cond, &st->mu);
    pthread_mutex_unlock(&st->mu);
}

static int hjpx_open(ExecNode *node) {
    HashJoinPxState *st = (HashJoinPxState *)node->state;
    if (!st || !st->build_child || !st->make_probe) return -1;
    st->cancel = 0;

    st->hj = vecx_hashjoin_create(st->build_key_col, st->probe_key_col);
    if (!st->hj) return -1;

    /* 第一阶段：串行排干 build 侧建表；
     * 与 worker 同理必须用 exec_open/exec_close（框架递归），
     * 否则组合 build 子树（filter→scan 等）的子节点从未初始化 */
    ExecNode *bc = st->build_child;
    if (exec_open(bc) != 0) return -1;
    VectorBlock *bb;
    while ((bb = exec_next(bc)) != NULL) {
        int rc = vecx_hashjoin_add_build(st->hj, bb);
        vector_block_destroy(bb);
        if (rc != 0) { exec_close(bc); return -1; }
    }
    exec_close(bc);
    exec_destroy(bc);
    st->build_child = NULL;
    /* 此后 st->hj 只读 */

    st->queue = px_queue_create(64, st->dop);
    if (!st->queue) return -1;

    px_scheduler_t *sched = px_scheduler_default();
    for (int i = 0; i < st->dop; i++) {
        HjProbeArg *arg = (HjProbeArg *)calloc(1, sizeof(HjProbeArg));
        if (!arg) goto submit_fail;
        /* 同步创建 probe 子树（每 worker 一棵独立 ExecNode 树）：工厂只在
         * open 期间被调用，probe_ctx 无需活到 worker 运行期 */
        arg->probe_sub = st->make_probe(st->probe_ctx);
        if (!arg->probe_sub) {
            free(arg);
            goto submit_fail;
        }
        arg->hj = st->hj;
        arg->queue = st->queue;
        arg->st = st;
        /* 计数先于 submit，防竞态；submit 队列满时会阻塞，绝不能持 st->mu
         * 调用，否则与 worker 的计数递减互锁（Exchange 复审 M-4）。
         * cancel 标志传 NULL：调度器对 NULL 不做排队跳过，本节点的任务
         * 必定执行，worker 从 st->cancel 读取消（复审 C-1） */
        pthread_mutex_lock(&st->mu);
        st->workers_outstanding++;
        pthread_mutex_unlock(&st->mu);
        int rc = px_scheduler_submit(sched, hj_probe_worker, arg, NULL);
        if (rc != 0) {
            pthread_mutex_lock(&st->mu);
            st->workers_outstanding--;
            pthread_mutex_unlock(&st->mu);
            exec_destroy(arg->probe_sub);   /* 从未 open，worker 未接管 */
            free(arg);
            goto submit_fail;
        }
    }
    st->opened = 1;
    return 0;

submit_fail:
    /* 已提交的 worker 会看到 abort、自清理并递减计数；
     * 等计数归零后销毁 queue，避免泄漏 queue 与缓冲块（Exchange 复审 I-1） */
    st->cancel = 1;
    px_queue_abort(st->queue);
    hjpx_wait_workers(st);
    px_queue_destroy(st->queue);
    st->queue = NULL;
    return -1;
}

static VectorBlock *hjpx_next(ExecNode *node) {
    HashJoinPxState *st = (HashJoinPxState *)node->state;
    if (!st || !st->opened) return NULL;
    return px_queue_pop(st->queue);   /* NULL=EOF 或 abort */
}

static void hjpx_reset(ExecNode *node) { (void)node; }

static void hjpx_close(ExecNode *node) {
    HashJoinPxState *st = (HashJoinPxState *)node->state;
    if (!st) return;
    if (st->opened) {
        st->cancel = 1;                        /* 协作式取消 */
        px_queue_abort(st->queue);             /* 唤醒阻塞的 push/pop */
        hjpx_wait_workers(st);                 /* 只等本节点的 worker */
        px_queue_destroy(st->queue);           /* 残余块在此释放 */
        st->queue = NULL;
        st->opened = 0;
    }
    if (st->hj) {
        vecx_hashjoin_destroy(st->hj);   /* worker 已全部退出，安全销毁 */
        st->hj = NULL;
    }
    if (st->build_child) {               /* open 失败早退路径 */
        exec_destroy(st->build_child);
        st->build_child = NULL;
    }
    if (st->sync_init) {                 /* 幂等销毁同步对象 */
        pthread_mutex_destroy(&st->mu);
        pthread_cond_destroy(&st->done_cond);
        st->sync_init = 0;
    }
}

ExecNode *exec_create_hashjoin_px(ExecNode *build_child,
                                  px_subtree_fn make_probe, void *probe_ctx,
                                  int dop, int build_key_col, int probe_key_col) {
    if (!build_child || !make_probe) return NULL;
    HashJoinPxState *st = (HashJoinPxState *)calloc(1, sizeof(HashJoinPxState));
    if (!st) return NULL;
    st->build_child = build_child;
    st->make_probe = make_probe;
    st->probe_ctx = probe_ctx;
    st->dop = dop < 1 ? 1 : dop;
    st->build_key_col = build_key_col;
    st->probe_key_col = probe_key_col;
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
    node->node_type = PLAN_JOIN_HASH;
    node->state = st;
    node->open = hjpx_open;
    node->next = hjpx_next;
    node->reset = hjpx_reset;
    node->close = hjpx_close;
    return node;
}
