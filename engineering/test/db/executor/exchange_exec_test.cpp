#include <gtest/gtest.h>
#include <mutex>
#include <set>
#include <vector>

extern "C" {
#include "db/executor/exec_exchange.h"
#include "db/vectorized/vectorized.h"
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

/* 测试辅助：集中回收 make_scan 分配的列缓冲 */
static std::vector<void *> &test_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

/* make_scan 由 dop 个 worker 线程并发调用，区间分配与缓冲登记须互斥 */
static std::mutex &test_mutex() {
    static std::mutex m;
    return m;
}

/* 每 worker 扫一段不相交的行区间 [base, base+rows) */
struct ScanCtx {
    int rows_per_worker;
    int batch;
    int next_base;
};

static ExecNode *make_scan(void *vctx) {
    ScanCtx *ctx = (ScanCtx *)vctx;
    std::lock_guard<std::mutex> lk(test_mutex());
    int base = ctx->next_base;
    ctx->next_base += ctx->rows_per_worker;

    int32_t *col = (int32_t *)malloc(sizeof(int32_t) * ctx->rows_per_worker);
    for (int i = 0; i < ctx->rows_per_worker; i++) col[i] = base + i;
    /* exec_create_seqscan 只保存指针不拷贝（见 seqscan_exec.c），
     * 这三个数组必须活到 worker 的 open() 之后，故堆分配并集中回收 */
    int *col_types = (int *)malloc(sizeof(int));
    col_types[0] = COLUMN_INT32;
    void **col_data = (void **)malloc(sizeof(void *));
    col_data[0] = col;
    int *elem = (int *)malloc(sizeof(int));
    elem[0] = (int)sizeof(int32_t);
    test_buffers().push_back(col);
    test_buffers().push_back(col_types);
    test_buffers().push_back(col_data);
    test_buffers().push_back(elem);
    return exec_create_seqscan(0, 1, col_types, col_data, elem,
                               ctx->rows_per_worker, ctx->batch);
}

class ExchangeExecTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (void *p : test_buffers()) free(p);
        test_buffers().clear();
    }
};

TEST_F(ExchangeExecTest, ParallelScanUnionEqualsSerial) {
    const int kDop = 4, kRows = 1000, kBatch = 256;
    ScanCtx ctx{kRows, kBatch, 0};

    ExecNode *ex = exec_create_exchange(make_scan, &ctx, kDop);
    ASSERT_NE(ex, nullptr);
    ASSERT_EQ(ex->open(ex), 0);

    std::multiset<int> got;
    VectorBlock *b;
    while ((b = ex->next(ex)) != nullptr) {
        int32_t *col = (int32_t *)b->columns[0];
        for (int i = 0; i < b->num_rows; i++) got.insert(col[i]);
        vector_block_destroy(b);
    }
    ex->close(ex);
    exec_destroy(ex);

    ASSERT_EQ(got.size(), (size_t)kDop * kRows);
    for (int i = 0; i < kDop * kRows; i++) {
        EXPECT_EQ(got.count(i), 1u) << "missing/duplicate row " << i;
    }
}

TEST_F(ExchangeExecTest, DopOneIsPassThrough) {
    ScanCtx ctx{100, 64, 0};
    ExecNode *ex = exec_create_exchange(make_scan, &ctx, 1);
    ASSERT_NE(ex, nullptr);
    ASSERT_EQ(ex->open(ex), 0);
    int total = 0;
    VectorBlock *b;
    while ((b = ex->next(ex)) != nullptr) {
        total += b->num_rows;
        vector_block_destroy(b);
    }
    ex->close(ex);
    exec_destroy(ex);
    EXPECT_EQ(total, 100);
}

TEST_F(ExchangeExecTest, CloseWithoutFullDrainReclaimsWorkers) {
    ScanCtx ctx{100000, 4096, 0};        /* 数据量大，消费一块就 close */
    ExecNode *ex = exec_create_exchange(make_scan, &ctx, 4);
    ASSERT_NE(ex, nullptr);
    ASSERT_EQ(ex->open(ex), 0);
    VectorBlock *b = ex->next(ex);
    ASSERT_NE(b, nullptr);
    vector_block_destroy(b);
    ex->close(ex);                        /* 须取消悬挂 worker 且不死锁 */
    exec_destroy(ex);
    SUCCEED();
}

/* 嵌套 Exchange 回归（审查 I-2）：外层 worker 的子树是内层 Exchange，
 * 内层 close 若等全局调度器 wait_idle 会自等自死锁；
 * per-Exchange 完成计数下必须正常完成。 */
static ExecNode *make_inner_exchange(void *vctx) {
    /* 内层 Exchange dop=2，叶子复用 make_scan（同一 ScanCtx，mutex 分配区间） */
    return exec_create_exchange(make_scan, vctx, 2);
}

TEST_F(ExchangeExecTest, NestedExchangeClosesWithoutDeadlock) {
    const int kRows = 50;
    ScanCtx ctx{kRows, 64, 0};
    ExecNode *outer = exec_create_exchange(make_inner_exchange, &ctx, 2);
    ASSERT_NE(outer, nullptr);
    ASSERT_EQ(outer->open(outer), 0);

    int total = 0;
    VectorBlock *b;
    while ((b = outer->next(outer)) != nullptr) {
        total += b->num_rows;
        vector_block_destroy(b);
    }
    outer->close(outer);                  /* 旧实现在此自死锁 */
    exec_destroy(outer);
    EXPECT_EQ(total, 2 * 2 * kRows);
}
