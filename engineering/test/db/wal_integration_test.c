/**
 * @file wal_integration_test.c
 * @brief WAL 集成测试
 *
 * 测试用例:
 * 1. test_wal_fsync     - 验证 fsync 正确性
 * 2. test_wal_recovery  - 验证 WAL 恢复能力
 * 3. test_wal_failure   - 验证 WAL 失败处理
 */

#include "db/wal.h"
#include "db/disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TEST_WAL_PATH "test_wal_integration.db"
#define TEST_PAGE_SIZE 8192

/* ============================================================
 * 辅助宏
 * ============================================================ */
#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (条件: %s)\n", msg, #cond); \
        return 1; \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("  [FAIL] %s (期望=%d, 实际=%d)\n", msg, (int)(b), (int)(a)); \
        return 1; \
    } \
} while(0)

#define ASSERT_NE(a, b, msg) do { \
    if ((a) == (b)) { \
        printf("  [FAIL] %s (期望 != %d, 实际=%d)\n", msg, (int)(b), (int)(a)); \
        return 1; \
    } \
} while(0)

/* ============================================================
 * 回调上下文
 * ============================================================ */
typedef struct {
    int count;
    wal_log_type_t types[100];
    uint32_t key_lens[100];
    uint32_t value_lens[100];
} replay_ctx_t;

static int replay_callback(void *ctx, wal_log_type_t type,
                           const void *key, size_t key_len,
                           const void *value, size_t value_len) {
    replay_ctx_t *rctx = (replay_ctx_t *)ctx;
    int i = rctx->count++;
    if (i < 100) {
        rctx->types[i] = type;
        rctx->key_lens[i] = (uint32_t)key_len;
        rctx->value_lens[i] = (uint32_t)value_len;
    }
    (void)key; (void)value;
    return 0;
}

/* ============================================================
 * 测试用例 1: test_wal_fsync
 * 验证 fsync 正确性：写入后刷盘，重新打开验证数据不丢失
 * ============================================================ */
static int test_wal_fsync(void) {
    printf("=== test_wal_fsync: 验证 fsync 正确性 ===\n");

    /* 清理旧文件 */
    remove(TEST_WAL_PATH);

    /* 创建 WAL */
    wal_t *wal = wal_create(TEST_WAL_PATH, TEST_PAGE_SIZE);
    ASSERT_TRUE(wal != NULL, "创建 WAL 成功");

    /* 写入事务 */
    uint64_t lsn_begin = wal_write_begin(wal, 100);
    ASSERT_TRUE(lsn_begin > 0, "写入 BEGIN 成功");

    uint64_t lsn_insert = wal_write_insert(wal, 100, "key1", 4, "value1", 6);
    ASSERT_TRUE(lsn_insert > 0, "写入 INSERT 成功");

    uint64_t lsn_commit = wal_write_commit(wal, 100);
    ASSERT_TRUE(lsn_commit > 0, "写入 COMMIT 成功");

    /* 刷盘（fsync） */
    int rc = wal_flush(wal);
    ASSERT_EQ(rc, 0, "wal_flush 成功");

    /* 关闭 WAL */
    rc = wal_close(wal);
    ASSERT_EQ(rc, 0, "wal_close 成功");

    /* 重新打开验证 */
    wal_t *wal2 = wal_open(TEST_WAL_PATH);
    ASSERT_TRUE(wal2 != NULL, "重新打开 WAL 成功");

    /* 验证 LSN 已持久化 */
    uint64_t lsn = wal_get_lsn(wal2);
    ASSERT_TRUE(lsn >= 3, "LSN 已持久化 (>= 3)");

    /* 获取统计信息 */
    wal_stats_t stats;
    rc = wal_get_stats(wal2, &stats);
    ASSERT_EQ(rc, 0, "获取统计信息成功");
    ASSERT_TRUE(stats.total_records >= 3, "记录数 >= 3");

    wal_close(wal2);

    /* 清理 */
    remove(TEST_WAL_PATH);

    printf("  [PASS] fsync 正确性验证通过\n");
    return 0;
}

/* ============================================================
 * 测试用例 2: test_wal_recovery
 * 验证 WAL 恢复能力：写入数据后关闭，打开后能恢复到正确状态
 * ============================================================ */
static int test_wal_recovery(void) {
    printf("=== test_wal_recovery: 验证 WAL 恢复能力 ===\n");

    remove(TEST_WAL_PATH);

    /* Phase 1: 写入并提交事务 */
    wal_t *wal = wal_create(TEST_WAL_PATH, TEST_PAGE_SIZE);
    ASSERT_TRUE(wal != NULL, "创建 WAL 成功");

    /* 事务 1: 完整提交 */
    wal_write_begin(wal, 1);
    wal_write_insert(wal, 1, "k1", 2, "v1", 2);
    wal_write_insert(wal, 1, "k2", 2, "v2", 2);
    wal_write_commit(wal, 1);

    /* 事务 2: 完整提交 */
    wal_write_begin(wal, 2);
    wal_write_update(wal, 2, "k1", 2, "v1", 2, "v1_new", 6);
    wal_write_commit(wal, 2);

    /* 事务 3: 未提交（模拟崩溃） */
    wal_write_begin(wal, 3);
    wal_write_insert(wal, 3, "k3", 2, "v3", 2);
    /* 不提交，模拟崩溃 */

    /* 写入检查点 */
    uint32_t dirty_pages[] = {1, 2};
    wal_write_checkpoint(wal, dirty_pages, 2);

    wal_close(wal);

    /* Phase 2: 重新打开并恢复 */
    wal_t *wal2 = wal_open(TEST_WAL_PATH);
    ASSERT_TRUE(wal2 != NULL, "重新打开 WAL 成功");

    /* 验证 LSN */
    uint64_t lsn = wal_get_lsn(wal2);
    ASSERT_TRUE(lsn >= 5, "恢复后 LSN 正确 (>= 5)");

    /* 分析 WAL */
    wal_recovery_info_t info;
    int rc = wal_analyze(TEST_WAL_PATH, &info);
    ASSERT_EQ(rc, 0, "分析 WAL 成功");

    /* 验证有活动事务（事务 3 未提交） */
    ASSERT_TRUE(info.active_txn_count >= 1, "检测到未提交事务");

    wal_recovery_info_free(&info);
    wal_close(wal2);

    /* Phase 3: 验证 redo */
    replay_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    rc = wal_redo(TEST_WAL_PATH, 0, replay_callback, &ctx);
    ASSERT_EQ(rc, 0, "redo 执行成功");
    ASSERT_TRUE(ctx.count >= 1, "redo 至少重放一条记录");

    /* 清理 */
    remove(TEST_WAL_PATH);

    printf("  [PASS] WAL 恢复能力验证通过\n");
    return 0;
}

/* ============================================================
 * 测试用例 3: test_wal_failure
 * 验证 WAL 失败处理：空路径、无效参数等
 * ============================================================ */
static int test_wal_failure(void) {
    printf("=== test_wal_failure: 验证 WAL 失败处理 ===\n");

    /* 测试 NULL 参数 */
    wal_t *wal = wal_create(NULL, TEST_PAGE_SIZE);
    /* NULL 路径应使用默认路径，不崩溃 */
    if (wal != NULL) {
        wal_close(wal);
        remove("wal.db");
    }

    /* 测试打开不存在的文件 */
    wal_t *wal2 = wal_open("nonexistent_wal_file.db");
    ASSERT_TRUE(wal2 != NULL, "打开不存在的文件（会初始化）");
    wal_close(wal2);
    remove("nonexistent_wal_file.db");

    /* 测试无效 flush */
    wal_t *wal3 = wal_create(TEST_WAL_PATH, TEST_PAGE_SIZE);
    ASSERT_TRUE(wal3 != NULL, "创建 WAL 成功");
    int rc = wal_flush(wal3);
    ASSERT_EQ(rc, 0, "空 flush 成功");

    /* 测试无效事务操作 */
    uint64_t lsn = wal_write_begin(NULL, 1);
    ASSERT_EQ(lsn, 0, "NULL wal 返回 0");

    lsn = wal_write_insert(NULL, 1, "k", 1, "v", 1);
    ASSERT_EQ(lsn, 0, "NULL wal 写入返回 0");

    lsn = wal_write_commit(NULL, 1);
    ASSERT_EQ(lsn, 0, "NULL wal 提交返回 0");

    wal_close(wal3);
    remove(TEST_WAL_PATH);

    /* 测试 wal_need_checkpoint */
    wal_t *wal4 = wal_create(TEST_WAL_PATH, TEST_PAGE_SIZE);
    ASSERT_TRUE(wal4 != NULL, "创建 WAL 成功");
    bool need = wal_need_checkpoint(wal4);
    ASSERT_TRUE(!need, "初始不需要检查点");

    /* 写大量记录触发检查点 */
    for (int i = 0; i < 1001; i++) {
        wal_write_insert(wal4, 1, "k", 1, "v", 1);
    }
    need = wal_need_checkpoint(wal4);
    ASSERT_TRUE(need, "大量写入后需要检查点");

    wal_close(wal4);
    remove(TEST_WAL_PATH);

    /* 测试错误信息 */
    wal_t *wal5 = wal_create(TEST_WAL_PATH, TEST_PAGE_SIZE);
    ASSERT_TRUE(wal5 != NULL, "创建 WAL 成功");
    const char *errmsg = wal_errmsg(NULL);
    ASSERT_TRUE(errmsg != NULL, "NULL WAL 错误信息非空");
    wal_close(wal5);
    remove(TEST_WAL_PATH);

    printf("  [PASS] WAL 失败处理验证通过\n");
    return 0;
}

/* ============================================================
 * 测试用例 4: test_wal_transaction
 * 验证完整事务生命周期
 * ============================================================ */
static int test_wal_transaction(void) {
    printf("=== test_wal_transaction: 验证完整事务生命周期 ===\n");

    remove(TEST_WAL_PATH);

    wal_t *wal = wal_create(TEST_WAL_PATH, TEST_PAGE_SIZE);
    ASSERT_TRUE(wal != NULL, "创建 WAL 成功");

    /* 事务 1: 正常提交 */
    uint64_t lsn1 = wal_write_begin(wal, 1);
    ASSERT_TRUE(lsn1 > 0, "BEGIN 成功");
    wal_write_insert(wal, 1, "user1", 5, "Alice", 5);
    wal_write_insert(wal, 1, "user2", 5, "Bob", 3);
    uint64_t lsn_commit1 = wal_write_commit(wal, 1);
    ASSERT_TRUE(lsn_commit1 > 0, "COMMIT 成功");

    /* 事务 2: 回滚 */
    uint64_t lsn2 = wal_write_begin(wal, 2);
    ASSERT_TRUE(lsn2 > 0, "BEGIN 成功");
    wal_write_insert(wal, 2, "temp", 4, "data", 4);
    uint64_t lsn_abort = wal_write_abort(wal, 2);
    ASSERT_TRUE(lsn_abort > 0, "ABORT 成功");

    /* 事务 3: 更新操作 */
    wal_write_begin(wal, 3);
    wal_write_update(wal, 3, "user1", 5, "Alice", 5, "Alice_New", 9);
    wal_write_commit(wal, 3);

    /* 刷盘 */
    wal_flush(wal);

    /* 检查点 */
    uint32_t pages[] = {1, 2, 3};
    wal_write_checkpoint(wal, pages, 3);

    wal_stats_t stats;
    wal_get_stats(wal, &stats);
    ASSERT_TRUE(stats.total_records >= 5, "记录数正确");
    ASSERT_TRUE(stats.checkpoint_lsn > 0, "检查点 LSN 已记录");

    wal_close(wal);

    /* 重新打开验证 */
    wal_t *wal2 = wal_open(TEST_WAL_PATH);
    ASSERT_TRUE(wal2 != NULL, "重新打开成功");

    uint64_t lsn = wal_get_lsn(wal2);
    ASSERT_TRUE(lsn >= stats.total_records, "LSN 持久化正确");

    wal_close(wal2);
    remove(TEST_WAL_PATH);

    printf("  [PASS] 完整事务生命周期验证通过\n");
    return 0;
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("WAL 集成测试\n");
    printf("========================================\n\n");

    int failed = 0;

    failed += test_wal_fsync();
    printf("\n");

    failed += test_wal_recovery();
    printf("\n");

    failed += test_wal_failure();
    printf("\n");

    failed += test_wal_transaction();
    printf("\n");

    printf("========================================\n");
    if (failed == 0) {
        printf("所有测试通过 (PASS)\n");
    } else {
        printf("失败用例数: %d\n", failed);
    }
    printf("========================================\n");

    return failed;
}
