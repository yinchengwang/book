#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <vector>

extern "C" {
#include "db/vectorized/vectorized.h"
#include "db/executor/exec_hashjoin_px.h"
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

/* 测试辅助：集中回收 make_table 分配的列缓冲与元数据数组 */
static std::vector<void *> &hj_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

static ExecNode *make_table(const int32_t *keys, const int32_t *vals, int n, int batch) {
    int32_t *k = (int32_t *)malloc(sizeof(int32_t) * n);
    int32_t *v = (int32_t *)malloc(sizeof(int32_t) * n);
    memcpy(k, keys, sizeof(int32_t) * n);
    memcpy(v, vals, sizeof(int32_t) * n);
    /* exec_create_seqscan 只保存指针不拷贝（seqscan_exec.c），
     * 这三个元数据数组必须活到节点 open 之后，故堆分配并集中回收 */
    int *col_types = (int *)malloc(sizeof(int) * 2);
    col_types[0] = COLUMN_INT32;
    col_types[1] = COLUMN_INT32;
    void **col_data = (void **)malloc(sizeof(void *) * 2);
    col_data[0] = k;
    col_data[1] = v;
    int *elem = (int *)malloc(sizeof(int) * 2);
    elem[0] = (int)sizeof(int32_t);
    elem[1] = (int)sizeof(int32_t);
    hj_buffers().push_back(k);
    hj_buffers().push_back(v);
    hj_buffers().push_back(col_types);
    hj_buffers().push_back(col_data);
    hj_buffers().push_back(elem);
    return exec_create_seqscan(0, 2, col_types, col_data, elem, n, batch);
}

/* probe 工厂：把大行数 probe 表按 worker 切片。
 * next_base 无需互斥：工厂只在 hjpx_open 内被同步调用（Task 4 修订语义） */
struct ProbeCtx {
    const int32_t *keys;
    const int32_t *vals;
    int rows_per_worker;
    int batch;
    int next_base;
};

static ExecNode *make_probe_slice(void *vctx) {
    ProbeCtx *ctx = (ProbeCtx *)vctx;
    int base = ctx->next_base;
    ctx->next_base += ctx->rows_per_worker;
    return make_table(ctx->keys + base, ctx->vals + base, ctx->rows_per_worker, ctx->batch);
}

class HashJoinPxTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (void *p : hj_buffers()) free(p);
        hj_buffers().clear();
    }
};

TEST_F(HashJoinPxTest, ParallelProbeEqualsSerialVecx) {
    /* build 表：key 0..99，val = key*10 */
    const int kBuild = 100;
    std::vector<int32_t> bk(kBuild), bv(kBuild);
    for (int i = 0; i < kBuild; i++) { bk[i] = i; bv[i] = i * 10; }

    /* probe 表：4000 行，key = i%100（每 key 40 个 probe 行），val = i */
    const int kProbe = 4000, kDop = 4, kSlice = kProbe / kDop;
    std::vector<int32_t> pk(kProbe), pv(kProbe);
    for (int i = 0; i < kProbe; i++) { pk[i] = i % kBuild; pv[i] = i; }

    /* ---- 串行基线（直接 vecx_hashjoin） ---- */
    std::multiset<std::pair<int32_t, int32_t>> baseline;
    {
        vecx_hashjoin_t *hj = vecx_hashjoin_create(0, 0);
        ExecNode *bscan = make_table(bk.data(), bv.data(), kBuild, kBuild);
        bscan->open(bscan);
        VectorBlock *bb;
        while ((bb = bscan->next(bscan)) != nullptr) {
            ASSERT_EQ(vecx_hashjoin_add_build(hj, bb), 0);
            vector_block_destroy(bb);
        }
        bscan->close(bscan);
        exec_destroy(bscan);

        ExecNode *pscan = make_table(pk.data(), pv.data(), kProbe, 512);
        pscan->open(pscan);
        VectorBlock *pb;
        while ((pb = pscan->next(pscan)) != nullptr) {
            VectorBlock *out = nullptr;
            int n = vecx_hashjoin_probe(hj, pb, &out);
            if (n > 0 && out) {
                /* 输出列布局：build(k,v) + probe(k,v) */
                int32_t *bv_col = (int32_t *)out->columns[1];
                int32_t *pv_col = (int32_t *)out->columns[3];
                for (int r = 0; r < out->num_rows; r++)
                    baseline.insert({bv_col[r], pv_col[r]});
                vector_block_destroy(out);
            }
            vector_block_destroy(pb);
        }
        pscan->close(pscan);
        exec_destroy(pscan);
        vecx_hashjoin_destroy(hj);
    }

    /* ---- 并行 probe ---- */
    ExecNode *build_child = make_table(bk.data(), bv.data(), kBuild, kBuild);
    ProbeCtx pctx{pk.data(), pv.data(), kSlice, 512, 0};
    ExecNode *jnode = exec_create_hashjoin_px(build_child, make_probe_slice, &pctx,
                                              kDop, 0, 0);
    ASSERT_NE(jnode, nullptr);
    ASSERT_EQ(jnode->open(jnode), 0);

    std::multiset<std::pair<int32_t, int32_t>> parallel;
    VectorBlock *out;
    while ((out = jnode->next(jnode)) != nullptr) {
        int32_t *bv_col = (int32_t *)out->columns[1];
        int32_t *pv_col = (int32_t *)out->columns[3];
        for (int r = 0; r < out->num_rows; r++)
            parallel.insert({bv_col[r], pv_col[r]});
        vector_block_destroy(out);
    }
    jnode->close(jnode);
    exec_destroy(jnode);

    EXPECT_EQ(parallel, baseline);
    EXPECT_EQ(parallel.size(), (size_t)kProbe);  /* 每 probe 行恰一匹配 */
}
