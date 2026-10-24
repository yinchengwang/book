#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <map>
#include <vector>

extern "C" {
#include "db/executor/exec_shard.h"
#include "db/executor/executor_framework.h"
#include "db/executor/exec_operators.h"
#include "db/sharding/sharding.h"
#include "db/vectorized/vectorized.h"
#include "db/core/vector_types.h"
#include "db/core/columnar_store.h"
}

/* RANGE 分片 4 个：[0,250) [250,500) [500,750) [750,1000) */
static shard_router_t *make_router() {
    shard_config_t *cfg = shard_config_create(SHARD_RANGE, 4);
    cfg->key_type = SHARD_KEY_INT;
    shard_router_t *r = shard_router_create(cfg);
    shard_config_destroy(cfg);
    for (int i = 0; i < 4; i++) {
        shard_info_t *info = shard_info_create(i, "s");
        info->min_value = i * 250;
        info->max_value = (i + 1) * 250 - 1;
        info->host = strdup("127.0.0.1");
        info->port = 9000 + i;
        shard_router_add(r, info);   /* router 内部深拷贝 */
        shard_info_destroy(info);
    }
    return r;
}

TEST(ShardPrune, EqualPredicateRoutesSingleShard) {
    shard_router_t *r = make_router();
    vecx_pred_t pred{};
    pred.op = CMP_EQ;
    pred.i64 = 300;                       /* 落在 [250,500) → shard 1 */
    int ids[4];
    int n = px_shard_prune(r, &pred, ids, 4);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(ids[0], 1);
    shard_router_destroy(r);
}

TEST(ShardPrune, EqualOutOfDomainYieldsZeroShards) {
    shard_router_t *r = make_router();
    vecx_pred_t pred{};
    pred.op = CMP_EQ;
    pred.i64 = 99999;                     /* 超出全域 [0,1000)：不得回退 shard 0 */
    int ids[4];
    int n = px_shard_prune(r, &pred, ids, 4);
    EXPECT_EQ(n, 0);
    shard_router_destroy(r);
}

TEST(ShardPrune, RangePredicatePrunesUnrelated) {
    shard_router_t *r = make_router();
    vecx_pred_t pred{};
    pred.op = CMP_LT;
    pred.i64 = 500;                       /* 只需 shard 0,1 */
    int ids[4];
    int n = px_shard_prune(r, &pred, ids, 4);
    EXPECT_EQ(n, 2);
    std::set<int> got(ids, ids + n);
    EXPECT_EQ(got, (std::set<int>{0, 1}));
    shard_router_destroy(r);
}

TEST(ShardPrune, NoPredicateFansOutAll) {
    shard_router_t *r = make_router();
    int ids[4];
    int n = px_shard_prune(r, nullptr, ids, 4);
    EXPECT_EQ(n, 4);
    shard_router_destroy(r);
}

/* ---- 扇出集成：每分片一个内存表，裁剪后只读入选分片 ---- */
struct FakeShardDb {
    std::map<int, std::vector<int32_t>> rows;   /* shard_id → 数据 */
    std::set<int> opened_shards;                 /* 记录实际被打开的 */
};

/* exec_create_seqscan 只保存 col_types/col_data/col_elem_size 指针不拷贝
 * （seqscan_exec.c:88-90），栈数组会在工厂返回后悬挂——堆分配并登记，
 * 测试结束时集中回收。rows.data() 指针稳定（std::map 节点存储，
 * 4 个 key 在扇出创建前已全部插入，不会重哈希/迁移）。 */
static std::vector<void *> &test_buffers() {
    static std::vector<void *> bufs;
    return bufs;
}

static void free_test_buffers() {
    for (void *p : test_buffers()) free(p);
    test_buffers().clear();
}

static ExecNode *open_fake_shard(int shard_id, void *vctx) {
    FakeShardDb *db = (FakeShardDb *)vctx;
    db->opened_shards.insert(shard_id);
    std::vector<int32_t> &rows = db->rows[shard_id];
    int *col_types = (int *)malloc(sizeof(int));
    col_types[0] = COLUMN_INT32;
    void **col_data = (void **)malloc(sizeof(void *));
    col_data[0] = rows.data();
    int *elem = (int *)malloc(sizeof(int));
    elem[0] = (int)sizeof(int32_t);
    test_buffers().push_back(col_types);
    test_buffers().push_back(col_data);
    test_buffers().push_back(elem);
    return exec_create_seqscan(0, 1, col_types, col_data, elem,
                               (int64_t)rows.size(), 256);
}

TEST(ShardPrune, FanoutReadsOnlySelectedShards) {
    shard_router_t *r = make_router();
    FakeShardDb db;
    for (int s = 0; s < 4; s++)
        for (int i = 0; i < 100; i++)
            db.rows[s].push_back(s * 250 + i);   /* shard s 存 [s*250, s*250+99] */

    vecx_pred_t pred{};
    pred.op = CMP_LT;
    pred.i64 = 500;

    ExecNode *fan = exec_create_shard_fanout(r, &pred, open_fake_shard, &db);
    ASSERT_NE(fan, nullptr);
    ASSERT_EQ(fan->open(fan), 0);

    std::multiset<int> got;
    VectorBlock *b;
    while ((b = fan->next(fan)) != nullptr) {
        int32_t *col = (int32_t *)b->columns[0];
        for (int i = 0; i < b->num_rows; i++) got.insert(col[i]);
        vector_block_destroy(b);
    }
    fan->close(fan);
    exec_destroy(fan);

    EXPECT_EQ(db.opened_shards, (std::set<int>{0, 1}));   /* 裁剪生效 */
    EXPECT_EQ(got.size(), 200u);                            /* 两分片全量 */
    shard_router_destroy(r);
    free_test_buffers();
}
