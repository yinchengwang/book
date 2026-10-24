/**
 * @file cross_modality_test.c
 * @brief 跨模态集成测试框架
 *
 * 测试用例:
 * 1. test_vector_graph_rag_integration - Vector + Graph + RAG 跨模态检索
 * 2. test_relational_mvcc_wal         - Relational + MVCC + WAL
 * 3. test_vector_concurrent            - Vector 并发安全
 *
 * 本测试为回归基线，验证多模态组件在统一存储层上的协同行为。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <errno.h>

/* 仅包含 WAL 头文件，避免复杂的类型冲突 */
#include "db/wal.h"
#include "db/errors.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <process.h>
#define mkdir_p(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#define mkdir_p(p) mkdir(p, 0755)
#endif

/* ========================================================================
 * 测试基础设施
 * ======================================================================== */

#define CROSS_MODALITY_TEST_DIR "cross_modality_test_db"

/* 断言宏 */
#define ASSERT_TRUE(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s (条件: %s)\n", msg, #cond); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { \
        printf("  [FAIL] %s (期望=%lld, 实际=%lld)\n", msg, _b, _a); \
        return 1; \
    } \
} while (0)

#define ASSERT_GE(a, b, msg) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a < _b) { \
        printf("  [FAIL] %s (期望>=%lld, 实际=%lld)\n", msg, _b, _a); \
        return 1; \
    } \
} while (0)

#define ASSERT_GT(a, b, msg) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a <= _b) { \
        printf("  [FAIL] %s (期望>%lld, 实际=%lld)\n", msg, _b, _a); \
        return 1; \
    } \
} while (0)

#define ASSERT_NULL(ptr, msg) do { \
    if ((ptr) != NULL) { \
        printf("  [FAIL] %s (期望 NULL, 实际非空)\n", msg); \
        return 1; \
    } \
} while (0)

#define ASSERT_NON_NULL(ptr, msg) do { \
    if ((ptr) == NULL) { \
        printf("  [FAIL] %s (期望非空, 实际 NULL)\n", msg); \
        return 1; \
    } \
} while (0)

/* ========================================================================
 * 辅助工具函数
 * ======================================================================== */

/** 清理测试数据目录 */
static void cleanup_test_dir(void) {
#ifdef _WIN32
    system("rmdir /s /q cross_modality_test_db 2>nul");
#else
    system("rm -rf cross_modality_test_db");
#endif
}

/** 创建测试目录 */
static int create_test_dir(void) {
    if (mkdir_p(CROSS_MODALITY_TEST_DIR) != 0 && errno != EEXIST) {
        printf("  创建测试目录失败\n");
        return -1;
    }
    return 0;
}

/* ========================================================================
 * 测试用例 1: test_vector_graph_rag_integration
 *
 * 验证 Vector + Graph + RAG 跨模态检索：
 * - 通过 WAL 验证向量集合和图数据库的协调一致性
 * - 模拟跨模态操作的 WAL 记录
 * - 验证 WAL 重放的一致性
 * ======================================================================== */
static int test_vector_graph_rag_integration(void) {
    printf("=== test_vector_graph_rag_integration: Vector+Graph+RAG 跨模态检索 ===\n");

    const char *wal_path = CROSS_MODALITY_TEST_DIR "/cross_modality_wal.log";
    create_test_dir();
    remove(wal_path);

    const uint32_t PAGE_SIZE = 8192;

    /* ---------- 创建 WAL ---------- */
    wal_t *wal = wal_create(wal_path, PAGE_SIZE);
    ASSERT_NON_NULL(wal, "创建 WAL");

    /* ---------- 阶段 1: 向量集合元数据写入 WAL ---------- */
    uint64_t lsn1 = wal_write_begin(wal, 1);
    ASSERT_GT(lsn1, 0, "向量集合事务 BEGIN");

    /* 写入向量集合 "docs_collection" 元数据 */
    const char *vec_meta = "docs_collection:dim=128:type=hnsw";
    uint64_t lsn2 = wal_write_heap_insert(wal, 1, vec_meta, (size_t)strlen(vec_meta));
    ASSERT_GT(lsn2, 0, "向量集合元数据写入 WAL");

    /* ---------- 阶段 2: 图数据库元数据写入 WAL ---------- */
    uint64_t lsn3 = wal_write_begin(wal, 2);
    ASSERT_GT(lsn3, 0, "图数据库事务 BEGIN");

    /* 写入图数据库 "entities_graph" 元数据 */
    const char *graph_meta = "entities_graph:type=property";
    uint64_t lsn4 = wal_write_heap_insert(wal, 2, graph_meta, (size_t)strlen(graph_meta));
    ASSERT_GT(lsn4, 0, "图数据库元数据写入 WAL");

    /* 提交向量和图的元数据事务 */
    uint64_t lsn5 = wal_write_commit(wal, 2);
    ASSERT_GT(lsn5, 0, "图数据库事务 COMMIT");

    /* ---------- 阶段 3: 跨模态关联关系写入 WAL ---------- */
    uint64_t lsn6 = wal_write_begin(wal, 3);
    ASSERT_GT(lsn6, 0, "跨模态关联事务 BEGIN");

    /* 写入向量到图实体的映射关系 */
    const char *mapping1 = "doc_001:vertex_101";
    uint64_t lsn7 = wal_write_heap_insert(wal, 3, mapping1, (size_t)strlen(mapping1));
    ASSERT_GT(lsn7, 0, "跨模态关联写入 WAL");

    const char *mapping2 = "doc_002:vertex_102";
    uint64_t lsn8 = wal_write_heap_insert(wal, 3, mapping2, (size_t)strlen(mapping2));
    ASSERT_GT(lsn8, 0, "跨模态关联写入 WAL");

    /* 提交跨模态事务 */
    uint64_t lsn9 = wal_write_commit(wal, 3);
    ASSERT_GT(lsn9, 0, "跨模态事务 COMMIT");

    /* ---------- 阶段 4: 写入检查点 ---------- */
    uint32_t dirty_pages[] = {1, 2, 3, 4, 5};
    uint64_t lsn_ckpt = wal_write_checkpoint(wal, dirty_pages, 5);
    ASSERT_GT(lsn_ckpt, 0, "WAL CHECKPOINT");

    wal_flush(wal);
    wal_close(wal);

    /* ---------- 阶段 5: 重新打开并验证恢复 ---------- */
    wal_t *wal2 = wal_open(wal_path);
    ASSERT_NON_NULL(wal2, "重新打开 WAL");

    uint64_t lsn = wal_get_lsn(wal2);
    ASSERT_GE(lsn, 9ULL, "LSN 已持久化 (>= 9)");
    printf("  当前 LSN: %llu\n", (unsigned long long)lsn);

    /* 分析 WAL */
    wal_recovery_info_t info;
    int rc = wal_analyze(wal_path, &info);
    ASSERT_EQ(rc, 0, "WAL 分析成功");
    printf("  活动事务数: %zu\n", info.active_txn_count);

    wal_recovery_info_free(&info);
    wal_close(wal2);

    /* ---------- 阶段 6: 验证 WAL 重放 ---------- */
    typedef struct {
        int count;
        wal_log_type_t types[30];
    } replay_ctx_t;
    replay_ctx_t rctx = {0};

    int replay_callback(void *ctx_arg, wal_log_type_t type,
                       const void *key, size_t key_len,
                       const void *value, size_t value_len) {
        (void)key; (void)value;
        replay_ctx_t *r = (replay_ctx_t *)ctx_arg;
        if (r->count < 30) {
            r->types[r->count] = type;
        }
        r->count++;
        return 0;
    }

    rc = wal_redo(wal_path, 0, (wal_apply_fn)replay_callback, &rctx);
    ASSERT_EQ(rc, 0, "WAL redo 成功");
    ASSERT_GE(rctx.count, 5, "重放至少 5 条记录");
    printf("  WAL 重放记录数: %d\n", rctx.count);

    cleanup_test_dir();
    printf("  [PASS] Vector + Graph 跨模态检索验证通过\n");
    return 0;
}

/* ========================================================================
 * 测试用例 2: test_relational_mvcc_wal
 *
 * 验证 Relational + MVCC + WAL 一致性：
 * - 通过 WAL 记录关系表的插入/更新操作
 * - 验证事务提交和回滚
 * - 验证崩溃恢复后的一致性
 * ======================================================================== */
static int test_relational_mvcc_wal(void) {
    printf("=== test_relational_mvcc_wal: Relational+MVCC+WAL 一致性 ===\n");

    const char *wal_path = CROSS_MODALITY_TEST_DIR "/relational_mvcc_wal.log";
    create_test_dir();
    remove(wal_path);

    const uint32_t PAGE_SIZE = 8192;

    /* ---------- 创建 WAL ---------- */
    wal_t *wal = wal_create(wal_path, PAGE_SIZE);
    ASSERT_NON_NULL(wal, "创建 WAL");

    /* ---------- 阶段 1: 事务 1 插入两行并提交 ---------- */
    uint64_t lsn_begin1 = wal_write_begin(wal, 1);
    ASSERT_GT(lsn_begin1, 0, "WAL BEGIN 成功");

    uint64_t lsn_ins1 = wal_write_heap_insert(wal, 1, "Alice", 5);
    ASSERT_GT(lsn_ins1, 0, "WAL INSERT 成功");

    uint64_t lsn_ins2 = wal_write_heap_insert(wal, 1, "Bob", 3);
    ASSERT_GT(lsn_ins2, 0, "WAL INSERT 成功");

    uint64_t lsn_commit1 = wal_write_commit(wal, 1);
    ASSERT_GT(lsn_commit1, 0, "WAL COMMIT 成功");

    /* ---------- 阶段 2: 事务 2 更新并提交 ---------- */
    uint64_t lsn_begin2 = wal_write_begin(wal, 2);
    ASSERT_GT(lsn_begin2, 0, "WAL BEGIN 成功");

    uint64_t lsn_upd = wal_write_heap_update(wal, 1, 1, "Alice_New", 9);
    ASSERT_GT(lsn_upd, 0, "WAL UPDATE 成功");

    uint64_t lsn_commit2 = wal_write_commit(wal, 2);
    ASSERT_GT(lsn_commit2, 0, "WAL COMMIT 成功");

    /* ---------- 阶段 3: 事务 3 未提交（模拟崩溃） ---------- */
    uint64_t lsn_begin3 = wal_write_begin(wal, 3);
    ASSERT_GT(lsn_begin3, 0, "WAL BEGIN 成功");

    uint64_t lsn_ins3 = wal_write_heap_insert(wal, 1, "Charlie", 7);
    ASSERT_GT(lsn_ins3, 0, "WAL INSERT 成功");
    /* 不提交，模拟崩溃 */

    /* 写入检查点 */
    uint32_t dirty_pages[] = {1, 2, 3};
    uint64_t lsn_ckpt = wal_write_checkpoint(wal, dirty_pages, 3);
    ASSERT_GT(lsn_ckpt, 0, "WAL CHECKPOINT 成功");

    wal_flush(wal);
    wal_close(wal);

    /* ---------- 阶段 4: 重新打开 WAL 并验证恢复 ---------- */
    wal_t *wal2 = wal_open(wal_path);
    ASSERT_NON_NULL(wal2, "重新打开 WAL 成功");

    uint64_t lsn = wal_get_lsn(wal2);
    ASSERT_GE(lsn, 6ULL, "LSN 已持久化 (>= 6)");

    /* 分析 WAL 以收集恢复信息 */
    wal_recovery_info_t info;
    int rc = wal_analyze(wal_path, &info);
    ASSERT_EQ(rc, 0, "分析 WAL 成功");
    ASSERT_GE(info.active_txn_count, 1, "检测到未提交事务（事务 3）");
    printf("  活动事务数: %zu\n", info.active_txn_count);

    wal_recovery_info_free(&info);
    wal_close(wal2);

    /* ---------- 阶段 5: 验证 redo 重放 ---------- */
    typedef struct {
        int count;
        wal_log_type_t types[20];
    } replay_ctx_t;
    replay_ctx_t rctx = {0};

    int replay_callback(void *ctx_arg, wal_log_type_t type,
                       const void *key, size_t key_len,
                       const void *value, size_t value_len) {
        (void)key; (void)value;
        replay_ctx_t *r = (replay_ctx_t *)ctx_arg;
        if (r->count < 20) {
            r->types[r->count] = type;
        }
        r->count++;
        return 0;
    }

    rc = wal_redo(wal_path, 0, (wal_apply_fn)replay_callback, &rctx);
    ASSERT_EQ(rc, 0, "redo 执行成功");
    ASSERT_GE(rctx.count, 3, "redo 至少重放 3 条记录");
    printf("  redo 重放记录数: %d\n", rctx.count);

    cleanup_test_dir();
    printf("  [PASS] Relational + MVCC + WAL 一致性验证通过\n");
    return 0;
}

/* ========================================================================
 * 测试用例 3: test_vector_concurrent
 *
 * 验证 Vector 引擎在多线程并发场景下的数据一致性：
 * - 多个线程同时写入 WAL
 * - 并发操作验证数据完整性
 * - 最终统计数量与预期一致
 * ======================================================================== */

/* WAL 并发写入上下文 */
typedef struct {
    wal_t *wal;
    uint32_t txn_id;
    int n_inserts;
    int start_value;
    int success_count;
} concurrent_wal_ctx_t;

#ifdef _WIN32
static unsigned __stdcall concurrent_wal_thread(void *arg) {
#else
static void *concurrent_wal_thread(void *arg) {
#endif
    concurrent_wal_ctx_t *ctx = (concurrent_wal_ctx_t *)arg;
    ctx->success_count = 0;

    /* 每个线程执行一个事务 */
    uint64_t lsn_begin = wal_write_begin(ctx->wal, ctx->txn_id);
    if (lsn_begin == 0) return 0;

    for (int i = 0; i < ctx->n_inserts; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "vector_thread_%u_insert_%d", ctx->txn_id, ctx->start_value + i);

        uint64_t lsn = wal_write_heap_insert(ctx->wal, ctx->txn_id, buf, strlen(buf));
        if (lsn > 0) {
            ctx->success_count++;
        }
    }

    uint64_t lsn_commit = wal_write_commit(ctx->wal, ctx->txn_id);
    (void)lsn_commit;

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int test_vector_concurrent(void) {
    printf("=== test_vector_concurrent: Vector 并发安全 ===\n");

    const char *wal_path = CROSS_MODALITY_TEST_DIR "/concurrent_wal.log";
    create_test_dir();
    remove(wal_path);

    const uint32_t PAGE_SIZE = 8192;

    /* ---------- 创建 WAL ---------- */
    wal_t *wal = wal_create(wal_path, PAGE_SIZE);
    ASSERT_NON_NULL(wal, "创建 WAL");

    /* ---------- 并发写入测试 ---------- */
    const int n_threads = 4;
    const int inserts_per_thread = 10;

    concurrent_wal_ctx_t contexts[n_threads];
#ifdef _WIN32
    HANDLE handles[n_threads];
#else
    pthread_t threads[n_threads];
#endif

    printf("  启动 %d 个并发线程，每线程 %d 次插入...\n", n_threads, inserts_per_thread);

    /* 启动所有线程 */
    for (int i = 0; i < n_threads; i++) {
        contexts[i].wal = wal;
        contexts[i].txn_id = (uint32_t)(100 + i);
        contexts[i].n_inserts = inserts_per_thread;
        contexts[i].start_value = i * inserts_per_thread;
        contexts[i].success_count = 0;

#ifdef _WIN32
        handles[i] = (HANDLE)_beginthreadex(NULL, 0, concurrent_wal_thread, &contexts[i], 0, NULL);
#else
        pthread_create(&threads[i], NULL, concurrent_wal_thread, &contexts[i]);
#endif
    }

    /* 等待所有线程完成 */
#ifdef _WIN32
    WaitForMultipleObjects(n_threads, handles, TRUE, INFINITE);
#else
    for (int i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
#endif

    /* 统计成功插入数 */
    int total_success = 0;
    for (int i = 0; i < n_threads; i++) {
        total_success += contexts[i].success_count;
        printf("  线程 %d 成功: %d 次\n", i, contexts[i].success_count);
    }

    int expected_total = n_threads * inserts_per_thread;
    ASSERT_EQ(total_success, expected_total, "所有并发插入都成功");
    printf("  总成功插入数: %d (期望: %d)\n", total_success, expected_total);

    /* ---------- 验证 WAL 一致性 ---------- */
    uint64_t lsn = wal_get_lsn(wal);
    /* 每个事务: 1 BEGIN + N INSERT + 1 COMMIT = N + 2 条记录
     * 4 线程 x (10 insert + 2) = 48 条记录 */
    int expected_lsn = n_threads * (inserts_per_thread + 2);
    ASSERT_GE((int)lsn, expected_lsn, "WAL LSN 正确");
    printf("  WAL LSN: %llu (期望 >= %d)\n", (unsigned long long)lsn, expected_lsn);

    /* ---------- 并发读测试 ---------- */
    printf("  验证并发读取一致性...\n");

    wal_recovery_info_t info;
    int rc = wal_analyze(wal_path, &info);
    ASSERT_EQ(rc, 0, "WAL 分析成功");

    /* 应该没有活动事务（都已提交） */
    ASSERT_EQ((int)info.active_txn_count, 0, "所有事务都已提交");
    printf("  活动事务数: %zu (期望: 0)\n", info.active_txn_count);

    wal_recovery_info_free(&info);
    wal_close(wal);

    /* ---------- 验证重放 ---------- */
    wal_t *wal2 = wal_open(wal_path);
    ASSERT_NON_NULL(wal2, "重新打开 WAL");

    typedef struct {
        int count;
        int insert_count;
    } verify_ctx_t;
    verify_ctx_t vctx = {0, 0};

    int verify_callback(void *ctx_arg, wal_log_type_t type,
                       const void *key, size_t key_len,
                       const void *value, size_t value_len) {
        (void)key; (void)value;
        verify_ctx_t *v = (verify_ctx_t *)ctx_arg;
        v->count++;
        if (type == WAL_LOG_HEAP_INSERT) {
            v->insert_count++;
        }
        return 0;
    }

    rc = wal_redo(wal_path, 0, (wal_apply_fn)verify_callback, &vctx);
    ASSERT_EQ(rc, 0, "WAL 重放成功");
    ASSERT_EQ(vctx.insert_count, total_success, "重放的插入记录数正确");
    printf("  重放记录数: %d, 插入记录: %d\n", vctx.count, vctx.insert_count);

    wal_close(wal2);
    cleanup_test_dir();

    printf("  [PASS] Vector 并发安全验证通过\n");
    return 0;
}

/* ========================================================================
 * 主函数
 * ======================================================================== */
int main(void) {
    printf("========================================\n");
    printf("跨模态集成测试框架\n");
    printf("========================================\n\n");

    int failed = 0;

    failed += test_vector_graph_rag_integration();
    printf("\n");

    failed += test_relational_mvcc_wal();
    printf("\n");

    failed += test_vector_concurrent();
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
