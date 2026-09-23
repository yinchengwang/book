// engineering/test/db/executor/px_benchmark_test.cpp
// gap04 Task 6 验收基准：32M 行 int64，filter(col > 25% 分位) + SUM 聚合。
// 单线程基线 vs Exchange dop=4（每 worker 扫不相交切片、各自局部 filter，
// 主线程合并部分和）。SUM 可结合，合并语义正确。
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cstdlib>

extern "C" {
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
#include "db/vectorized/vectorized.h"
#include "db/executor/exec_exchange.h"
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/executor/px_scheduler.h"
}

/* 32M 行（256MB）：8M 行时串行基线仅 ~80ms，Exchange 固定开销（线程唤醒、
 * 队列、块分配）占比过大，实测只能到 ~2.7x；放大数据集摊薄固定开销后
 * 稳定 ≥3x（见 task-6 报告数字）。 */
static const int64_t kTotal = 32 * 1000 * 1000;
static std::vector<int64_t> g_data;

/* exec_create_seqscan 直接持有 col_types/col_data/col_elem_size 指针不拷贝
 * （operators/seqscan_exec.c:88-90），且 open 时才消费——栈数组会在
 * make_slice 返回后悬空。这里每切片堆分配三个小数组，登记到注册表，
 * 测试体末尾统一 free（g_data 本身是静态存储，无需堆）。 */
static std::vector<void *> g_heap_reg;

struct SliceCtx { int slice; int nworkers; };

static ExecNode *make_slice(void *vctx) {
    SliceCtx *ctx = (SliceCtx *)vctx;
    int64_t base = (kTotal / ctx->nworkers) * ctx->slice;
    int rows = (int)(kTotal / ctx->nworkers);
    int *col_types = (int *)malloc(sizeof(int));
    void **col_data = (void **)malloc(sizeof(void *));
    int *elem = (int *)malloc(sizeof(int));
    if (!col_types || !col_data || !elem) {
        free(col_types); free(col_data); free(elem);
        return nullptr;
    }
    col_types[0] = COLUMN_INT64;
    col_data[0] = g_data.data() + base;
    elem[0] = (int)sizeof(int64_t);
    /* 工厂由 Exchange 在 open 内同步逐 worker 调用（见 exec_exchange.h），
     * 注册表只在主线程被推，无并发。 */
    g_heap_reg.push_back(col_types);
    g_heap_reg.push_back(col_data);
    g_heap_reg.push_back(elem);
    return exec_create_seqscan(0, 1, col_types, col_data, elem, rows, 8192);
}

static double run_serial(int64_t *sum_out) {
    auto t0 = std::chrono::steady_clock::now();
    int64_t sum = 0;
    int64_t threshold = kTotal / 4;
    SliceCtx sc = {0, 1};
    ExecNode *scan = make_slice(&sc);
    if (!scan) { *sum_out = -1; return 1e30; }
    scan->open(scan);
    VectorBlock *b;
    while ((b = scan->next(scan)) != nullptr) {
        VectorBlock *f = nullptr;
        int n = vecx_filter_block(b, 0, CMP_GT, &threshold, &f);
        if (n > 0 && f) {
            double s; int has;
            vecx_agg_scalar(f, 0, VECX_AGG_SUM, nullptr, 0, &s, &has);
            if (has) sum += (int64_t)s;
            vector_block_destroy(f);
        }
        vector_block_destroy(b);
    }
    scan->close(scan);
    exec_destroy(scan);
    *sum_out = sum;
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

/* 并行 worker：scan -> 局部 filter（生产 exec_create_filter），过滤后块推回
 * Exchange，主线程只做 SUM。#39 修复后生产 filter 的三个缺陷（空块误报 EOF、
 * 输入块泄漏、worker open 非递归）均已消除，基准恢复覆盖生产路径。
 * 节点挂 left=scan，exec_destroy 会递归销毁子树并 free state。 */
struct ParCtx { SliceCtx slice; int64_t threshold; };

static ExecNode *make_filtered_slice(void *vctx) {
    ParCtx *ctx = (ParCtx *)vctx;
    ExecNode *scan = make_slice(&ctx->slice);
    if (!scan) return nullptr;
    vecx_pred_t pred;
    pred.col = 0;
    pred.op = CMP_GT;
    pred.i64 = ctx->threshold;
    pred.f64 = 0.0;
    pred.str = nullptr;
    ExecNode *filter = exec_create_filter(&pred);
    if (!filter) { exec_destroy(scan); return nullptr; }
    filter->left = scan;
    return filter;
}

static double run_parallel(int dop, int64_t *sum_out) {
    auto t0 = std::chrono::steady_clock::now();
    static ParCtx ctxs[PX_SCHED_MAX_WORKERS];
    /* 每个 worker 需要自己的 ctx（slice 不同）：Exchange 的 make_subtree
     * 在 open 内同步逐 worker 调一次，按调用序号分片 */
    struct FactoryCtx { ParCtx *ctxs; int next; } fctx = {ctxs, 0};
    for (int i = 0; i < dop; i++) {
        ctxs[i].slice.slice = i;
        ctxs[i].slice.nworkers = dop;
        ctxs[i].threshold = kTotal / 4;
    }
    auto factory = [](void *p) -> ExecNode * {
        FactoryCtx *f = (FactoryCtx *)p;
        return make_filtered_slice(&f->ctxs[f->next++]);
    };
    ExecNode *ex = exec_create_exchange(factory, &fctx, dop);
    if (!ex) { *sum_out = -1; return 1e30; }
    ex->open(ex);
    int64_t sum = 0;
    VectorBlock *b;
    while ((b = ex->next(ex)) != nullptr) {
        double s; int has;
        vecx_agg_scalar(b, 0, VECX_AGG_SUM, nullptr, 0, &s, &has);
        if (has) sum += (int64_t)s;
        vector_block_destroy(b);
    }
    ex->close(ex);
    exec_destroy(ex);
    *sum_out = sum;
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

TEST(PxBenchmark, FourWorkersSpeedupAtLeast3x) {
    /* 基准机 CPU < 4 核时跳过 */
    if (px_scheduler_num_workers(px_scheduler_default()) < 4) GTEST_SKIP();

    g_data.resize(kTotal);
    for (int64_t i = 0; i < kTotal; i++) g_data[i] = i;

    int64_t sum_serial = 0, sum_parallel = 0;
    double t_serial = run_serial(&sum_serial);
    double t_parallel = run_parallel(4, &sum_parallel);

    /* 释放 make_slice 登记的每切片堆数组（两条路径都已 destroy 完算子树） */
    for (void *p : g_heap_reg) free(p);
    g_heap_reg.clear();

    EXPECT_EQ(sum_serial, sum_parallel);              /* 结果一致 */
    double speedup = t_serial / t_parallel;
    ::testing::Test::RecordProperty("speedup", speedup);
    ::testing::Test::RecordProperty("serial_s", t_serial);
    ::testing::Test::RecordProperty("parallel_s", t_parallel);
    EXPECT_GE(speedup, 3.0)
        << "speedup " << speedup << "x (serial " << t_serial
        << "s, parallel " << t_parallel << "s)";
}
