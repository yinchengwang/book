#include <gtest/gtest.h>
#include <set>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>

extern "C" {
#include "db/executor/exec_exchange.h"
#include "db/vectorized/vectorized.h"   /* vecx_pred_t/vecx_expr_t：exec_operators.h 前置依赖 */
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/executor/px_queue.h"
#include "db/executor/px_wire.h"
#include "db/distributed/rpc.h"
#include "db/distributed/rpc_stream.h"   /* A6: rpc_stream_connect/rpc_stream_abort */
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

static std::vector<void *> &net_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

/* seqscan 原样存储列指针，列缓冲必须堆分配且活到测试结束（登记进 net_buffers） */
static ExecNode *make_scan_rows(int base, int rows, int batch) {
    int32_t *col = (int32_t *)malloc(sizeof(int32_t) * rows);
    for (int i = 0; i < rows; i++) col[i] = base + i;
    /* exec_create_seqscan 只保存指针不拷贝（见 seqscan_exec.c），
     * 这三个数组同样必须活到 open() 之后，故堆分配并集中回收
     * （对齐 exchange_exec_test 的 make_scan；brief 原稿用栈数组会悬空） */
    int *col_types = (int *)malloc(sizeof(int));
    col_types[0] = COLUMN_INT32;
    void **col_data = (void **)malloc(sizeof(void *));
    col_data[0] = col;
    int *elem = (int *)malloc(sizeof(int));
    elem[0] = (int)sizeof(int32_t);
    net_buffers().push_back(col);
    net_buffers().push_back(col_types);
    net_buffers().push_back(col_data);
    net_buffers().push_back(elem);
    return exec_create_seqscan(0, 1, col_types, col_data, elem, rows, batch);
}

struct NetScanCtx { int base; int rows; int batch; };
static ExecNode *net_scan_fn(void *vctx) {
    NetScanCtx *c = (NetScanCtx *)vctx;
    return make_scan_rows(c->base, c->rows, c->batch);
}

class ExchangeNetTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (void *p : net_buffers()) free(p);
        net_buffers().clear();
    }
};

TEST_F(ExchangeNetTest, SenderToReceiverLoopback) {
    const int kRows = 5000;
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19601;
    addr.node_id = 2;

    ExecNode *recv = exec_create_exchange_receiver(&addr);
    ASSERT_NE(recv, nullptr);
    ASSERT_EQ(recv->open(recv), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    NetScanCtx sctx{0, kRows, 512};
    ExecNode *send = exec_create_exchange_sender(net_scan_fn, &sctx, &addr);
    ASSERT_NE(send, nullptr);
    ASSERT_EQ(send->open(send), 0);
    while (send->next(send) != nullptr) {}   /* 驱动发送直到 EOF */
    send->close(send);
    exec_destroy(send);

    std::multiset<int> got;
    VectorBlock *b;
    while ((b = recv->next(recv)) != nullptr) {
        int32_t *col = (int32_t *)b->columns[0];
        for (int i = 0; i < b->num_rows; i++) got.insert(col[i]);
        px_wire_block_destroy(b);   /* A1: 深销毁——反序列化的 STRING 载荷随块释放 */
    }
    recv->close(recv);
    exec_destroy(recv);

    ASSERT_EQ(got.size(), (size_t)kRows);
    for (int i = 0; i < kRows; i++) EXPECT_EQ(got.count(i), 1u);
}

TEST_F(ExchangeNetTest, ReceiverReportsSenderAbort) {
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19602;
    addr.node_id = 3;

    ExecNode *recv = exec_create_exchange_receiver(&addr);
    ASSERT_NE(recv, nullptr);
    ASSERT_EQ(recv->open(recv), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* 模拟故障：连上后直接断开（不发 END） */
    rpc_stream_t *raw = rpc_stream_connect(&addr);
    ASSERT_NE(raw, nullptr);
    /* 强杀：不走 close（不发 END 帧），直接断 fd */
    rpc_stream_abort(raw);

    VectorBlock *b = recv->next(recv);
    EXPECT_EQ(b, nullptr);                        /* EOF/abort，不挂死 */
    recv->close(recv);
    exec_destroy(recv);
}
