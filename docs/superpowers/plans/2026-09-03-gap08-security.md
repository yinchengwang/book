# Gap#8 安全权限系统实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现完整的安全权限系统（RBAC + ACL + Audit Log）

**Architecture:** SecurityManager 统一入口，包含 RBAC/ACL/Audit 三个子模块

**Tech Stack:** C 语言，CMake 构建，GTest 单元测试，pthread 线程

## Global Constraints

- 遵循现有代码风格 (extern "C"、命名下划线分隔)
- 所有新文件加入对应 CMakeLists.txt
- 与 Gap#3 Executor Framework 集成（执行时权限检查）

---

### Task 1: SecurityManager 统一入口

**Files:**
- Create: `engineering/include/db/security/security_manager.h`
- Create: `engineering/src/db/security/security_manager.c`
- Create: `engineering/src/db/security/CMakeLists.txt`

**Interfaces:**
- Produces: security_mgr_t, security_manager_create/destroy

- [ ] **Step 1: 创建 security_manager.h**

```c
// engineering/include/db/security/security_manager.h
#ifndef DB_SECURITY_MANAGER_H
#define DB_SECURITY_MANAGER_H

#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct security_manager security_mgr_t;

/**
 * @brief 创建安全管理器
 */
security_mgr_t *security_manager_create(void);

/**
 * @brief 销毁安全管理器
 */
void security_manager_destroy(security_mgr_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* DB_SECURITY_MANAGER_H */
```

- [ ] **Step 2: 创建 security_manager.c**

```c
// engineering/src/db/security/security_manager.c
#include "db/security/security_manager.h"
#include <stdlib.h>
#include <string.h>

struct security_manager {
    // 预留扩展
    int dummy;
};

security_mgr_t *security_manager_create(void) {
    security_mgr_t *mgr = calloc(1, sizeof(security_mgr_t));
    return mgr;
}

void security_manager_destroy(security_mgr_t *mgr) {
    free(mgr);
}
```

- [ ] **Step 3: 创建 CMakeLists.txt**

- [ ] **Step 4: 提交**

---

### Task 2: RBAC 模块

**Files:**
- Modify: `engineering/include/db/security/security_manager.h`
- Modify: `engineering/src/db/security/security_manager.c`

**Interfaces:**
- Consumes: Task 1
- Produces: 用户/角色/权限管理 API

- [ ] **Step 1: 添加 RBAC 头文件**

```c
// 添加到 security_manager.h

/* 权限类型 */
typedef enum {
    PERM_READ = 0,
    PERM_WRITE,
    PERM_DELETE,
    PERM_CREATE,
    PERM_DROP,
    PERM_GRANT,
    PERM_ADMIN
} permission_t;

/* 用户 */
typedef struct {
    int user_id;
    char username[64];
    char password_hash[128];
    int *roles;
    int role_count;
    bool enabled;
} user_t;

/* 角色 */
typedef struct {
    int role_id;
    char name[64];
    int *permissions;
    int perm_count;
    int parent_role_id;
} role_t;

/* 用户管理 API */
int security_create_user(security_mgr_t *mgr, const char *username, const char *password);
int security_drop_user(security_mgr_t *mgr, int user_id);
int security_grant_role(security_mgr_t *mgr, int user_id, int role_id);
int security_revoke_role(security_mgr_t *mgr, int user_id, int role_id);
const user_t *security_get_user(security_mgr_t *mgr, int user_id);

/* 角色管理 API */
int security_create_role(security_mgr_t *mgr, const char *name, int parent_role_id);
int security_drop_role(security_mgr_t *mgr, int role_id);
int security_grant_permission(security_mgr_t *mgr, int role_id, permission_t perm);
int security_revoke_permission(security_mgr_t *mgr, int role_id, permission_t perm);

/* 权限检查 */
bool security_check_permission(security_mgr_t *mgr, int user_id, permission_t perm);
```

- [ ] **Step 2: 实现 RBAC**

- [ ] **Step 3: 提交**

---

### Task 3: Row/Column ACL

**Files:**
- Modify: `engineering/include/db/security/security_manager.h`
- Modify: `engineering/src/db/security/security_manager.c`

**Interfaces:**
- Consumes: Task 2
- Produces: ACL 管理 API

- [ ] **Step 1: 添加 ACL 类型**

```c
/* ACL 级别 */
typedef enum {
    ACL_TABLE = 0,
    ACL_COLUMN,
    ACL_ROW
} acl_level_t;

/* ACL 条目 */
typedef struct {
    int acl_id;
    int role_id;
    int table_id;
    int column_id;      /* -1 表示整表 */
    char row_filter[256];
    permission_t perm;
    acl_level_t level;
} acl_entry_t;

/* ACL 管理 API */
int security_create_acl(security_mgr_t *mgr, const acl_entry_t *entry);
int security_drop_acl(security_mgr_t *mgr, int acl_id);
const char *security_get_row_filter(security_mgr_t *mgr, int user_id, int table_id);
int *security_get_allowed_columns(security_mgr_t *mgr, int user_id, int table_id, int *count);
```

- [ ] **Step 2: 实现 ACL**

- [ ] **Step 3: 提交**

---

### Task 4: Audit Log

**Files:**
- Modify: `engineering/include/db/security/security_manager.h`
- Modify: `engineering/src/db/security/security_manager.c`

**Interfaces:**
- Consumes: Task 2
- Produces: 审计日志 API

- [ ] **Step 1: 添加审计类型**

```c
/* 操作类型 */
typedef enum {
    OP_SELECT = 0,
    OP_INSERT,
    OP_UPDATE,
    OP_DELETE,
    OP_CREATE,
    OP_DROP,
    OP_GRANT,
    OP_REVOKE,
    OP_LOGIN,
    OP_LOGOUT
} operation_type_t;

/* 审计日志 */
typedef struct {
    int64_t log_id;
    int user_id;
    operation_type_t op;
    int table_id;
    char sql[1024];
    int affected_rows;
    char client_ip[64];
    time_t timestamp;
    int status;  /* 0=成功, -1=失败 */
} audit_log_t;

/* 审计 API */
int security_log_operation(security_mgr_t *mgr, const audit_log_t *log);
int security_query_audit(security_mgr_t *mgr, int user_id, time_t start, time_t end,
                        audit_log_t **results, int *count);
int security_purge_old_logs(security_mgr_t *mgr, time_t before);
```

- [ ] **Step 2: 实现审计**

- [ ] **Step 3: 提交**

---

### Task 5: Executor 集成

**Files:**
- Create: `engineering/include/db/security/security_executor.h`
- Create: `engineering/src/db/security/security_executor.c`

**Interfaces:**
- Consumes: security_manager.h, exec_node.h
- Produces: 安全检查 ExecNode

- [ ] **Step 1: 创建安全检查算子**

```c
// engineering/include/db/security/security_executor.h
#ifndef DB_SECURITY_EXECUTOR_H
#define DB_SECURITY_EXECUTOR_H

#include "db/security/security_manager.h"
#include "db/executor/exec_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建安全检查 ExecNode
 *
 * 在执行节点外层包装安全检查：
 * - 执行前检查权限
 * - 记录审计日志
 */
ExecNode *exec_create_with_security(security_mgr_t *mgr,
                                   ExecNode *child,
                                   int user_id,
                                   permission_t required_perm);

#ifdef __cplusplus
}
#endif

#endif /* DB_SECURITY_EXECUTOR_H */
```

- [ ] **Step 2: 实现安全执行器**

```c
// security_executor.c
// 1. 检查用户权限
// 2. 执行子节点
// 3. 记录审计日志
```

- [ ] **Step 3: 提交**

---

### Task 6: 单元测试

**Files:**
- Create: `engineering/test/db/security/security_test.cpp`
- Create: `engineering/test/db/security/CMakeLists.txt`

- [ ] **Step 1: 创建测试**

```cpp
#include <gtest/gtest.h>
#include "db/security/security_manager.h"

class SecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = security_manager_create();
    }
    void TearDown() override {
        if (mgr) security_manager_destroy(mgr);
    }
    security_mgr_t *mgr = nullptr;
};

// 用户管理测试
TEST_F(SecurityTest, CreateDropUser) {
    int uid = security_create_user(mgr, "alice", "pass123");
    EXPECT_GE(uid, 0);
    EXPECT_TRUE(security_drop_user(mgr, uid));
}

// 角色管理测试
TEST_F(SecurityTest, CreateRoleAndGrant) {
    int rid = security_create_role(mgr, "admin", -1);
    EXPECT_GE(rid, 0);
    EXPECT_EQ(security_grant_permission(mgr, rid, PERM_ADMIN), 0);
}

// 权限检查测试
TEST_F(SecurityTest, CheckPermission) {
    int uid = security_create_user(mgr, "bob", "pass");
    int rid = security_create_role(mgr, "reader", -1);
    security_grant_permission(mgr, rid, PERM_READ);
    security_grant_role(mgr, uid, rid);

    EXPECT_TRUE(security_check_permission(mgr, uid, PERM_READ));
    EXPECT_FALSE(security_check_permission(mgr, uid, PERM_WRITE));
}

// ACL 测试
TEST_F(SecurityTest, ACL) {
    int rid = security_create_role(mgr, "limited", -1);
    acl_entry_t entry = {.role_id = rid, .table_id = 1, .column_id = -1};
    EXPECT_GE(security_create_acl(mgr, &entry), 0);
}

// 审计日志测试
TEST_F(SecurityTest, AuditLog) {
    audit_log_t log = {
        .user_id = 1,
        .op = OP_SELECT,
        .table_id = 1,
        .status = 0
    };
    EXPECT_GE(security_log_operation(mgr, &log), 0);
}
```

- [ ] **Step 2: 提交**

---

## 任务依赖关系

```
Task 1: SecurityManager  ← 基础
Task 2: RBAC            ← 依赖 Task 1
Task 3: ACL             ← 依赖 Task 2
Task 4: Audit           ← 依赖 Task 2
Task 5: Executor 集成    ← 依赖 Task 2-4
Task 6: 单元测试        ← 依赖 Task 1-5
```

## 成功标准

- [ ] Task 1: SecurityManager 统一入口
- [ ] Task 2: RBAC 用户/角色/权限管理
- [ ] Task 3: Row/Column ACL 访问控制
- [ ] Task 4: Audit Log 审计日志
- [ ] Task 5: Executor 集成
- [ ] Task 6: 单元测试全部通过
