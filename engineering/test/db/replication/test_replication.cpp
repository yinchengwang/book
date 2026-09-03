/*
 * test_replication.c - 主从复制测试
 */

#include <gtest/gtest.h>
#include <db/replication/replication.h>

/* ─────────────────────────────────────────────────────────────────
 * 配置测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Replication, ConfigCreate)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_PRIMARY);
    ASSERT_NE(nullptr, config);
    EXPECT_EQ(REPL_ROLE_PRIMARY, config->role);
    EXPECT_EQ(5432, config->primary_port);
    EXPECT_EQ(5433, config->replica_port);
    EXPECT_EQ(8, config->max_replicas);

    repl_config_destroy(config);
}

TEST(Replication, ConfigCreateReplica)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_REPLICA);
    ASSERT_NE(nullptr, config);
    EXPECT_EQ(REPL_ROLE_REPLICA, config->role);

    repl_config_destroy(config);
}

/* ─────────────────────────────────────────────────────────────────
 * 复制管理器测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Replication, ManagerCreate)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_PRIMARY);
    repl_manager_t *manager = repl_manager_create(config);

    ASSERT_NE(nullptr, manager);
    EXPECT_EQ(REPL_ROLE_PRIMARY, repl_get_role(manager));
    EXPECT_EQ(REPL_STATE_DISCONNECTED, repl_get_state(manager));

    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

TEST(Replication, ManagerCreateReplica)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_REPLICA);
    repl_manager_t *manager = repl_manager_create(config);

    ASSERT_NE(nullptr, manager);
    EXPECT_EQ(REPL_ROLE_REPLICA, repl_get_role(manager));

    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

/* ─────────────────────────────────────────────────────────────────
 * 启动/停止测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Replication, StartStopPrimary)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_PRIMARY);
    repl_manager_t *manager = repl_manager_create(config);

    ASSERT_NE(nullptr, manager);

    int result = repl_start(manager);
    EXPECT_EQ(0, result);
    EXPECT_EQ(REPL_STATE_NORMAL, repl_get_state(manager));

    repl_stop(manager);
    EXPECT_EQ(REPL_STATE_SHUTDOWN, repl_get_state(manager));

    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

TEST(Replication, StartStopReplica)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_REPLICA);
    repl_manager_t *manager = repl_manager_create(config);

    ASSERT_NE(nullptr, manager);

    int result = repl_start(manager);
    EXPECT_EQ(0, result);
    EXPECT_EQ(REPL_STATE_CONNECTING, repl_get_state(manager));

    repl_stop(manager);

    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

/* ─────────────────────────────────────────────────────────────────
 * 统计信息测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Replication, StatsInit)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_PRIMARY);
    repl_manager_t *manager = repl_manager_create(config);

    const repl_stats_t *stats = repl_get_stats(manager);
    ASSERT_NE(nullptr, stats);
    EXPECT_EQ(0, stats->bytes_sent);
    EXPECT_EQ(0, stats->bytes_received);
    EXPECT_EQ(0, stats->msgs_sent);
    EXPECT_EQ(0, stats->msgs_received);

    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

/* ─────────────────────────────────────────────────────────────────
 * 复制延迟测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Replication, GetLag)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_PRIMARY);
    repl_manager_t *manager = repl_manager_create(config);

    int64_t lag = repl_get_lag_ms(manager);
    EXPECT_GE(lag, 0);

    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

/* ─────────────────────────────────────────────────────────────────
 * 故障切换测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Replication, FailoverCheck)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_REPLICA);
    repl_manager_t *manager = repl_manager_create(config);

    /* 初始状态不需要故障切换 */
    bool need_failover = repl_need_failover(manager);
    EXPECT_FALSE(need_failover);

    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

TEST(Replication, Failover)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_REPLICA);
    repl_manager_t *manager = repl_manager_create(config);

    int result = repl_do_failover(manager);
    EXPECT_EQ(0, result);
    EXPECT_EQ(REPL_ROLE_PRIMARY, repl_get_role(manager));

    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

/* ─────────────────────────────────────────────────────────────────
 * 连接测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Replication, ConnectPrimary)
{
    repl_config_t *config = repl_config_create(REPL_ROLE_REPLICA);
    repl_manager_t *manager = repl_manager_create(config);

    /* 连接接口存在（实际连接需要主节点运行）
     * 简化版实现返回有效连接对象但 fd = -1 */
    repl_connection_t *conn = repl_connect_primary(manager, "localhost", 5432);
    ASSERT_NE(nullptr, conn);
    /* 连接对象已创建，后续操作通过 API 进行 */
    repl_connection_close(conn);
    repl_manager_destroy(manager);
    repl_config_destroy(config);
}

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Replication, StateName)
{
    EXPECT_STREQ("disconnected", repl_state_name(REPL_STATE_DISCONNECTED));
    EXPECT_STREQ("connecting", repl_state_name(REPL_STATE_CONNECTING));
    EXPECT_STREQ("handshake", repl_state_name(REPL_STATE_HANDSHAKE));
    EXPECT_STREQ("streaming", repl_state_name(REPL_STATE_STREAMING));
    EXPECT_STREQ("catchup", repl_state_name(REPL_STATE_CATCHUP));
    EXPECT_STREQ("normal", repl_state_name(REPL_STATE_NORMAL));
    EXPECT_STREQ("shutdown", repl_state_name(REPL_STATE_SHUTDOWN));
}

TEST(Replication, RoleName)
{
    EXPECT_STREQ("primary", repl_role_name(REPL_ROLE_PRIMARY));
    EXPECT_STREQ("replica", repl_role_name(REPL_ROLE_REPLICA));
}