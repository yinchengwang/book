/* distributed_join_test.cpp - Gap#4 Task 11 双实例分布式 Join 验收
 *
 * 组合 Task 10 Exchange sender/receiver 与 vecx_hashjoin：
 * - BROADCAST：远端小表经网络送达 → 本地建 hash 表 → 本地大表 probe，
 *   结果与串行基线完全一致（4000 行）。
 * - REPARTITION：两侧按 hash(key)%2 分流，节点0 本地 join 偶数半区，
 *   节点1 的奇数半区 probe 数据经 sender/receiver 送达节点0 验证数据面
 *   保真后与节点1 本地 join 结果合并，并集 = 全集串行基线。
 * - 故障注入：receiver 中途被杀，sender 快速失败（<10s）不挂死（依赖 A8）。
 *
 * A9：receiver 产出的块一律 px_wire_block_destroy（反序列化深销毁），
 *     本地 scan / join 输出块仍用 vector_block_destroy。
 */
#include <gtest/gtest.h>
#include <set>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "db/executor/exec_exchange.h"
#include "db/vectorized/vectorized.h"   /* vecx_pred_t/vecx_expr_t：exec_operators.h 前置依赖 */
#include "db/executor/exec_operators.h"
#include "db/executor/executor_framework.h"
#include "db/executor/px_wire.h"        /* A9: px_wire_block_destroy */
#include "db/distributed/rpc.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

/* ---------- 工具 ---------- */
static std::vector<void *> &dj_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

/* seqscan 原样保存列指针（见 seqscan_exec.c），列缓冲及
 * col_types/col_data/elem 三个参数数组都必须堆分配并活到 open() 之后，
 * 统一登记进 dj_buffers 由 TearDown 回收（镜像 exchange_net_test 的
 * make_scan_rows；brief 原稿用栈数组会悬空）。 */
static ExecNode *make_kv_scan(const int32_t *keys, const int32_t *vals, int n, int batch) {
    int32_t *k = (int32_t *)malloc(sizeof(int32_t) * n);
    int32_t *v = (int32_t *)malloc(sizeof(int32_t) * n);
    memcpy(k, keys, sizeof(int32_t) * n);
    memcpy(v, vals, sizeof(int32_t) * n);
    int *col_types = (int *)malloc(sizeof(int) * 2);
    col_types[0] = COLUMN_INT32;
    col_types[1] = COLUMN_INT32;
    void **col_data = (void **)malloc(sizeof(void *) * 2);
    col_data[0] = k;
    col_data[1] = v;
    int *elem = (int *)malloc(sizeof(int) * 2);
    elem[0] = (int)sizeof(int32_t);
    elem[1] = (int)sizeof(int32_t);
    dj_buffers().push_back(k);
    dj_buffers().push_back(v);
    dj_buffers().push_back(col_types);
    dj_buffers().push_back(col_data);
    dj_buffers().push_back(elem);
    return exec_create_seqscan(0, 2, col_types, col_data, elem, n, batch);
}

struct ScanFnCtx { const int32_t *k, *v; int n, batch; };
static ExecNode *scan_fn(void *p) {
    ScanFnCtx *c = (ScanFnCtx *)p;
    return make_kv_scan(c->k, c->v, c->n, c->batch);
}

using RowSet = std::multiset<std::pair<int32_t, int32_t>>;

/* 从 join 输出块收集 (build_val, probe_val) */
static void collect_join_rows(VectorBlock *out, RowSet &dst) {
    int32_t *bv = (int32_t *)out->columns[1];
    int32_t *pv = (int32_t *)out->columns[3];
    for (int r = 0; r < out->num_rows; r++) dst.insert({bv[r], pv[r]});
}

/* 串行基线：本地 vecx_hashjoin（本地产块 → vector_block_destroy） */
static RowSet baseline_join(const std::vector<int32_t> &bk, const std::vector<int32_t> &bv,
                            const std::vector<int32_t> &pk, const std::vector<int32_t> &pv) {
    RowSet result;
    vecx_hashjoin_t *hj = vecx_hashjoin_create(0, 0);
    ExecNode *bs = make_kv_scan(bk.data(), bv.data(), (int)bk.size(), (int)bk.size());
    bs->open(bs);
    VectorBlock *b;
    while ((b = bs->next(bs)) != nullptr) { vecx_hashjoin_add_build(hj, b); vector_block_destroy(b); }
    bs->close(bs); exec_destroy(bs);

    ExecNode *ps = make_kv_scan(pk.data(), pv.data(), (int)pk.size(), 512);
    ps->open(ps);
    while ((b = ps->next(ps)) != nullptr) {
        VectorBlock *out = nullptr;
        if (vecx_hashjoin_probe(hj, b, &out) > 0 && out) {
            collect_join_rows(out, result);
            vector_block_destroy(out);
        }
        vector_block_destroy(b);
    }
    ps->close(ps); exec_destroy(ps);
    vecx_hashjoin_destroy(hj);
    return result;
}

class DistributedJoinTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (void *p : dj_buffers()) free(p);
        dj_buffers().clear();
    }
};

/* ---------- BROADCAST：远端小表 → 本地建表 → 本地 probe ---------- */
TEST_F(DistributedJoinTest, BroadcastJoinTwoInstances) {
    /* 节点 B（:19701）：小表 100 行；节点 A（本测试线程）：大表 4000 行 */
    std::vector<int32_t> bk(100), bv(100);
    for (int i = 0; i < 100; i++) { bk[i] = i; bv[i] = i * 10; }
    std::vector<int32_t> pk(4000), pv(4000);
    for (int i = 0; i < 4000; i++) { pk[i] = i % 100; pv[i] = i; }

    rpc_node_address_t a_addr{};
    strcpy(a_addr.host, "127.0.0.1");
    a_addr.port = 19701;
    a_addr.node_id = 1;

    /* A：receiver 收小表 */
    ExecNode *recv = exec_create_exchange_receiver(&a_addr);
    ASSERT_NE(recv, nullptr);
    ASSERT_EQ(recv->open(recv), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* B：sender 发小表（模拟远端节点，独立线程） */
    ScanFnCtx bctx{bk.data(), bv.data(), 100, 64};
    std::thread node_b([&] {
        ExecNode *send = exec_create_exchange_sender(scan_fn, &bctx, &a_addr);
        if (!send) return;
        send->open(send);
        while (send->next(send) != nullptr) {}
        send->close(send);
        exec_destroy(send);
    });

    /* A：收全量小表 → 建表 → 本地 probe（A9：receiver 产块深销毁） */
    vecx_hashjoin_t *hj = vecx_hashjoin_create(0, 0);
    VectorBlock *b;
    while ((b = recv->next(recv)) != nullptr) {
        ASSERT_EQ(vecx_hashjoin_add_build(hj, b), 0);
        px_wire_block_destroy(b);
    }
    recv->close(recv);
    exec_destroy(recv);
    node_b.join();

    RowSet got;
    ExecNode *ps = make_kv_scan(pk.data(), pv.data(), 4000, 512);
    ps->open(ps);
    while ((b = ps->next(ps)) != nullptr) {
        VectorBlock *out = nullptr;
        if (vecx_hashjoin_probe(hj, b, &out) > 0 && out) {
            collect_join_rows(out, got);
            vector_block_destroy(out);
        }
        vector_block_destroy(b);
    }
    ps->close(ps);
    exec_destroy(ps);
    vecx_hashjoin_destroy(hj);

    RowSet want = baseline_join(bk, bv, pk, pv);
    EXPECT_EQ(got, want);
    EXPECT_EQ(got.size(), 4000u);
}

/* ---------- REPARTITION：双侧按 hash(key)%2 分流，各 join 一半 ---------- */
TEST_F(DistributedJoinTest, RepartitionJoinTwoInstances) {
    /* 全量数据：build 200 行、probe 4000 行，逻辑上平分在两节点 */
    std::vector<int32_t> bk(200), bv(200);
    for (int i = 0; i < 200; i++) { bk[i] = i; bv[i] = i * 10; }
    std::vector<int32_t> pk(4000), pv(4000);
    for (int i = 0; i < 4000; i++) { pk[i] = i % 200; pv[i] = i; }

    /* 按 key%2 切分：节点0 负责偶数 key，节点1 负责奇数 key */
    std::vector<int32_t> bk0, bv0, bk1, bv1, pk0, pv0, pk1, pv1;
    for (int i = 0; i < 200; i++) {
        if (bk[i] % 2 == 0) { bk0.push_back(bk[i]); bv0.push_back(bv[i]); }
        else                { bk1.push_back(bk[i]); bv1.push_back(bv[i]); }
    }
    for (int i = 0; i < 4000; i++) {
        if (pk[i] % 2 == 0) { pk0.push_back(pk[i]); pv0.push_back(pv[i]); }
        else                { pk1.push_back(pk[i]); pv1.push_back(pv[i]); }
    }

    /* "网络传输"：节点1 的奇数半区 probe 数据经 sender/receiver 送达节点0
       验证数据面（真实部署双方互发；本测试单向验证线格式+流通道承载
       repartition 半区，此为计划的显式设计）。 */
    rpc_node_address_t n0_addr{};
    strcpy(n0_addr.host, "127.0.0.1");
    n0_addr.port = 19702;
    n0_addr.node_id = 10;

    ExecNode *recv1 = exec_create_exchange_receiver(&n0_addr);
    ASSERT_NE(recv1, nullptr);
    ASSERT_EQ(recv1->open(recv1), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* 节点1 把奇数半区 probe 数据发给节点0（build 半区各自本地处理） */
    ScanFnCtx p1ctx{pk1.data(), pv1.data(), (int)pk1.size(), 512};
    std::thread node1([&] {
        ExecNode *send = exec_create_exchange_sender(scan_fn, &p1ctx, &n0_addr);
        if (!send) return;
        send->open(send);
        while (send->next(send) != nullptr) {}
        send->close(send);
        exec_destroy(send);
    });

    /* 节点0：本地 join 偶数半区 */
    RowSet got = baseline_join(bk0, bv0, pk0, pv0);

    /* 节点0 收取节点1 的奇数半区数据（验证 repartition 数据面保真） */
    std::vector<int32_t> rk, rv;
    VectorBlock *b;
    while ((b = recv1->next(recv1)) != nullptr) {
        int32_t *kc = (int32_t *)b->columns[0];
        int32_t *vc = (int32_t *)b->columns[1];
        for (int r = 0; r < b->num_rows; r++) { rk.push_back(kc[r]); rv.push_back(vc[r]); }
        px_wire_block_destroy(b);   /* A9：receiver 产块深销毁 */
    }
    recv1->close(recv1);
    exec_destroy(recv1);
    node1.join();

    /* 传输保真校验：收到的奇数半区与发送端一致 */
    ASSERT_EQ(rk.size(), pk1.size());
    for (size_t i = 0; i < rk.size(); i++) {
        EXPECT_EQ(rk[i], pk1[i]);
        EXPECT_EQ(rv[i], pv1[i]);
    }

    /* 节点1 本地 join 奇数半区，合并两节点结果 */
    RowSet got1 = baseline_join(bk1, bv1, pk1, pv1);
    got.insert(got1.begin(), got1.end());

    RowSet want = baseline_join(bk, bv, pk, pv);
    EXPECT_EQ(got, want);
}

/* ---------- 故障注入：receiver 被杀 → sender 报错且不挂死（依赖 A8） ---------- */
TEST_F(DistributedJoinTest, SenderFailsCleanlyWhenReceiverDies) {
    rpc_node_address_t addr{};
    strcpy(addr.host, "127.0.0.1");
    addr.port = 19703;
    addr.node_id = 11;

    ExecNode *recv = exec_create_exchange_receiver(&addr);
    ASSERT_NE(recv, nullptr);
    ASSERT_EQ(recv->open(recv), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<int32_t> k(100000), v(100000);
    for (int i = 0; i < 100000; i++) { k[i] = i; v[i] = i; }
    ScanFnCtx ctx{k.data(), v.data(), 100000, 4096};

    ExecNode *send = exec_create_exchange_sender(scan_fn, &ctx, &addr);
    ASSERT_NE(send, nullptr);
    ASSERT_EQ(send->open(send), 0);

    /* 发一部分后杀掉 receiver；A8 保证 listener_stop 唤醒阻塞在
     * serve_conn->recv_all 的 accept 线程，pthread_join 不死锁 */
    send->next(send);
    recv->close(recv);
    exec_destroy(recv);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto t0 = std::chrono::steady_clock::now();
    while (send->next(send) != nullptr) {}   /* 应快速失败，不挂死 */
    auto secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(secs, 10.0);
    send->close(send);                        /* 资源回收不崩溃 */
    exec_destroy(send);
}
