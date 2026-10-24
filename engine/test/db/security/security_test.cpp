/**
 * @file test/db/security/security_test.cpp
 * @brief 安全管理系统单元测试
 */

#include "gtest/gtest.h"
#include "db/security/security_manager.h"
#include <cstring>

/* ============================================================
 * 测试夹具
 * ============================================================ */

class SecurityTest : public ::testing::Test {
protected:
    security_mgr_t *mgr;

    void SetUp() override {
        mgr = security_manager_create();
        ASSERT_NE(nullptr, mgr);
    }

    void TearDown() override {
        security_manager_destroy(mgr);
        mgr = nullptr;
    }
};

/* ============================================================
 * 用户管理测试
 * ============================================================ */

TEST_F(SecurityTest, CreateAndDropUser) {
    /* 创建用户 */
    int user_id = security_create_user(mgr, "alice", "password123");
    EXPECT_GT(user_id, 0);

    /* 重复创建同名用户应失败 */
    EXPECT_EQ(-1, security_create_user(mgr, "alice", "password123"));

    /* 删除用户 */
    EXPECT_EQ(0, security_drop_user(mgr, user_id));

    /* 删除不存在的用户应失败 */
    EXPECT_EQ(-1, security_drop_user(mgr, user_id));

    /* 删除后可以重新创建同名用户 */
    int new_user_id = security_create_user(mgr, "alice", "newpassword");
    EXPECT_GT(new_user_id, 0);
}

TEST_F(SecurityTest, CreateUserNullParams) {
    EXPECT_EQ(-1, security_create_user(nullptr, "alice", "password"));
    EXPECT_EQ(-1, security_create_user(mgr, nullptr, "password"));
    EXPECT_EQ(-1, security_create_user(mgr, "alice", nullptr));
}

TEST_F(SecurityTest, DropUserInvalidParams) {
    EXPECT_EQ(-1, security_drop_user(nullptr, 1));
    EXPECT_EQ(-1, security_drop_user(mgr, -1));
    EXPECT_EQ(-1, security_drop_user(mgr, 999));  /* 不存在的用户 */
}

TEST_F(SecurityTest, CreateMultipleUsers) {
    int id1 = security_create_user(mgr, "user1", "pass1");
    int id2 = security_create_user(mgr, "user2", "pass2");
    int id3 = security_create_user(mgr, "user3", "pass3");

    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_GT(id3, 0);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

/* ============================================================
 * 角色管理测试
 * ============================================================ */

TEST_F(SecurityTest, CreateAndDropRole) {
    /* 创建角色 */
    int role_id = security_create_role(mgr, "admin", -1);
    EXPECT_GT(role_id, 0);

    /* 重复创建同名角色应该可以（角色按ID区分） */
    int role_id2 = security_create_role(mgr, "admin", -1);
    EXPECT_GT(role_id2, 0);
    EXPECT_NE(role_id, role_id2);

    /* 删除角色 */
    EXPECT_EQ(0, security_drop_role(mgr, role_id));

    /* 删除不存在的角色应失败 */
    EXPECT_EQ(-1, security_drop_role(mgr, role_id));
}

TEST_F(SecurityTest, CreateRoleWithParent) {
    int parent_id = security_create_role(mgr, "parent_role", -1);
    EXPECT_GT(parent_id, 0);

    /* 创建子角色 */
    int child_id = security_create_role(mgr, "child_role", parent_id);
    EXPECT_GT(child_id, 0);
    EXPECT_NE(parent_id, child_id);

    /* 删除有子角色的父角色应失败 */
    EXPECT_EQ(-1, security_drop_role(mgr, parent_id));

    /* 先删除子角色 */
    EXPECT_EQ(0, security_drop_role(mgr, child_id));

    /* 现在可以删除父角色 */
    EXPECT_EQ(0, security_drop_role(mgr, parent_id));
}

TEST_F(SecurityTest, CreateRoleInvalidParent) {
    /* 父角色不存在 */
    EXPECT_EQ(-1, security_create_role(mgr, "orphan", 999));
}

TEST_F(SecurityTest, CreateRoleNullParams) {
    EXPECT_EQ(-1, security_create_role(nullptr, "admin", -1));
    EXPECT_EQ(-1, security_create_role(mgr, nullptr, -1));
}

TEST_F(SecurityTest, DropRoleInvalidParams) {
    EXPECT_EQ(-1, security_drop_role(nullptr, 1));
    EXPECT_EQ(-1, security_drop_role(mgr, -1));
    EXPECT_EQ(-1, security_drop_role(mgr, 999));  /* 不存在的角色 */
}

/* ============================================================
 * 用户-角色关联测试
 * ============================================================ */

TEST_F(SecurityTest, GrantAndRevokeRole) {
    int user_id = security_create_user(mgr, "testuser", "password");
    int role_id = security_create_role(mgr, "testrole", -1);

    EXPECT_GT(user_id, 0);
    EXPECT_GT(role_id, 0);

    /* 授予角色 */
    EXPECT_EQ(0, security_grant_role(mgr, user_id, role_id));

    /* 重复授予应成功（幂等） */
    EXPECT_EQ(0, security_grant_role(mgr, user_id, role_id));

    /* 撤销角色 */
    EXPECT_EQ(0, security_revoke_role(mgr, user_id, role_id));

    /* 撤销不存在的关联应失败 */
    EXPECT_EQ(-1, security_revoke_role(mgr, user_id, role_id));
}

TEST_F(SecurityTest, GrantRoleInvalidParams) {
    EXPECT_EQ(-1, security_grant_role(nullptr, 1, 1));
    EXPECT_EQ(-1, security_grant_role(mgr, -1, 1));
    EXPECT_EQ(-1, security_grant_role(mgr, 1, -1));
    EXPECT_EQ(-1, security_grant_role(mgr, 999, 1));  /* 用户不存在 */
    EXPECT_EQ(-1, security_grant_role(mgr, 1, 999));  /* 角色不存在 */
}

TEST_F(SecurityTest, RevokeRoleInvalidParams) {
    EXPECT_EQ(-1, security_revoke_role(nullptr, 1, 1));
    EXPECT_EQ(-1, security_revoke_role(mgr, -1, 1));
    EXPECT_EQ(-1, security_revoke_role(mgr, 1, -1));
}

/* ============================================================
 * 角色-权限关联测试
 * ============================================================ */

TEST_F(SecurityTest, GrantAndRevokePermission) {
    int role_id = security_create_role(mgr, "admin", -1);
    EXPECT_GT(role_id, 0);

    /* 授予权限 */
    EXPECT_EQ(0, security_grant_permission(mgr, role_id, PERM_READ));
    EXPECT_EQ(0, security_grant_permission(mgr, role_id, PERM_WRITE));

    /* 重复授予应成功（幂等） */
    EXPECT_EQ(0, security_grant_permission(mgr, role_id, PERM_READ));

    /* 撤销权限 */
    EXPECT_EQ(0, security_revoke_permission(mgr, role_id, PERM_READ));

    /* 撤销不存在的权限应失败 */
    EXPECT_EQ(-1, security_revoke_permission(mgr, role_id, PERM_READ));
}

TEST_F(SecurityTest, GrantPermissionInvalidParams) {
    EXPECT_EQ(-1, security_grant_permission(nullptr, 1, PERM_READ));
    EXPECT_EQ(-1, security_grant_permission(mgr, -1, PERM_READ));
    EXPECT_EQ(-1, security_grant_permission(mgr, 1, (permission_t)-1));
    EXPECT_EQ(-1, security_grant_permission(mgr, 1, PERM_MAX));
    EXPECT_EQ(-1, security_grant_permission(mgr, 999, PERM_READ));  /* 角色不存在 */
}

TEST_F(SecurityTest, RevokePermissionInvalidParams) {
    EXPECT_EQ(-1, security_revoke_permission(nullptr, 1, PERM_READ));
    EXPECT_EQ(-1, security_revoke_permission(mgr, -1, PERM_READ));
    EXPECT_EQ(-1, security_revoke_permission(mgr, 1, (permission_t)-1));
    EXPECT_EQ(-1, security_revoke_permission(mgr, 1, PERM_MAX));
}

/* ============================================================
 * 权限检查测试
 * ============================================================ */

TEST_F(SecurityTest, CheckPermission) {
    int user_id = security_create_user(mgr, "testuser", "password");
    int role_id = security_create_role(mgr, "testrole", -1);

    EXPECT_GT(user_id, 0);
    EXPECT_GT(role_id, 0);

    /* 关联用户和角色 */
    EXPECT_EQ(0, security_grant_role(mgr, user_id, role_id));

    /* 初始无权限 */
    EXPECT_FALSE(security_check_permission(mgr, user_id, PERM_READ));

    /* 授予权限后有权限 */
    EXPECT_EQ(0, security_grant_permission(mgr, role_id, PERM_READ));
    EXPECT_TRUE(security_check_permission(mgr, user_id, PERM_READ));
    EXPECT_FALSE(security_check_permission(mgr, user_id, PERM_WRITE));

    /* 撤销权限后无权限 */
    EXPECT_EQ(0, security_revoke_permission(mgr, role_id, PERM_READ));
    EXPECT_FALSE(security_check_permission(mgr, user_id, PERM_READ));
}

TEST_F(SecurityTest, CheckPermissionWithInheritance) {
    int parent_role_id = security_create_role(mgr, "parent", -1);
    int child_role_id = security_create_role(mgr, "child", parent_role_id);
    int user_id = security_create_user(mgr, "testuser", "password");

    EXPECT_GT(parent_role_id, 0);
    EXPECT_GT(child_role_id, 0);
    EXPECT_GT(user_id, 0);

    /* 给父角色授予权限 */
    EXPECT_EQ(0, security_grant_permission(mgr, parent_role_id, PERM_ADMIN));

    /* 关联用户和子角色 */
    EXPECT_EQ(0, security_grant_role(mgr, user_id, child_role_id));

    /* 子角色继承父角色权限 */
    EXPECT_TRUE(security_check_permission(mgr, user_id, PERM_ADMIN));
}

TEST_F(SecurityTest, CheckPermissionInvalidParams) {
    EXPECT_FALSE(security_check_permission(nullptr, 1, PERM_READ));
    EXPECT_FALSE(security_check_permission(mgr, -1, PERM_READ));
    EXPECT_FALSE(security_check_permission(mgr, 1, (permission_t)-1));
    EXPECT_FALSE(security_check_permission(mgr, 1, PERM_MAX));
    EXPECT_FALSE(security_check_permission(mgr, 999, PERM_READ));  /* 用户不存在 */
}

TEST_F(SecurityTest, CheckPermissionMultipleRoles) {
    int user_id = security_create_user(mgr, "testuser", "password");
    int role1_id = security_create_role(mgr, "role1", -1);
    int role2_id = security_create_role(mgr, "role2", -1);

    EXPECT_GT(user_id, 0);
    EXPECT_GT(role1_id, 0);
    EXPECT_GT(role2_id, 0);

    /* 授予不同权限给不同角色 */
    EXPECT_EQ(0, security_grant_permission(mgr, role1_id, PERM_READ));
    EXPECT_EQ(0, security_grant_permission(mgr, role2_id, PERM_WRITE));

    /* 关联用户和两个角色 */
    EXPECT_EQ(0, security_grant_role(mgr, user_id, role1_id));
    EXPECT_EQ(0, security_grant_role(mgr, user_id, role2_id));

    /* 用户同时拥有两个角色的权限 */
    EXPECT_TRUE(security_check_permission(mgr, user_id, PERM_READ));
    EXPECT_TRUE(security_check_permission(mgr, user_id, PERM_WRITE));
    EXPECT_FALSE(security_check_permission(mgr, user_id, PERM_DELETE));
}

/* ============================================================
 * ACL 管理测试
 * ============================================================ */

TEST_F(SecurityTest, CreateAndDropAcl) {
    int role_id = security_create_role(mgr, "testrole", -1);
    EXPECT_GT(role_id, 0);

    acl_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.role_id = role_id;
    entry.table_id = 1;
    entry.column_id = -1;
    entry.perm = PERM_READ;
    entry.level = ACL_TABLE;

    /* 创建 ACL */
    int acl_id = security_create_acl(mgr, &entry);
    EXPECT_GT(acl_id, 0);

    /* 删除 ACL */
    EXPECT_EQ(0, security_drop_acl(mgr, acl_id));

    /* 删除不存在的 ACL 应失败 */
    EXPECT_EQ(-1, security_drop_acl(mgr, acl_id));
}

TEST_F(SecurityTest, CreateAclInvalidRole) {
    acl_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.role_id = 999;  /* 不存在的角色 */
    entry.table_id = 1;
    entry.perm = PERM_READ;
    entry.level = ACL_TABLE;

    EXPECT_EQ(-1, security_create_acl(mgr, &entry));
}

TEST_F(SecurityTest, CreateAclNullParams) {
    EXPECT_EQ(-1, security_create_acl(nullptr, nullptr));
}

TEST_F(SecurityTest, DropAclInvalidParams) {
    EXPECT_EQ(-1, security_drop_acl(nullptr, 1));
    EXPECT_EQ(-1, security_drop_acl(mgr, -1));
}

TEST_F(SecurityTest, GetRowFilter) {
    int role_id = security_create_role(mgr, "testrole", -1);
    int user_id = security_create_user(mgr, "testuser", "password");

    EXPECT_GT(role_id, 0);
    EXPECT_GT(user_id, 0);

    /* 关联用户和角色 */
    EXPECT_EQ(0, security_grant_role(mgr, user_id, role_id));

    /* 无 ACL 时返回 NULL */
    EXPECT_EQ(nullptr, security_get_row_filter(mgr, user_id, 1));

    /* 创建行级 ACL */
    acl_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.role_id = role_id;
    entry.table_id = 1;
    entry.perm = PERM_READ;
    entry.level = ACL_ROW;
    strcpy(entry.row_filter, "department = 'sales'");

    EXPECT_GT(security_create_acl(mgr, &entry), 0);

    /* 获取行过滤器 */
    const char *filter = security_get_row_filter(mgr, user_id, 1);
    EXPECT_NE(nullptr, filter);
    EXPECT_STREQ("department = 'sales'", filter);
}

TEST_F(SecurityTest, GetRowFilterInvalidParams) {
    EXPECT_EQ(nullptr, security_get_row_filter(nullptr, 1, 1));
    EXPECT_EQ(nullptr, security_get_row_filter(mgr, -1, 1));
    EXPECT_EQ(nullptr, security_get_row_filter(mgr, 1, -1));
}

TEST_F(SecurityTest, GetAllowedColumns) {
    int role_id = security_create_role(mgr, "testrole", -1);
    int user_id = security_create_user(mgr, "testuser", "password");

    EXPECT_GT(role_id, 0);
    EXPECT_GT(user_id, 0);

    EXPECT_EQ(0, security_grant_role(mgr, user_id, role_id));

    /* 无 ACL 时返回 NULL */
    int count = -1;
    EXPECT_EQ(nullptr, security_get_allowed_columns(mgr, user_id, 1, &count));
    EXPECT_EQ(0, count);

    /* 创建列级 ACL */
    acl_entry_t entry1;
    memset(&entry1, 0, sizeof(entry1));
    entry1.role_id = role_id;
    entry1.table_id = 1;
    entry1.column_id = 1;
    entry1.perm = PERM_READ;
    entry1.level = ACL_COLUMN;

    acl_entry_t entry2;
    memset(&entry2, 0, sizeof(entry2));
    entry2.role_id = role_id;
    entry2.table_id = 1;
    entry2.column_id = 2;
    entry2.perm = PERM_READ;
    entry2.level = ACL_COLUMN;

    EXPECT_GT(security_create_acl(mgr, &entry1), 0);
    EXPECT_GT(security_create_acl(mgr, &entry2), 0);

    /* 获取允许的列 */
    int *columns = security_get_allowed_columns(mgr, user_id, 1, &count);
    ASSERT_NE(nullptr, columns);
    EXPECT_EQ(2, count);

    /* 验证列 ID */
    bool has_col1 = false, has_col2 = false;
    for (int i = 0; i < count; i++) {
        if (columns[i] == 1) has_col1 = true;
        if (columns[i] == 2) has_col2 = true;
    }
    EXPECT_TRUE(has_col1);
    EXPECT_TRUE(has_col2);

    free(columns);
}

TEST_F(SecurityTest, GetAllowedColumnsTableLevel) {
    int role_id = security_create_role(mgr, "testrole", -1);
    int user_id = security_create_user(mgr, "testuser", "password");

    EXPECT_GT(role_id, 0);
    EXPECT_GT(user_id, 0);

    EXPECT_EQ(0, security_grant_role(mgr, user_id, role_id));

    /* 创建表级 ACL */
    acl_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.role_id = role_id;
    entry.table_id = 1;
    entry.column_id = -1;
    entry.perm = PERM_READ;
    entry.level = ACL_TABLE;

    EXPECT_GT(security_create_acl(mgr, &entry), 0);

    /* 表级权限返回 NULL 表示所有列都允许 */
    int count = -1;
    int *columns = security_get_allowed_columns(mgr, user_id, 1, &count);
    EXPECT_EQ(nullptr, columns);
    EXPECT_EQ(0, count);
}

TEST_F(SecurityTest, GetAllowedColumnsInvalidParams) {
    int count = -1;
    EXPECT_EQ(nullptr, security_get_allowed_columns(nullptr, 1, 1, &count));
    EXPECT_EQ(nullptr, security_get_allowed_columns(mgr, -1, 1, &count));
    EXPECT_EQ(nullptr, security_get_allowed_columns(mgr, 1, -1, &count));
    EXPECT_EQ(nullptr, security_get_allowed_columns(mgr, 1, 1, nullptr));
}

/* ============================================================
 * 审计日志测试
 * ============================================================ */

TEST_F(SecurityTest, LogAndQueryOperation) {
    int user_id = security_create_user(mgr, "testuser", "password");
    EXPECT_GT(user_id, 0);

    /* 记录操作 */
    audit_log_t log;
    memset(&log, 0, sizeof(log));
    log.user_id = user_id;
    log.op = OP_SELECT;
    log.table_id = 1;
    log.affected_rows = 10;
    log.status = 0;
    strcpy(log.sql, "SELECT * FROM users");
    strcpy(log.client_ip, "127.0.0.1");

    EXPECT_EQ(0, security_log_operation(mgr, &log));

    /* 查询审计日志 */
    audit_log_t *results = nullptr;
    int count = 0;
    EXPECT_EQ(0, security_query_audit(mgr, user_id, 0, 0, &results, &count));

    EXPECT_NE(nullptr, results);
    EXPECT_EQ(1, count);
    EXPECT_EQ(user_id, results[0].user_id);
    EXPECT_EQ(OP_SELECT, results[0].op);
    EXPECT_EQ(1, results[0].table_id);
    EXPECT_EQ(10, results[0].affected_rows);
    EXPECT_STREQ("SELECT * FROM users", results[0].sql);
    EXPECT_STREQ("127.0.0.1", results[0].client_ip);

    free(results);
}

TEST_F(SecurityTest, QueryAuditByTimeRange) {
    int user_id = security_create_user(mgr, "testuser", "password");
    EXPECT_GT(user_id, 0);

    time_t now = time(nullptr);

    /* 记录操作 */
    audit_log_t log;
    memset(&log, 0, sizeof(log));
    log.user_id = user_id;
    log.op = OP_INSERT;
    log.table_id = 1;
    log.status = 0;
    log.timestamp = now - 100;  /* 旧日志 */

    EXPECT_EQ(0, security_log_operation(mgr, &log));

    log.timestamp = now + 100;  /* 新日志 */
    EXPECT_EQ(0, security_log_operation(mgr, &log));

    /* 查询特定时间范围 */
    audit_log_t *results = nullptr;
    int count = 0;
    EXPECT_EQ(0, security_query_audit(mgr, -1, now - 50, now + 50, &results, &count));
    EXPECT_EQ(1, count);

    free(results);
}

TEST_F(SecurityTest, QueryAuditByUser) {
    int user1_id = security_create_user(mgr, "user1", "password");
    int user2_id = security_create_user(mgr, "user2", "password");
    EXPECT_GT(user1_id, 0);
    EXPECT_GT(user2_id, 0);

    /* 记录操作 */
    audit_log_t log;
    memset(&log, 0, sizeof(log));
    log.op = OP_SELECT;
    log.table_id = 1;
    log.status = 0;

    log.user_id = user1_id;
    EXPECT_EQ(0, security_log_operation(mgr, &log));

    log.user_id = user2_id;
    EXPECT_EQ(0, security_log_operation(mgr, &log));

    /* 只查询 user1 的日志 */
    audit_log_t *results = nullptr;
    int count = 0;
    EXPECT_EQ(0, security_query_audit(mgr, user1_id, 0, 0, &results, &count));
    EXPECT_EQ(1, count);
    EXPECT_EQ(user1_id, results[0].user_id);

    free(results);
}

TEST_F(SecurityTest, QueryAuditNoResults) {
    audit_log_t *results = nullptr;
    int count = -1;
    EXPECT_EQ(0, security_query_audit(mgr, 999, 0, 0, &results, &count));
    EXPECT_EQ(nullptr, results);
    EXPECT_EQ(0, count);
}

TEST_F(SecurityTest, LogOperationInvalidParams) {
    EXPECT_EQ(-1, security_log_operation(nullptr, nullptr));
}

TEST_F(SecurityTest, QueryAuditInvalidParams) {
    audit_log_t *results = nullptr;
    int count = 0;
    EXPECT_EQ(-1, security_query_audit(nullptr, -1, 0, 0, &results, &count));
    EXPECT_EQ(-1, security_query_audit(mgr, -1, 0, 0, nullptr, &count));
    EXPECT_EQ(-1, security_query_audit(mgr, -1, 0, 0, &results, nullptr));
}

TEST_F(SecurityTest, PurgeOldLogs) {
    time_t now = time(nullptr);

    int user_id = security_create_user(mgr, "testuser", "password");
    EXPECT_GT(user_id, 0);

    /* 记录旧日志 */
    audit_log_t log;
    memset(&log, 0, sizeof(log));
    log.user_id = user_id;
    log.op = OP_SELECT;
    log.table_id = 1;
    log.status = 0;
    log.timestamp = now - 200;

    EXPECT_EQ(0, security_log_operation(mgr, &log));

    /* 记录新日志 */
    log.timestamp = now - 50;
    EXPECT_EQ(0, security_log_operation(mgr, &log));

    /* 清理旧日志 */
    EXPECT_EQ(1, security_purge_old_logs(mgr, now - 100));

    /* 验证只剩一条日志 */
    audit_log_t *results = nullptr;
    int count = 0;
    EXPECT_EQ(0, security_query_audit(mgr, -1, 0, 0, &results, &count));
    EXPECT_EQ(1, count);

    free(results);
}

TEST_F(SecurityTest, PurgeOldLogsInvalidParams) {
    EXPECT_EQ(-1, security_purge_old_logs(nullptr, time(nullptr)));
    EXPECT_EQ(-1, security_purge_old_logs(mgr, 0));
    EXPECT_EQ(-1, security_purge_old_logs(mgr, -1));
}

/* ============================================================
 * 生命周期测试
 * ============================================================ */

TEST(SecurityLifecycle, CreateAndDestroy) {
    security_mgr_t *mgr = security_manager_create();
    EXPECT_NE(nullptr, mgr);
    security_manager_destroy(mgr);
}

TEST(SecurityLifecycle, CreateDestroyNull) {
    /* 销毁 NULL 管理器应该安全 */
    security_manager_destroy(nullptr);
}

TEST(SecurityLifecycle, DoubleDestroy) {
    security_mgr_t *mgr = security_manager_create();
    ASSERT_NE(nullptr, mgr);

    security_manager_destroy(mgr);

    /* 重复销毁应该安全 */
    security_manager_destroy(mgr);
}

/* ============================================================
 * 并发安全测试（基础）
 * ============================================================ */

TEST_F(SecurityTest, ConcurrentUserCreation) {
    /* 顺序创建多个用户验证 ID 唯一性 */
    int ids[10];
    for (int i = 0; i < 10; i++) {
        char name[32];
        sprintf(name, "user%d", i);
        ids[i] = security_create_user(mgr, name, "password");
        EXPECT_GT(ids[i], 0);
    }

    /* 验证 ID 唯一性 */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(SecurityTest, ConcurrentRoleCreation) {
    /* 顺序创建多个角色验证 ID 唯一性 */
    int ids[10];
    for (int i = 0; i < 10; i++) {
        char name[32];
        sprintf(name, "role%d", i);
        ids[i] = security_create_role(mgr, name, -1);
        EXPECT_GT(ids[i], 0);
    }

    /* 验证 ID 唯一性 */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}