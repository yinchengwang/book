/* exchange_net_exec.c - Exchange 网络模式（Gap#4 Task 10）
 *
 * sender：open 时同步创建子树并 exec_open，首次 next 驱动整棵子树、
 * 逐块 px_wire_serialize 后经 rpc_stream 发送，子树 EOF 发 LAST 标记帧，
 * close 的 rpc_stream_close 再发 END 帧。对上层 next 恒 NULL（无产出 scan）。
 *
 * receiver：open 创建 px_queue(nproducers=1) + rpcs_listen；监听线程
 * 按帧回调解包投入队列，next() 从队列拉块。LAST/END=EOF；
 * ERR/CRC 失败/对端异常断开(RPCS_FRAME_ABORT)=abort→next 恒 NULL。
 *
 * controller annotations：
 * - A2：子树驱动必须走 exec_open/exec_close/exec_destroy（框架递归），
 *   裸 vtable 对组合子树（filter→scan）会静默零行。
 * - A3：LAST 与 END 都会触发 producer_done，done_called 去重（cb 单线程）。
 * - A4：receiver_close 顺序 abort(queue) → listener_stop → queue_destroy，
 *   防监听线程阻塞在满队列 push 时 join 死锁。
 * - A5：RPCS_FRAME_ABORT 与 ERR/CRC 失败同路径（aborted + queue_abort）。
 */
#include "db/executor/exec_exchange.h"
#include "db/executor/executor_framework.h"
#include "db/executor/px_queue.h"
#include "db/executor/px_wire.h"
#include "db/distributed/rpc_stream.h"
#include "db/optimizer/optimizer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ---- sender ---- */

typedef struct {
    px_subtree_fn make_subtree;
    void *ctx;
    rpc_node_address_t addr;
    rpc_stream_t *stream;
    ExecNode *sub;
    uint32_t seq;
    int failed;
    int last_sent;          /* 重复 next（EOF 后）不重复发 LAST */
} SenderState;

static int sender_open(ExecNode *node) {
    SenderState *st = (SenderState *)node->state;
    if (!st || !st->make_subtree) return -1;
    st->sub = st->make_subtree(st->ctx);
    if (!st->sub) return -1;
    if (exec_open(st->sub) != 0) {          /* A2：框架递归打开组合子树 */
        exec_destroy(st->sub);
        st->sub = NULL;
        return -1;
    }
    st->stream = rpc_stream_connect(&st->addr);
    if (!st->stream) {
        exec_close(st->sub);
        exec_destroy(st->sub);
        st->sub = NULL;
        return -1;
    }
    return 0;
}

static VectorBlock *sender_next(ExecNode *node) {
    SenderState *st = (SenderState *)node->state;
    if (!st || !st->sub || st->failed || st->last_sent) return NULL;

    VectorBlock *b;
    while ((b = st->sub->next(st->sub)) != NULL) {
        uint8_t *buf = NULL;
        uint32_t size = 0;
        if (px_wire_serialize(b, st->seq, 0, NULL, &buf, &size) != 0) {
            vector_block_destroy(b);   /* 发送方自有块：非反序列化来源，普通销毁 */
            st->failed = 1;
            return NULL;
        }
        vector_block_destroy(b);
        int rc = rpc_stream_send(st->stream, RPCS_FRAME_DATA, buf, size);
        free(buf);
        if (rc != 0) { st->failed = 1; return NULL; }
        st->seq++;
    }

    /* 子树 EOF：发 LAST 标记帧（close 的 END 帧随后） */
    uint8_t *buf = NULL;
    uint32_t size = 0;
    if (px_wire_serialize(NULL, st->seq, PXW_FLAG_LAST, NULL, &buf, &size) == 0) {
        rpc_stream_send(st->stream, RPCS_FRAME_DATA, buf, size);
        free(buf);
    }
    st->last_sent = 1;
    return NULL;   /* sender 对上层无产出 */
}

static void sender_reset(ExecNode *node) { (void)node; }

static void sender_close(ExecNode *node) {
    SenderState *st = (SenderState *)node->state;
    if (!st) return;
    if (st->sub) {
        exec_close(st->sub);        /* A2：与 exec_open 配对 */
        exec_destroy(st->sub);
        st->sub = NULL;
    }
    if (st->stream) {
        rpc_stream_close(st->stream);   /* 自动 END 帧 */
        st->stream = NULL;
    }
}

ExecNode *exec_create_exchange_sender(px_subtree_fn make_subtree, void *ctx,
                                      const rpc_node_address_t *addr) {
    if (!make_subtree || !addr) return NULL;
    SenderState *st = (SenderState *)calloc(1, sizeof(SenderState));
    if (!st) return NULL;
    st->make_subtree = make_subtree;
    st->ctx = ctx;
    st->addr = *addr;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) { free(st); return NULL; }
    node->node_type = PLAN_EXCHANGE;
    node->state = st;
    node->open = sender_open;
    node->next = sender_next;
    node->reset = sender_reset;
    node->close = sender_close;
    return node;
}

/* ---- receiver ---- */

typedef struct {
    rpc_node_address_t bind_addr;
    rpcs_listener_t *listener;
    px_queue_t *queue;       /* 单生产者（监听线程）语义：nproducers=1 */
    volatile int aborted;
    int done_called;         /* A3：LAST 与 END 双路径 producer_done 去重 */
} ReceiverState;

/* 在监听线程上下文执行（单线程，done_called 无需同步）。
 * px_queue_push 因队列满而阻塞监听线程——有意的背压传导（TCP 窗口随之收紧）；
 * receiver_close 先 abort 队列解除该阻塞（A4）。 */
static void receiver_frame_cb(uint8_t frame_type, const uint8_t *data,
                              uint32_t size, void *vctx) {
    ReceiverState *st = (ReceiverState *)vctx;
    if (frame_type == RPCS_FRAME_END) {
        if (!st->done_called) {   /* A3 */
            st->done_called = 1;
            px_queue_producer_done(st->queue);
        }
        return;
    }
    if (frame_type == RPCS_FRAME_ABORT) {   /* A5：对端异常断开，视同 ERR */
        st->aborted = 1;
        px_queue_abort(st->queue);
        return;
    }
    uint32_t seq, flags;
    VectorBlock *b = px_wire_deserialize(data, size, &seq, &flags);
    if (flags == PXW_DESER_CRC_FAIL || (flags & PXW_FLAG_ERR)) {
        st->aborted = 1;
        px_queue_abort(st->queue);
        return;
    }
    if (flags & PXW_FLAG_LAST) {
        if (!st->done_called) {   /* A3 */
            st->done_called = 1;
            px_queue_producer_done(st->queue);
        }
        return;
    }
    if (b) {
        if (px_queue_push(st->queue, b) != 0) {
            px_wire_block_destroy(b);   /* A1：深销毁（STRING 串载荷） */
        }
    }
}

static int receiver_open(ExecNode *node) {
    ReceiverState *st = (ReceiverState *)node->state;
    if (!st) return -1;
    st->queue = px_queue_create(64, 1);
    if (!st->queue) return -1;
    st->listener = rpcs_listen(&st->bind_addr, receiver_frame_cb, st);
    if (!st->listener) {
        px_queue_destroy(st->queue);
        st->queue = NULL;
        return -1;
    }
    return 0;
}

static VectorBlock *receiver_next(ExecNode *node) {
    ReceiverState *st = (ReceiverState *)node->state;
    if (!st || !st->queue) return NULL;
    return px_queue_pop(st->queue);   /* LAST/END→EOF；ERR/CRC/ABORT→abort→NULL */
}

static void receiver_reset(ExecNode *node) { (void)node; }

static void receiver_close(ExecNode *node) {
    ReceiverState *st = (ReceiverState *)node->state;
    if (!st) return;
    /* A4：先 abort 队列——监听线程若阻塞在满队列 push 上会被唤醒
     * （push 失败、cb 返回），随后 stop 的 pthread_join 才不会死锁 */
    if (st->queue) px_queue_abort(st->queue);
    if (st->listener) {
        rpcs_listener_stop(st->listener);
        st->listener = NULL;
    }
    if (st->queue) {
        px_queue_destroy(st->queue);   /* 残余块回收（沿用普通 destroy，已知接受限制） */
        st->queue = NULL;
    }
}

ExecNode *exec_create_exchange_receiver(const rpc_node_address_t *bind_addr) {
    if (!bind_addr) return NULL;
    ReceiverState *st = (ReceiverState *)calloc(1, sizeof(ReceiverState));
    if (!st) return NULL;
    st->bind_addr = *bind_addr;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) { free(st); return NULL; }
    node->node_type = PLAN_EXCHANGE;
    node->state = st;
    node->open = receiver_open;
    node->next = receiver_next;
    node->reset = receiver_reset;
    node->close = receiver_close;
    return node;
}
