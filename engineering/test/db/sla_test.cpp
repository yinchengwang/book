/**
 * @file sla_test.cpp
 * @brief 订阅、对账、老化模块单元测试
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "db/subscription/cdc.h"
#include "db/subscription/subscription.h"
#include "db/ledger/ledger.h"
#include "db/aging/aging.h"
#include "db/core/log.h"
}

/* ========================================================================
 * 测试辅助函数
 * ======================================================================== */

class SlaTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_ERROR;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        temp_dir = "test_sla_temp";
#ifdef _WIN32
        _mkdir(temp_dir.c_str());
#else
        mkdir(temp_dir.c_str(), 0755);
#endif
    }

    void TearDown() override {
#ifdef _WIN32
        system(("rmdir /s /q " + temp_dir).c_str());
#else
        system(("rm -rf " + temp_dir).c_str());
#endif
    }

    std::string temp_dir;
};

/* ========================================================================
 * CDC 模块测试
 * ======================================================================== */

class CDCTest : public SlaTest {
};

TEST_F(CDCTest, CreateAndDestroy) {
    cdc_context_t *ctx = cdc_create(temp_dir.c_str(), 0);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(cdc_get_current_lsn(ctx), 0u);
    EXPECT_EQ(ctx->relation_count, 0);

    cdc_destroy(ctx);
}

TEST_F(CDCTest, AddRelation) {
    cdc_context_t *ctx = cdc_create(temp_dir.c_str(), 100);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(cdc_add_relation(ctx, 1), 0);
    EXPECT_EQ(cdc_add_relation(ctx, 2), 0);
    EXPECT_EQ(ctx->relation_count, 2);

    EXPECT_TRUE(cdc_is_relation_monitored(ctx, 1));
    EXPECT_TRUE(cdc_is_relation_monitored(ctx, 2));
    EXPECT_FALSE(cdc_is_relation_monitored(ctx, 99));

    cdc_destroy(ctx);
}

TEST_F(CDCTest, RecordInsert) {
    cdc_context_t *ctx = cdc_create(temp_dir.c_str(), 0);
    ASSERT_NE(ctx, nullptr);

    cdc_add_relation(ctx, 1);

    char tuple[16] = "test_data";
    EXPECT_EQ(cdc_record_insert(ctx, 1, 100, tuple, sizeof(tuple)), 0);
    EXPECT_EQ(cdc_get_current_lsn(ctx), 1u);

    cdc_destroy(ctx);
}

TEST_F(CDCTest, RecordUpdate) {
    cdc_context_t *ctx = cdc_create(temp_dir.c_str(), 0);
    ASSERT_NE(ctx, nullptr);

    cdc_add_relation(ctx, 1);

    char old_tuple[16] = "old_data";
    char new_tuple[16] = "new_data";
    EXPECT_EQ(cdc_record_update(ctx, 1, 100, old_tuple, sizeof(old_tuple),
                                 new_tuple, sizeof(new_tuple)), 0);
    EXPECT_EQ(cdc_get_current_lsn(ctx), 1u);

    cdc_destroy(ctx);
}

TEST_F(CDCTest, RecordDelete) {
    cdc_context_t *ctx = cdc_create(temp_dir.c_str(), 0);
    ASSERT_NE(ctx, nullptr);

    cdc_add_relation(ctx, 1);

    char tuple[16] = "deleted";
    EXPECT_EQ(cdc_record_delete(ctx, 1, 100, tuple, sizeof(tuple)), 0);
    EXPECT_EQ(cdc_get_current_lsn(ctx), 1u);

    cdc_destroy(ctx);
}

TEST_F(CDCTest, SkipUnmonitoredRelation) {
    cdc_context_t *ctx = cdc_create(temp_dir.c_str(), 0);
    ASSERT_NE(ctx, nullptr);

    cdc_add_relation(ctx, 1);

    char tuple[16] = "test";
    /* 尝试记录未监控的表 ID=2 */
    EXPECT_EQ(cdc_record_insert(ctx, 2, 100, tuple, sizeof(tuple)), 0);
    /* LSN 不应增加 */
    EXPECT_EQ(cdc_get_current_lsn(ctx), 0u);

    cdc_destroy(ctx);
}

TEST_F(CDCTest, LSNManagement) {
    cdc_context_t *ctx = cdc_create(temp_dir.c_str(), 50);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(cdc_get_current_lsn(ctx), 50u);

    cdc_set_lsn(ctx, 100);
    EXPECT_EQ(cdc_get_current_lsn(ctx), 100u);

    cdc_destroy(ctx);
}

/* ========================================================================
 * 订阅模块测试
 * ======================================================================== */

class SubscriptionTest : public SlaTest {
};

static int test_callback_count = 0;

static int test_callback(const cdc_change_t *change, void *user_data) {
    (void)change;
    (void)user_data;
    test_callback_count++;
    return 0;
}

TEST_F(SubscriptionTest, CreateAndDestroy) {
    subscription_manager_t *mgr = subscription_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);
    EXPECT_EQ(mgr->subscription_count, 0);

    subscription_manager_destroy(mgr);
}

TEST_F(SubscriptionTest, CreateSubscription) {
    subscription_manager_t *mgr = subscription_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    subscription_t *sub = subscription_create(mgr, "test_sub", test_callback, nullptr);
    ASSERT_NE(sub, nullptr);

    EXPECT_STREQ(sub->name, "test_sub");
    EXPECT_EQ(sub->state, SUBSCRIPTION_STATE_INACTIVE);
    EXPECT_EQ(mgr->subscription_count, 1);

    subscription_manager_destroy(mgr);
}

TEST_F(SubscriptionTest, ActivateAndPause) {
    subscription_manager_t *mgr = subscription_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    subscription_create(mgr, "test_sub", test_callback, nullptr);

    EXPECT_EQ(subscription_activate(mgr, "test_sub"), 0);
    subscription_t *sub = subscription_get(mgr, "test_sub");
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->state, SUBSCRIPTION_STATE_ACTIVE);

    EXPECT_EQ(subscription_pause(mgr, "test_sub"), 0);
    sub = subscription_get(mgr, "test_sub");
    EXPECT_EQ(sub->state, SUBSCRIPTION_STATE_PAUSED);

    subscription_manager_destroy(mgr);
}

TEST_F(SubscriptionTest, DestroySubscription) {
    subscription_manager_t *mgr = subscription_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    subscription_create(mgr, "test_sub", test_callback, nullptr);
    EXPECT_EQ(mgr->subscription_count, 1);

    EXPECT_EQ(subscription_destroy(mgr, "test_sub"), 0);
    EXPECT_EQ(mgr->subscription_count, 0);

    subscription_manager_destroy(mgr);
}

TEST_F(SubscriptionTest, DuplicateName) {
    subscription_manager_t *mgr = subscription_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    subscription_create(mgr, "test_sub", test_callback, nullptr);
    subscription_t *sub2 = subscription_create(mgr, "test_sub", test_callback, nullptr);

    EXPECT_EQ(sub2, nullptr);  /* 应创建失败 */
    EXPECT_EQ(mgr->subscription_count, 1);

    subscription_manager_destroy(mgr);
}

TEST_F(SubscriptionTest, FilterByRelation) {
    subscription_manager_t *mgr = subscription_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    subscription_t *sub = subscription_create(mgr, "test_sub", test_callback, nullptr);
    ASSERT_NE(sub, nullptr);

    subscription_add_relation(sub, 1);
    subscription_add_relation(sub, 2);
    EXPECT_EQ(sub->relation_count, 2);

    subscription_manager_destroy(mgr);
}

/* ========================================================================
 * 账本模块测试
 * ======================================================================== */

class LedgerTest : public SlaTest {
};

TEST_F(LedgerTest, CreateAndDestroy) {
    ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
    ASSERT_NE(ledger, nullptr);

    EXPECT_STREQ(ledger->name, "test_ledger");
    EXPECT_EQ(ledger->current_balance, 0);
    EXPECT_EQ(ledger->entry_count, 0);

    ledger_destroy(ledger);
}

TEST_F(LedgerTest, AddDebitEntry) {
    ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
    ASSERT_NE(ledger, nullptr);

    int64_t seq = ledger_add_entry(ledger, LEDGER_ENTRY_DEBIT, 100,
                                    "txn_001", "Payment for goods");
    EXPECT_GE(seq, 1);
    EXPECT_EQ(ledger->current_balance, -100);  /* 借方减少余额 */
    EXPECT_EQ(ledger->entry_count, 1);

    ledger_destroy(ledger);
}

TEST_F(LedgerTest, AddCreditEntry) {
    ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
    ASSERT_NE(ledger, nullptr);

    int64_t seq = ledger_add_entry(ledger, LEDGER_ENTRY_CREDIT, 500,
                                    "txn_002", "Sale revenue");
    EXPECT_GE(seq, 1);
    EXPECT_EQ(ledger->current_balance, 500);  /* 贷方增加余额 */
    EXPECT_EQ(ledger->entry_count, 1);

    ledger_destroy(ledger);
}

TEST_F(LedgerTest, IdempotencyKey) {
    ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
    ASSERT_NE(ledger, nullptr);

    /* 第一次添加 */
    int64_t seq1 = ledger_add_entry(ledger, LEDGER_ENTRY_CREDIT, 100,
                                     "idempotent_key", "First");
    EXPECT_GE(seq1, 1);

    /* 相同幂等键第二次添加应被忽略 */
    int64_t seq2 = ledger_add_entry(ledger, LEDGER_ENTRY_CREDIT, 100,
                                     "idempotent_key", "Second");
    EXPECT_EQ(seq2, seq1);  /* 返回相同序列号 */
    EXPECT_EQ(ledger->entry_count, 1);  /* 不增加条目 */

    ledger_destroy(ledger);
}

TEST_F(LedgerTest, BalanceCalculation) {
    ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
    ASSERT_NE(ledger, nullptr);

    ledger_add_entry(ledger, LEDGER_ENTRY_CREDIT, 1000, "txn_001", "Initial deposit");
    ledger_add_entry(ledger, LEDGER_ENTRY_DEBIT, 300, "txn_002", "Purchase 1");
    ledger_add_entry(ledger, LEDGER_ENTRY_DEBIT, 200, "txn_003", "Purchase 2");

    EXPECT_EQ(ledger->current_balance, 500);  /* 1000 - 300 - 200 */

    ledger_destroy(ledger);
}

TEST_F(LedgerTest, GetEntry) {
    ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
    ASSERT_NE(ledger, nullptr);

    ledger_add_entry(ledger, LEDGER_ENTRY_CREDIT, 100, "txn_001", "Test");
    ledger_add_entry(ledger, LEDGER_ENTRY_DEBIT, 50, "txn_002", "Test2");

    const ledger_entry_t *entry = ledger_get_entry(ledger, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->type, LEDGER_ENTRY_CREDIT);
    EXPECT_EQ(entry->amount, 100);

    ledger_destroy(ledger);
}

TEST_F(LedgerTest, VerifyBalance) {
    ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
    ASSERT_NE(ledger, nullptr);

    ledger_add_entry(ledger, LEDGER_ENTRY_CREDIT, 1000, "txn_001", "Initial");
    ledger_add_entry(ledger, LEDGER_ENTRY_DEBIT, 300, "txn_002", "Expense");

    EXPECT_TRUE(ledger_verify_balance(ledger));

    ledger_destroy(ledger);
}

TEST_F(LedgerTest, Reconcile) {
    ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
    ASSERT_NE(ledger, nullptr);

    ledger_add_entry(ledger, LEDGER_ENTRY_CREDIT, 1000, "txn_001", "Income");
    ledger_add_entry(ledger, LEDGER_ENTRY_DEBIT, 300, "txn_002", "Expense");

    ledger_reconcile_result_t *result = ledger_reconcile(ledger);
    ASSERT_NE(result, nullptr);

    EXPECT_TRUE(result->is_balanced);
    EXPECT_EQ(result->total_credit, 1000);
    EXPECT_EQ(result->total_debit, 300);
    EXPECT_EQ(result->expected_balance, 700);
    EXPECT_EQ(result->discrepancy, 0);

    ledger_free_reconcile_result(result);
    ledger_destroy(ledger);
}

TEST_F(LedgerTest, Persistence) {
    /* 创建并添加数据 */
    {
        ledger_t *ledger = ledger_create("test_ledger", temp_dir.c_str());
        ASSERT_NE(ledger, nullptr);

        ledger_add_entry(ledger, LEDGER_ENTRY_CREDIT, 500, "txn_001", "Initial");
        ledger_save(ledger);

        ledger_destroy(ledger);
    }

    /* 重新打开并验证 */
    {
        ledger_t *ledger = ledger_open("test_ledger", temp_dir.c_str());
        ASSERT_NE(ledger, nullptr);

        EXPECT_EQ(ledger->entry_count, 1);
        EXPECT_EQ(ledger->current_balance, 500);

        ledger_destroy(ledger);
    }
}

/* ========================================================================
 * 老化模块测试
 * ======================================================================== */

class AgingTest : public SlaTest {
};

TEST_F(AgingTest, CreateAndDestroy) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    EXPECT_EQ(mgr->policy_count, 0);
    EXPECT_GT(mgr->hot_capacity, 0);

    aging_manager_destroy(mgr);
}

TEST_F(AgingTest, AddPolicy) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    aging_policy_config_t policy = {};
    policy.type = AGING_POLICY_TTL;
    policy.config.ttl.max_age_seconds = 86400;  /* 1 天 */

    EXPECT_EQ(aging_add_policy(mgr, &policy), 0);
    EXPECT_EQ(mgr->policy_count, 1);

    const aging_policy_config_t *p = aging_get_policy(mgr, AGING_POLICY_TTL);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->config.ttl.max_age_seconds, 86400u);

    aging_manager_destroy(mgr);
}

TEST_F(AgingTest, RemovePolicy) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    aging_policy_config_t policy = {};
    policy.type = AGING_POLICY_TTL;
    aging_add_policy(mgr, &policy);

    EXPECT_EQ(aging_remove_policy(mgr, AGING_POLICY_TTL), 0);
    EXPECT_EQ(mgr->policy_count, 0);

    aging_manager_destroy(mgr);
}

TEST_F(AgingTest, CapacityManagement) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    aging_set_capacity(mgr, 1024 * 1024, 10 * 1024 * 1024, 100 * 1024 * 1024);

    EXPECT_EQ(mgr->hot_capacity, 1024 * 1024);
    EXPECT_EQ(mgr->warm_capacity, 10 * 1024 * 1024);
    EXPECT_EQ(mgr->cold_capacity, 100 * 1024 * 1024);

    aging_manager_destroy(mgr);
}

TEST_F(AgingTest, EvictionCheck) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    mgr->hot_usage = 900 * 1024;  /* 900KB 已用 */
    mgr->hot_capacity = 1024 * 1024;  /* 1MB 容量 */

    /* 需要淘汰（容量不足） */
    EXPECT_TRUE(aging_needs_eviction(mgr, AGING_TIER_HOT, 200 * 1024));

    /* 不需要淘汰（容量足够） */
    EXPECT_FALSE(aging_needs_eviction(mgr, AGING_TIER_HOT, 50 * 1024));

    aging_manager_destroy(mgr);
}

TEST_F(AgingTest, TierEvaluation) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    /* 添加 LRU 策略 */
    aging_policy_config_t policy = {};
    policy.type = AGING_POLICY_LRU;
    aging_add_policy(mgr, &policy);

    data_temperature_t temp = {};
    temp.current_tier = AGING_TIER_HOT;
    temp.last_access_time = (uint64_t)time(nullptr) * 1000000;
    temp.create_time = (uint64_t)time(nullptr) * 1000000;
    temp.access_count = 100;

    int32_t tier = aging_evaluate_tier(mgr, &temp);
    EXPECT_GE(tier, AGING_TIER_HOT);
    EXPECT_LE(tier, AGING_TIER_COLD);

    aging_manager_destroy(mgr);
}

TEST_F(AgingTest, ActionRecommendation) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    /* 添加策略 */
    aging_policy_config_t policy = {};
    policy.type = AGING_POLICY_LRU;
    aging_add_policy(mgr, &policy);

    data_temperature_t temp = {};
    temp.current_tier = AGING_TIER_HOT;
    temp.last_access_time = (uint64_t)(time(nullptr) - 3600) * 1000000;  /* 1 小时前 */
    temp.create_time = (uint64_t)(time(nullptr) - 86400) * 1000000;     /* 1 天前 */
    temp.access_count = 1;

    aging_action_t action = aging_recommend_action(mgr, &temp);
    EXPECT_GE(action, AGING_ACTION_KEEP);
    EXPECT_LE(action, AGING_ACTION_DELETE);

    aging_manager_destroy(mgr);
}

TEST_F(AgingTest, StatsManagement) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    aging_stats_t stats;
    aging_get_stats(mgr, &stats);

    EXPECT_EQ(stats.total_evictions, 0u);
    EXPECT_EQ(stats.total_archives, 0u);
    EXPECT_EQ(stats.total_deletes, 0u);

    aging_manager_destroy(mgr);
}

/* ========================================================================
 * 调度器测试
 * ======================================================================== */

class SchedulerTest : public SlaTest {
};

TEST_F(SchedulerTest, CreateAndDestroy) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    aging_scheduler_t *scheduler = aging_scheduler_create(mgr, 60);
    ASSERT_NE(scheduler, nullptr);

    EXPECT_FALSE(aging_scheduler_is_running(scheduler));
    EXPECT_EQ(aging_scheduler_get_interval(scheduler), 60u);

    aging_scheduler_destroy(scheduler);
    aging_manager_destroy(mgr);
}

TEST_F(SchedulerTest, IntervalManagement) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    aging_scheduler_t *scheduler = aging_scheduler_create(mgr, 60);
    ASSERT_NE(scheduler, nullptr);

    aging_scheduler_set_interval(scheduler, 120);
    EXPECT_EQ(aging_scheduler_get_interval(scheduler), 120u);

    aging_scheduler_destroy(scheduler);
    aging_manager_destroy(mgr);
}

TEST_F(SchedulerTest, BatchEvaluate) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    aging_scheduler_t *scheduler = aging_scheduler_create(mgr, 60);
    ASSERT_NE(scheduler, nullptr);

    /* 添加策略 */
    aging_policy_config_t policy = {};
    policy.type = AGING_POLICY_LRU;
    aging_add_policy(mgr, &policy);

    /* 准备测试数据 */
    void *data_ids[5];
    data_temperature_t temps[5];
    aging_action_t actions[5];

    for (int i = 0; i < 5; i++) {
        data_ids[i] = (void *)(intptr_t)(i + 1);
        temps[i].data_id = data_ids[i];
        temps[i].last_access_time = (uint64_t)(time(nullptr) - (i + 1) * 3600) * 1000000;
        temps[i].create_time = (uint64_t)(time(nullptr) - 86400) * 1000000;
        temps[i].access_count = 100 - i * 10;
        temps[i].current_tier = AGING_TIER_HOT;
    }

    int32_t count = aging_scheduler_batch_evaluate(scheduler, data_ids, temps, 5, actions, 5);

    /* 可能有一些动作被生成（取决于热度评估） */
    EXPECT_GE(count, 0);
    EXPECT_LE(count, 5);

    aging_scheduler_destroy(scheduler);
    aging_manager_destroy(mgr);
}

TEST_F(SchedulerTest, QueueSize) {
    aging_manager_t *mgr = aging_manager_create(temp_dir.c_str());
    ASSERT_NE(mgr, nullptr);

    aging_scheduler_t *scheduler = aging_scheduler_create(mgr, 60);
    ASSERT_NE(scheduler, nullptr);

    EXPECT_EQ(aging_scheduler_get_queue_size(scheduler), 0);

    aging_scheduler_destroy(scheduler);
    aging_manager_destroy(mgr);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
