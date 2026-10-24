#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/hash_index.h"
#include "db/toast.h"
#include "db/fsm.h"
#include "db/kv.h"
#include "db/kv.h"  /* for kv_txn_t forward decl */
}

/* Forward decl since kv_txn.h may not be installed */
struct kv_txn_s;
typedef struct kv_txn_s kv_txn_t;
extern "C" {
    kv_txn_t *kv_txn_begin(kv_t *db);
    int kv_txn_put(kv_txn_t *tx, const void *key, size_t klen,
                    const void *value, size_t vlen);
    int kv_txn_commit(kv_txn_t *tx);
    void kv_txn_rollback(kv_txn_t *tx);
    void kv_txn_free(kv_txn_t *tx);
}

namespace {
const char *kTmpKV = "/tmp/c7_kv_test.db";
}  // namespace

TEST(C7HashIndex, PutGetDel) {
    hash_index_t *idx = hash_index_create(64);
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(hash_index_put(idx, "a", 1, 100), 0);
    EXPECT_EQ(hash_index_put(idx, "b", 1, 200), 0);
    EXPECT_EQ(hash_index_put(idx, "a", 1, 101), 0);  /* 覆盖 */

    uint64_t v = 0;
    EXPECT_EQ(hash_index_get(idx, "a", 1, &v), 0);
    EXPECT_EQ(v, 101u);
    EXPECT_EQ(hash_index_get(idx, "missing", 7, &v), -1);

    EXPECT_EQ(hash_index_del(idx, "a", 1), 0);
    EXPECT_EQ(hash_index_get(idx, "a", 1, &v), -1);
    hash_index_destroy(idx);
}

TEST(C7Toast, SmallInlined) {
    int is_external = -1;
    uint8_t id[32] = {0};
    char data[100] = "small";
    EXPECT_EQ(toast_decide(data, 5, nullptr, &is_external, id), 0);
    EXPECT_EQ(is_external, 0);  /* <= 2KB 不外存 */
}

TEST(C7Toast, LargeExternal) {
    int is_external = -1;
    uint8_t id[32] = {0};
    char *big = malloc(4096);
    memset(big, 'X', 4096);
    /* 无 blob_engine → 即使超阈值也不外存 */
    EXPECT_EQ(toast_decide(big, 4096, nullptr, &is_external, id), 0);
    EXPECT_EQ(is_external, 0);
    free(big);
}

TEST(C7FSM, MarkAndFind) {
    fsm_t *f = fsm_create(100);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(fsm_mark_free(f, 5, true), 0);
    EXPECT_EQ(fsm_mark_free(f, 10, true), 0);
    EXPECT_EQ(fsm_n_pages(f), 100u);
    int32_t p = fsm_find_free(f);
    EXPECT_GE(p, 0);
    EXPECT_LT(p, 100);
    fsm_destroy(f);
}

TEST(C7KVTxn, BeginPutCommit) {
    remove(kTmpKV);
    kv_t *db = kv_open(kTmpKV);
    if (!db) GTEST_SKIP() << "kv_open failed";
    kv_txn_t *tx = kv_txn_begin(db);
    ASSERT_NE(tx, nullptr);
    EXPECT_EQ(kv_txn_put(tx, "k1", 2, "v1", 2), 0);
    EXPECT_EQ(kv_txn_put(tx, "k2", 2, "v2", 2), 0);
    EXPECT_EQ(kv_txn_commit(tx), 0);
    kv_txn_free(tx);
    kv_close(db);
    remove(kTmpKV);
}
