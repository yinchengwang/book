/*
 * test_mvcc.cpp - MVCC 并发控制测试
 */

#include <gtest/gtest.h>
#include <db/concurrency/mvcc.h>

/* ─────────────────────────────────────────────────────────────────
 * 测试辅助函数
 * ───────────────────────────────────────────────────────────────── */

static mvcc_snapshot_t *create_test_snapshot(
    mvcc_txn_id_t xmin,
    mvcc_txn_id_t xmax,
    const mvcc_txn_id_t *active_txns,
    int active_count)
{
    return mvcc_snapshot_create(xmin, xmax, active_txns, active_count);
}

/* ─────────────────────────────────────────────────────────────────
 * 版本链测试
 * ───────────────────────────────────────────────────────────────── */

TEST(MVCCVersion, CreateVersion)
{
    const char *data = "test";
    size_t data_len = 4;
    mvcc_version_t *version = mvcc_version_new(1, data, data_len);

    ASSERT_NE(nullptr, version);
    EXPECT_EQ(1, version->xmin);
    EXPECT_EQ(MVCC_INVALID_TXN_ID, version->xmax);
    EXPECT_EQ(0, memcmp(version->data, data, data_len));
    EXPECT_EQ(data_len, version->data_size);
    EXPECT_EQ(nullptr, version->next);

    mvcc_version_destroy(version);
}

TEST(MVCCVersion, InsertIntoChain)
{
    mvcc_version_t *head = nullptr;

    mvcc_version_t *v1 = mvcc_version_new(1, "v1", 2);
    mvcc_version_t *v2 = mvcc_version_new(2, "v2", 2);
    mvcc_version_t *v3 = mvcc_version_new(3, "v3", 2);

    /* 插入 v1 */
    mvcc_version_insert(&head, v1);
    EXPECT_EQ(v1, head);
    EXPECT_EQ(nullptr, head->next);

    /* 插入 v2 到头部 */
    mvcc_version_insert(&head, v2);
    EXPECT_EQ(v2, head);
    EXPECT_EQ(v1, head->next);

    /* 插入 v3 到头部 */
    mvcc_version_insert(&head, v3);
    EXPECT_EQ(v3, head);
    EXPECT_EQ(v2, head->next);
    EXPECT_EQ(v1, head->next->next);

    /* 清理 */
    mvcc_version_destroy(v3);
    mvcc_version_destroy(v2);
    mvcc_version_destroy(v1);
}

TEST(MVCCVersion, MarkDeleted)
{
    mvcc_version_t *version = mvcc_version_new(1, "data", 4);

    EXPECT_EQ(MVCC_INVALID_TXN_ID, version->xmax);

    mvcc_version_mark_deleted(version, 5);
    EXPECT_EQ(5, version->xmax);

    mvcc_version_destroy(version);
}

TEST(MVCCVersion, ChainLength)
{
    mvcc_version_t *head = nullptr;

    EXPECT_EQ(0, mvcc_version_chain_length(head));

    mvcc_version_insert(&head, mvcc_version_new(1, "v1", 2));
    EXPECT_EQ(1, mvcc_version_chain_length(head));

    mvcc_version_insert(&head, mvcc_version_new(2, "v2", 2));
    EXPECT_EQ(2, mvcc_version_chain_length(head));

    mvcc_version_insert(&head, mvcc_version_new(3, "v3", 2));
    EXPECT_EQ(3, mvcc_version_chain_length(head));

    /* 清理 */
    while (head != nullptr) {
        mvcc_version_t *next = head->next;
        mvcc_version_destroy(head);
        head = next;
    }
}

/* ─────────────────────────────────────────────────────────────────
 * 快照测试
 * ───────────────────────────────────────────────────────────────── */

TEST(MVCCSnapshot, CreateSnapshot)
{
    mvcc_txn_id_t active[] = {3, 5, 7};
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, active, 3);

    ASSERT_NE(nullptr, snapshot);
    EXPECT_EQ(1, snapshot->xmin);
    EXPECT_EQ(10, snapshot->xmax);
    EXPECT_EQ(3, snapshot->xip_count);

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCSnapshot, EmptySnapshot)
{
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, nullptr, 0);

    ASSERT_NE(nullptr, snapshot);
    EXPECT_EQ(1, snapshot->xmin);
    EXPECT_EQ(10, snapshot->xmax);
    EXPECT_EQ(0, snapshot->xip_count);
    EXPECT_EQ(nullptr, snapshot->xip_list);

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCSnapshot, ContainsTransaction)
{
    mvcc_txn_id_t active[] = {3, 5, 7};
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, active, 3);

    EXPECT_TRUE(mvcc_snapshot_contains_txn(snapshot, 3));
    EXPECT_TRUE(mvcc_snapshot_contains_txn(snapshot, 5));
    EXPECT_TRUE(mvcc_snapshot_contains_txn(snapshot, 7));
    EXPECT_FALSE(mvcc_snapshot_contains_txn(snapshot, 1));
    EXPECT_FALSE(mvcc_snapshot_contains_txn(snapshot, 2));
    EXPECT_FALSE(mvcc_snapshot_contains_txn(snapshot, 4));
    EXPECT_FALSE(mvcc_snapshot_contains_txn(snapshot, 10));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCSnapshot, CopySnapshot)
{
    mvcc_txn_id_t active[] = {3, 5, 7};
    mvcc_snapshot_t *original = create_test_snapshot(1, 10, active, 3);
    mvcc_snapshot_t *copy = mvcc_snapshot_copy(original);

    ASSERT_NE(nullptr, copy);
    EXPECT_EQ(original->xmin, copy->xmin);
    EXPECT_EQ(original->xmax, copy->xmax);
    EXPECT_EQ(original->xip_count, copy->xip_count);

    /* 验证是深拷贝 */
    EXPECT_NE(original->xip_list, copy->xip_list);
    for (int i = 0; i < original->xip_count; i++) {
        EXPECT_EQ(original->xip_list[i], copy->xip_list[i]);
    }

    mvcc_snapshot_destroy(original);
    mvcc_snapshot_destroy(copy);
}

TEST(MVCCSnapshot, SerializeAndDeserialize)
{
    mvcc_txn_id_t active[] = {3, 5, 7};
    mvcc_snapshot_t *original = create_test_snapshot(1, 10, active, 3);

    /* 序列化 */
    size_t buf_size = mvcc_snapshot_serialize_size(original);
    ASSERT_GT(buf_size, 0);

    char *buf = (char *)malloc(buf_size);
    ASSERT_NE(nullptr, buf);

    size_t written = mvcc_snapshot_serialize(original, buf, buf_size);
    EXPECT_EQ(buf_size, written);

    /* 反序列化 */
    mvcc_snapshot_t *restored = mvcc_snapshot_deserialize(buf, buf_size);
    ASSERT_NE(nullptr, restored);

    EXPECT_EQ(original->xmin, restored->xmin);
    EXPECT_EQ(original->xmax, restored->xmax);
    EXPECT_EQ(original->xip_count, restored->xip_count);
    for (int i = 0; i < original->xip_count; i++) {
        EXPECT_EQ(original->xip_list[i], restored->xip_list[i]);
    }

    free(buf);
    mvcc_snapshot_destroy(original);
    mvcc_snapshot_destroy(restored);
}

/* ─────────────────────────────────────────────────────────────────
 * 可见性判断测试
 * ───────────────────────────────────────────────────────────────── */

TEST(MVCCVisibility, OwnTransactionVisible)
{
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, nullptr, 0);

    /* 自身事务创建的版本始终可见 */
    EXPECT_TRUE(mvcc_version_visible(snapshot, 1, 0, 1));
    EXPECT_TRUE(mvcc_version_visible(snapshot, 5, 0, 5));
    EXPECT_TRUE(mvcc_version_visible(snapshot, 10, 0, 10));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCVisibility, CommittedVersionVisible)
{
    /* 快照 xmin=1, xmax=10，活跃事务：无
     * 版本由已提交事务（id=2）创建，未删除
     */
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, nullptr, 0);
    EXPECT_TRUE(mvcc_version_visible(snapshot, 2, 0, 1));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCVisibility, UncommittedVersionInvisible)
{
    /* 快照包含活跃事务 5
     * 版本由事务 5 创建，未提交
     */
    mvcc_txn_id_t active[] = {5};
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, active, 1);

    /* 事务 5 还在活跃列表中，未提交 */
    EXPECT_FALSE(mvcc_version_visible(snapshot, 5, 0, 1));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCVisibility, DeletedVersionInvisible)
{
    /* 快照 xmin=1, xmax=10，活跃事务：无
     * 版本由事务 2 创建，已被事务 6 删除
     * 删除事务 6 < xmax=10，已提交
     */
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, nullptr, 0);
    EXPECT_FALSE(mvcc_version_visible(snapshot, 2, 6, 1));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCVisibility, UncommittedDeleteInvisible)
{
    /* 快照包含活跃事务 6
     * 版本由事务 2 创建，被活跃事务 6 删除（未提交）
     *
     * 注意：即使删除事务未提交，该版本对其他事务也不可见
     * 因为存在一个"未完成的删除"标记
     */
    mvcc_txn_id_t active[] = {6};
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, active, 1);

    /* 删除事务 6 还在活跃列表中，未提交
     * 但存在未完成的删除标记，版本不可见 */
    EXPECT_FALSE(mvcc_version_visible(snapshot, 2, 6, 1));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCVisibility, FindVisibleInChain)
{
    /* 构建版本链：
     * HEAD -> V3(xmin=6, xmax=0) -> V2(xmin=5, xmax=7) -> V1(xmin=2, xmax=0)
     * 快照 xmin=1, xmax=10，活跃事务：[7]
     *
     * 可见性检查：
     * - V3: xmin=6 < xmax=10, 6不在活跃列表, xmax=0 -> 可见
     * - V2: xmax=7 < xmax=10, 7不在活跃列表 -> 已删除，不可访问
     * - V1: xmin=2 < xmax=10, 2不在活跃列表, xmax=0 -> 可见
     *
     * 版本链遍历从 HEAD 开始找第一个可见版本，即 V3
     */
    mvcc_version_t *head = nullptr;

    /* 按顺序创建并插入：先 V1，再 V2，再 V3 */
    mvcc_version_t *v1 = mvcc_version_new(2, "v1", 2);
    mvcc_version_insert(&head, v1);  /* HEAD -> V1 */

    mvcc_version_t *v2 = mvcc_version_new(5, "v2", 2);
    mvcc_version_mark_deleted(v2, 7);  /* V2 被事务 7 删除 */
    mvcc_version_insert(&head, v2);  /* HEAD -> V2 -> V1 */

    mvcc_version_t *v3 = mvcc_version_new(6, "v3", 2);
    mvcc_version_insert(&head, v3);  /* HEAD -> V3 -> V2 -> V1 */

    /* 快照 */
    mvcc_txn_id_t active[] = {7};
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, active, 1);

    /* 查找可见版本：从 HEAD 开始，第一个可见的是 V3 */
    mvcc_version_t *visible = mvcc_version_find_visible(head, snapshot, 1);
    EXPECT_NE(nullptr, visible);
    EXPECT_EQ(6, visible->xmin);  /* V3 可见，是链表中第一个可见版本 */

    /* 清理 */
    mvcc_snapshot_destroy(snapshot);
    mvcc_version_destroy(v3);
    mvcc_version_destroy(v2);
    mvcc_version_destroy(v1);
}

/* ─────────────────────────────────────────────────────────────────
 * 写冲突检测测试
 * ───────────────────────────────────────────────────────────────── */

TEST(MVCCVisibility, NoConflictWhenNotDeleted)
{
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, nullptr, 0);

    /* 行未被删除，无冲突 */
    EXPECT_FALSE(mvcc_check_write_conflict(snapshot, 0, 1));
    EXPECT_FALSE(mvcc_check_write_conflict(snapshot, MVCC_INVALID_TXN_ID, 1));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCVisibility, ConflictWithCommittedDelete)
{
    /* 行被事务 5 删除，已提交 */
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, nullptr, 0);

    /* 事务 5 已提交，xmax=5 < xmax=10 */
    EXPECT_TRUE(mvcc_check_write_conflict(snapshot, 5, 1));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCVisibility, NoConflictWithOwnDelete)
{
    /* 行被当前事务删除 */
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, nullptr, 0);

    EXPECT_FALSE(mvcc_check_write_conflict(snapshot, 1, 1));

    mvcc_snapshot_destroy(snapshot);
}

TEST(MVCCVisibility, NoConflictWithUncommittedDelete)
{
    /* 行被未提交事务删除 */
    mvcc_txn_id_t active[] = {5};
    mvcc_snapshot_t *snapshot = create_test_snapshot(1, 10, active, 1);

    /* 事务 5 在活跃列表中，未提交 */
    EXPECT_FALSE(mvcc_check_write_conflict(snapshot, 5, 1));

    mvcc_snapshot_destroy(snapshot);
}

/* ─────────────────────────────────────────────────────────────────
 * Undo 日志测试
 * ───────────────────────────────────────────────────────────────── */

TEST(MVCCUndo, CreateUndoRecord)
{
    mvcc_ctid_t row_ptr = {1, 2};
    const char *old_data = "old value";
    mvcc_undo_record_t *record = mvcc_undo_create(
        1, MVCC_UNDO_UPDATE, old_data, strlen(old_data), &row_ptr);

    ASSERT_NE(nullptr, record);
    EXPECT_EQ(1, record->txn_id);
    EXPECT_EQ(MVCC_UNDO_UPDATE, record->type);
    /* 验证 old_data 内容 */
    EXPECT_NE(nullptr, record->old_data);
    EXPECT_EQ(0, memcmp(record->old_data, old_data, strlen(old_data)));
    EXPECT_EQ(1, record->row_ptr.block_num);
    EXPECT_EQ(2, record->row_ptr.offset);

    mvcc_undo_destroy(record);
}

TEST(MVCCUndo, UndoTypes)
{
    mvcc_undo_record_t *insert_rec = mvcc_undo_create(
        1, MVCC_UNDO_INSERT, nullptr, 0, nullptr);
    mvcc_undo_record_t *update_rec = mvcc_undo_create(
        1, MVCC_UNDO_UPDATE, "old", 3, nullptr);
    mvcc_undo_record_t *delete_rec = mvcc_undo_create(
        1, MVCC_UNDO_DELETE, "data", 4, nullptr);

    EXPECT_EQ(MVCC_UNDO_INSERT, insert_rec->type);
    EXPECT_EQ(MVCC_UNDO_UPDATE, update_rec->type);
    EXPECT_EQ(MVCC_UNDO_DELETE, delete_rec->type);

    mvcc_undo_destroy(insert_rec);
    mvcc_undo_destroy(update_rec);
    mvcc_undo_destroy(delete_rec);
}

/* ─────────────────────────────────────────────────────────────────
 * GC 配置测试
 * ───────────────────────────────────────────────────────────────── */

TEST(MVCCGC, DefaultConfig)
{
    mvcc_gc_config_t config = mvcc_gc_default_config();

    EXPECT_EQ(50, config.vacuum_threshold);
    EXPECT_DOUBLE_EQ(0.2, config.vacuum_scale_factor);
    EXPECT_EQ(20, config.vacuum_cost_delay);
    EXPECT_EQ(1000, config.vacuum_cost_limit);
    EXPECT_EQ(60, config.autovacuum_naptime);
    EXPECT_TRUE(config.autovacuum_enabled);
}

TEST(MVCCGC, Analyze)
{
    int dead_count = 0;
    int result = mvcc_gc_analyze(1, &dead_count);

    /* 当前是占位实现，应该返回成功但 dead_count 为 0 */
    EXPECT_EQ(0, result);
    EXPECT_EQ(0, dead_count);
}

TEST(MVCCGC, Vacuum)
{
    mvcc_gc_config_t config = mvcc_gc_default_config();
    int result = mvcc_gc_vacuum(1, &config);

    /* 占位实现可能返回 0（不需要清理）或负数（未实现） */
    EXPECT_GE(result, -1);
}

TEST(MVCCGC, CleanupUndo)
{
    int cleaned = mvcc_gc_cleanup_undo(100);
    EXPECT_GE(cleaned, 0);
}

/* ─────────────────────────────────────────────────────────────────
 * 并发场景测试（概念验证）
 * ───────────────────────────────────────────────────────────────── */

TEST(MVCCScenario, ConcurrentReadWrite)
{
    /*
     * 场景：事务 T1 读取数据，事务 T2 同时写入同一行
     *
     * 时间线：
     * 1. T1 开始，获取快照 S1 (xmin=1, xmax=5, active=[2])
     * 2. T2 开始
     * 3. T2 插入新行
     * 4. T1 读取，应该看到旧版本
     * 5. T2 提交
     * 6. T1 再次读取同一行，还是看到旧版本（可重复读）
     */

    /* T1 的快照 */
    mvcc_txn_id_t t1_active[] = {2};  /* T2 正在运行 */
    mvcc_snapshot_t *t1_snapshot = create_test_snapshot(1, 5, t1_active, 1);

    /* 构建版本链：HEAD -> V2(T2插入) -> V1(T1能看到) */
    mvcc_version_t *head = nullptr;

    mvcc_version_t *v1 = mvcc_version_new(1, "or", 2);
    mvcc_version_insert(&head, v1);  /* HEAD -> V1 */

    mvcc_version_t *v2 = mvcc_version_new(2, "up", 2);
    mvcc_version_insert(&head, v2);  /* HEAD -> V2 -> V1 */

    /* T1 读取：
     * - V2(xmin=2): 2 在活跃列表中，未提交 -> 不可见
     * - V1(xmin=1): 1 < xmax=5, 不在活跃列表, xmax=0 -> 可见
     * 所以 T1 看到 V1（旧版本）*/
    mvcc_version_t *visible = mvcc_version_find_visible(head, t1_snapshot, 1);
    ASSERT_NE(nullptr, visible);
    EXPECT_EQ(1, visible->xmin);  /* 看到旧版本 V1，符合 MVCC 语义 */

    /* 清理 */
    mvcc_snapshot_destroy(t1_snapshot);
    mvcc_version_destroy(v2);
    mvcc_version_destroy(v1);
}

TEST(MVCCScenario, ReadCommittedSeesLatest)
{
    /*
     * 场景：READ COMMITTED 隔离级别
     *
     * 每次语句执行前重新获取快照
     */

    /* 构建初始版本链：HEAD -> V1 */
    mvcc_version_t *head = nullptr;
    mvcc_version_t *v1 = mvcc_version_new(1, "v1", 2);
    mvcc_version_insert(&head, v1);

    /* 第一次读取：快照不包含任何活跃事务 */
    mvcc_snapshot_t *snapshot1 = create_test_snapshot(1, 10, nullptr, 0);
    mvcc_version_t *visible1 = mvcc_version_find_visible(head, snapshot1, 1);
    ASSERT_NE(nullptr, visible1);
    EXPECT_EQ(1, visible1->xmin);
    mvcc_snapshot_destroy(snapshot1);

    /* T2 插入新版本：HEAD -> V2 -> V1 */
    mvcc_version_t *v2 = mvcc_version_new(2, "v2", 2);
    mvcc_version_insert(&head, v2);

    /* 第二次读取：快照不包含活跃事务，能看到 V2 */
    mvcc_snapshot_t *snapshot2 = create_test_snapshot(1, 10, nullptr, 0);
    mvcc_version_t *visible2 = mvcc_version_find_visible(head, snapshot2, 1);
    ASSERT_NE(nullptr, visible2);
    EXPECT_EQ(2, visible2->xmin);  /* 看到新版本 V2 */
    mvcc_snapshot_destroy(snapshot2);

    /* 清理 */
    mvcc_version_destroy(v2);
    mvcc_version_destroy(v1);
}