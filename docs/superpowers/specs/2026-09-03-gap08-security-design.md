# Gap#8 安全权限系统设计

> **日期:** 2026-09-03
> **状态:** 待批准

## 1. 目标

实现完整的安全权限系统，支持：
- RBAC (基于角色的访问控制)
- Row/Column ACL (行级/列级访问控制)
- Audit Log (审计日志)

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                     SecurityManager                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │     RBAC     │  │  Row/Col ACL │  │  AuditLog    │   │
│  │ (用户/角色)  │  │  (访问控制)  │  │  (审计日志) │   │
│  └──────────────┘  └──────────────┘  └──────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## 3. Phase 1: RBAC 模块

### 3.1 数据模型

```c
typedef enum {
    PERM_READ = 0,
    PERM_WRITE,
    PERM_DELETE,
    PERM_CREATE,
    PERM_DROP,
    PERM_GRANT,
    PERM_ADMIN
} permission_t;

typedef struct role {
    int role_id;
    char name[MAX_ROLE_NAME_LEN];
    int *permissions;
    int perm_count;
    int parent_role_id;  // 角色继承
} role_t;

typedef struct user {
    int user_id;
    char username[MAX_USERNAME_LEN];
    char password_hash[64];
    int *roles;
    int role_count;
    bool enabled;
} user_t;
```

### 3.2 API

```c
// 用户管理
int security_create_user(security_mgr_t *mgr, const char *username, const char *password);
int security_drop_user(security_mgr_t *mgr, int user_id);
int security_grant_role(security_mgr_t *mgr, int user_id, int role_id);
int security_revoke_role(security_mgr_t *mgr, int user_id, int role_id);

// 角色管理
int security_create_role(security_mgr_t *mgr, const char *name);
int security_drop_role(security_mgr_t *mgr, int role_id);
int security_grant_permission(security_mgr_t *mgr, int role_id, permission_t perm);
int security_revoke_permission(security_mgr_t *mgr, int role_id, permission_t perm);

// 权限检查
bool security_check_permission(security_mgr_t *mgr, int user_id, permission_t perm);
bool security_check_table_access(security_mgr_t *mgr, int user_id, int table_id, permission_t perm);
```

## 4. Phase 2: Row/Column ACL

### 4.1 数据模型

```c
typedef enum {
    ACL_TABLE = 0,
    ACL_COLUMN,
    ACL_ROW
} acl_level_t;

typedef struct acl_entry {
    int acl_id;
    int role_id;
    int table_id;
    int column_id;      // -1 表示整表
    char row_filter[256];  // 行过滤条件
    permission_t perm;
    acl_level_t level;
} acl_entry_t;
```

### 4.2 API

```c
// ACL 管理
int security_create_acl(security_mgr_t *mgr, const acl_entry_t *entry);
int security_drop_acl(security_mgr_t *mgr, int acl_id);

// 行级过滤
const char *security_get_row_filter(security_mgr_t *mgr, int user_id, int table_id);

// 列级过滤
int *security_get_allowed_columns(security_mgr_t *mgr, int user_id, int table_id, int *count);
```

## 5. Phase 3: Audit Log

### 5.1 数据模型

```c
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

typedef struct audit_log {
    int64_t log_id;
    int user_id;
    operation_type_t op;
    int table_id;
    char sql[MAX_SQL_LEN];
    int affected_rows;
    char client_ip[64];
    time_t timestamp;
    int status;  // 0=成功, -1=失败
} audit_log_t;
```

### 5.2 API

```c
// 审计记录
int security_log_operation(security_mgr_t *mgr, const audit_log_t *log);
int security_query_audit(security_mgr_t *mgr, int user_id, time_t start, time_t end,
                        audit_log_t **results, int *count);

// 日志管理
int security_purge_old_logs(security_mgr_t *mgr, time_t before);
```

## 6. SecurityManager 统一入口

```c
typedef struct security_manager {
    // RBAC
    user_t *users;
    role_t *roles;
    int user_count;
    int role_count;

    // ACL
    acl_entry_t *acls;
    int acl_count;

    // Audit
    audit_log_t *logs;
    int log_count;

    pthread_rwlock_t rwlock;
} security_mgr_t;
```

## 7. 文件结构

```
engineering/
├── include/db/security/
│   ├── security_manager.h      # 统一入口
│   ├── security_rbac.h         # RBAC
│   ├── security_acl.h          # ACL
│   └── security_audit.h        # 审计
├── src/db/security/
│   ├── security_manager.c
│   ├── security_rbac.c
│   ├── security_acl.c
│   └── security_audit.c
└── test/db/security/
    └── security_test.cpp
```

## 8. 实现顺序

| Phase | 内容 | 依赖 |
|-------|------|------|
| 1 | RBAC 模块 | 无 |
| 2 | Row/Column ACL | Phase 1 |
| 3 | Audit Log | Phase 1 |

## 9. 成功标准

- [ ] RBAC: 用户/角色/权限管理完整
- [ ] ACL: 行级/列级访问控制工作
- [ ] Audit: 操作审计日志记录和查询
- [ ] 与 Executor 集成（执行时权限检查）
- [ ] 单元测试覆盖
