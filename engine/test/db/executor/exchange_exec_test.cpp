#include <gtest/gtest.h>
#include <cstring>
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

/* gap04 三缺陷回归（D1 空块误报 EOF / D2 输入块泄漏 / D3 worker 裸 vtable）：
 * 子树是组合算子 filter(GT threshold) → seqscan，每 worker 扫一段从 0 开始的
 * 升序区间，threshold=500、batch=100 使前 5 个块全部零匹配：
 * - 修复前 D3：子树 scan 从未 open（seqscan source=NULL），worker 零行产出；
 * - 修复前 D1：即便 scan 已 open，首个全过滤块的 NULL 输出被误读为 EOF，
 *   查询在第 0 块处截断，同样零行；
 * - D2：输入块泄漏随本测试在重复运行下被覆盖（不崩溃/不截断即通过）。
 * 断言并行收集的多重集与串行期望值完全一致（并行/单机结果一致）。 */
struct FilterScanCtx {
    int rows_per_worker;   /* 每 worker 都扫 [0, rows_per_worker) 升序区间 */
    int batch;
    int64_t threshold;
};

static ExecNode *make_filter_scan(void *vctx) {
    FilterScanCtx *ctx = (FilterScanCtx *)vctx;
    std::lock_guard<std::mutex> lk(test_mutex());

    int n = ctx->rows_per_worker;
    int32_t *col = (int32_t *)malloc(sizeof(int32_t) * n);
    for (int i = 0; i < n; i++) col[i] = i;   /* 每 worker 区间从 0 开始：前若干块全被过滤 */
    /* 与 make_scan 同理：seqscan 只保存指针，堆分配并集中回收 */
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

    ExecNode *scan = exec_create_seqscan(0, 1, col_types, col_data, elem,
                                         n, ctx->batch);
    if (!scan) return nullptr;

    vecx_pred_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.col = 0;
    pred.op = CMP_GT;
    pred.i64 = ctx->threshold;
    ExecNode *filter = exec_create_filter(&pred);
    if (!filter) {
        exec_destroy(scan);
        return nullptr;
    }
    filter->left = scan;   /* 组合子树：filter 为根，scan 为子节点 */
    return filter;
}

TEST_F(ExchangeExecTest, ExchangeWrappedFilterChainMatchesSerial) {
    const int kDop = 2, kRows = 1000, kBatch = 100;
    const int64_t kThreshold = 500;
    FilterScanCtx ctx{kRows, kBatch, kThreshold};

    ExecNode *ex = exec_create_exchange(make_filter_scan, &ctx, kDop);
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

    /* 串行期望：每 worker 产出 (threshold, kRows) 全部值 */
    std::multiset<int> expected;
    for (int w = 0; w < kDop; w++)
        for (int v = (int)kThreshold + 1; v < kRows; v++)
            expected.insert(v);
    EXPECT_EQ(got, expected);
}
