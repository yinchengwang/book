/**
 * @file src/db/security/security_manager.c
 * @brief 安全管理系统统一入口实现
 */

#include <db/security/security_manager.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 结构体定义
 * ============================================================ */

struct security_manager {
    user_t       *users;       /* 用户数组 */
    int           user_count;
    int           user_capacity;
    role_t       *roles;       /* 角色数组 */
    int           role_count;
    int           role_capacity;
    acl_entry_t   *acls;        /* ACL 条目数组 */
    int           acl_count;
    int           acl_capacity;
    audit_log_t  *audit_logs;  /* 审计日志数组 */
    int           audit_count;
    int           audit_capacity;
    int64_t       next_audit_id;
    pthread_rwlock_t rwlock;
    int           next_user_id;
    int           next_role_id;
    int           next_acl_id;
};

/* ============================================================
 * 工具函数
 * ============================================================ */

static int hash_password(const char *password, char *hash_out, size_t hash_size)
{
    if (password == NULL || hash_out == NULL || hash_size < 128) {
        return -1;
    }

    /* 简单的 hash 实现：使用 DJB2 */
    unsigned long hash = 5381;
    const unsigned char *p = (const unsigned char *)password;
    int c;

    while ((c = *p++)) {
        hash = ((hash << 5) + hash) + c;
    }

    snprintf(hash_out, hash_size, "%016lx", hash);
    return 0;
}

static int ensure_user_capacity(security_mgr_t *mgr)
{
    if (mgr->user_count < mgr->user_capacity) {
        return 0;
    }

    int new_capacity = mgr->user_capacity == 0 ? 16 : mgr->user_capacity * 2;
    user_t *new_users = (user_t *)realloc(mgr->users, new_capacity * sizeof(user_t));
    if (new_users == NULL) {
        return -1;
    }

    mgr->users = new_users;
    mgr->user_capacity = new_capacity;
    return 0;
}

static int ensure_role_capacity(security_mgr_t *mgr)
{
    if (mgr->role_count < mgr->role_capacity) {
        return 0;
    }

    int new_capacity = mgr->role_capacity == 0 ? 16 : mgr->role_capacity * 2;
    role_t *new_roles = (role_t *)realloc(mgr->roles, new_capacity * sizeof(role_t));
    if (new_roles == NULL) {
        return -1;
    }

    mgr->roles = new_roles;
    mgr->role_capacity = new_capacity;
    return 0;
}

static user_t *find_user_by_id(security_mgr_t *mgr, int user_id)
{
    for (int i = 0; i < mgr->user_count; i++) {
        if (mgr->users[i].user_id == user_id) {
            return &mgr->users[i];
        }
    }
    return NULL;
}

static user_t *find_user_by_name(security_mgr_t *mgr, const char *username)
{
    for (int i = 0; i < mgr->user_count; i++) {
        if (strcmp(mgr->users[i].username, username) == 0) {
            return &mgr->users[i];
        }
    }
    return NULL;
}

static role_t *find_role_by_id(security_mgr_t *mgr, int role_id)
{
    for (int i = 0; i < mgr->role_count; i++) {
        if (mgr->roles[i].role_id == role_id) {
            return &mgr->roles[i];
        }
    }
    return NULL;
}

static int add_permission_to_role(role_t *role, permission_t perm)
{
    /* 检查是否已有此权限 */
    for (int i = 0; i < role->perm_count; i++) {
        if (role->permissions[i] == (int)perm) {
            return 0;
        }
    }

    /* 分配新的权限数组空间 */
    int *new_perms = (int *)realloc(role->permissions, (role->perm_count + 1) * sizeof(int));
    if (new_perms == NULL) {
        return -1;
    }

    role->permissions = new_perms;
    role->permissions[role->perm_count++] = (int)perm;
    return 0;
}

static int remove_permission_from_role(role_t *role, permission_t perm)
{
    for (int i = 0; i < role->perm_count; i++) {
        if (role->permissions[i] == (int)perm) {
            /* 移动后面的元素 */
            for (int j = i; j < role->perm_count - 1; j++) {
                role->permissions[j] = role->permissions[j + 1];
            }
            role->perm_count--;
            return 0;
        }
    }
    return -1;
}

/* ============================================================
 * 生命周期管理
 * ============================================================ */

security_mgr_t *security_manager_create(void)
{
    security_mgr_t *mgr = (security_mgr_t *)malloc(sizeof(security_mgr_t));
    if (mgr == NULL) {
        return NULL;
    }

    memset(mgr, 0, sizeof(security_mgr_t));

    if (pthread_rwlock_init(&mgr->rwlock, NULL) != 0) {
        free(mgr);
        return NULL;
    }

    mgr->next_user_id = 1;
    mgr->next_role_id = 1;
    mgr->next_acl_id = 1;
    mgr->next_audit_id = 1;

    return mgr;
}

void security_manager_destroy(security_mgr_t *mgr)
{
    if (mgr == NULL) {
        return;
    }

    /* 释放用户资源 */
    for (int i = 0; i < mgr->user_count; i++) {
        free(mgr->users[i].roles);
    }
    free(mgr->users);

    /* 释放角色资源 */
    for (int i = 0; i < mgr->role_count; i++) {
        free(mgr->roles[i].permissions);
    }
    free(mgr->roles);

    /* 释放 ACL 资源 */
    free(mgr->acls);

    /* 释放审计日志资源 */
    free(mgr->audit_logs);

    pthread_rwlock_destroy(&mgr->rwlock);
    free(mgr);
}

/* ============================================================
 * 用户管理
 * ============================================================ */

int security_create_user(security_mgr_t *mgr, const char *username, const char *password)
{
    if (mgr == NULL || username == NULL || password == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    /* 检查用户名是否已存在 */
    if (find_user_by_name(mgr, username) != NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    /* 确保容量 */
    if (ensure_user_capacity(mgr) != 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    user_t *user = &mgr->users[mgr->user_count];
    memset(user, 0, sizeof(user_t));

    user->user_id = mgr->next_user_id++;
    strncpy(user->username, username, sizeof(user->username) - 1);
    user->username[sizeof(user->username) - 1] = '\0';

    if (hash_password(password, user->password_hash, sizeof(user->password_hash)) != 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    user->enabled = true;
    user->roles = NULL;
    user->role_count = 0;

    mgr->user_count++;
    pthread_rwlock_unlock(&mgr->rwlock);

    return user->user_id;
}

int security_drop_user(security_mgr_t *mgr, int user_id)
{
    if (mgr == NULL || user_id < 0) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    int found_index = -1;
    for (int i = 0; i < mgr->user_count; i++) {
        if (mgr->users[i].user_id == user_id) {
            found_index = i;
            break;
        }
    }

    if (found_index < 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    /* 释放用户角色数组 */
    free(mgr->users[found_index].roles);

    /* 移动后面的用户 */
    for (int i = found_index; i < mgr->user_count - 1; i++) {
        mgr->users[i] = mgr->users[i + 1];
    }

    mgr->user_count--;
    pthread_rwlock_unlock(&mgr->rwlock);

    return 0;
}

/* ============================================================
 * 角色管理
 * ============================================================ */

int security_create_role(security_mgr_t *mgr, const char *name, int parent_role_id)
{
    if (mgr == NULL || name == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    /* 检查父角色是否存在 */
    if (parent_role_id >= 0) {
        role_t *parent = find_role_by_id(mgr, parent_role_id);
        if (parent == NULL) {
            pthread_rwlock_unlock(&mgr->rwlock);
            return -1;
        }
    }

    /* 确保容量 */
    if (ensure_role_capacity(mgr) != 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    role_t *role = &mgr->roles[mgr->role_count];
    memset(role, 0, sizeof(role_t));

    role->role_id = mgr->next_role_id++;
    strncpy(role->name, name, sizeof(role->name) - 1);
    role->name[sizeof(role->name) - 1] = '\0';
    role->parent_role_id = parent_role_id;
    role->permissions = NULL;
    role->perm_count = 0;

    mgr->role_count++;
    pthread_rwlock_unlock(&mgr->rwlock);

    return role->role_id;
}

int security_drop_role(security_mgr_t *mgr, int role_id)
{
    if (mgr == NULL || role_id < 0) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    int found_index = -1;
    for (int i = 0; i < mgr->role_count; i++) {
        if (mgr->roles[i].role_id == role_id) {
            found_index = i;
            break;
        }
    }

    if (found_index < 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    /* 检查是否有其他角色以此角色为父角色 */
    for (int i = 0; i < mgr->role_count; i++) {
        if (mgr->roles[i].parent_role_id == role_id) {
            pthread_rwlock_unlock(&mgr->rwlock);
            return -1;
        }
    }

    /* 释放角色权限数组 */
    free(mgr->roles[found_index].permissions);

    /* 移动后面的角色 */
    for (int i = found_index; i < mgr->role_count - 1; i++) {
        mgr->roles[i] = mgr->roles[i + 1];
    }

    mgr->role_count--;
    pthread_rwlock_unlock(&mgr->rwlock);

    return 0;
}

/* ============================================================
 * 用户-角色关联
 * ============================================================ */

int security_grant_role(security_mgr_t *mgr, int user_id, int role_id)
{
    if (mgr == NULL || user_id < 0 || role_id < 0) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    user_t *user = find_user_by_id(mgr, user_id);
    if (user == NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    role_t *role = find_role_by_id(mgr, role_id);
    if (role == NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    /* 检查用户是否已有此角色 */
    for (int i = 0; i < user->role_count; i++) {
        if (user->roles[i] == role_id) {
            pthread_rwlock_unlock(&mgr->rwlock);
            return 0;
        }
    }

    /* 分配新的角色数组空间 */
    int *new_roles = (int *)realloc(user->roles, (user->role_count + 1) * sizeof(int));
    if (new_roles == NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    user->roles = new_roles;
    user->roles[user->role_count++] = role_id;

    pthread_rwlock_unlock(&mgr->rwlock);
    return 0;
}

int security_revoke_role(security_mgr_t *mgr, int user_id, int role_id)
{
    if (mgr == NULL || user_id < 0 || role_id < 0) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    user_t *user = find_user_by_id(mgr, user_id);
    if (user == NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    /* 查找并移除角色 */
    for (int i = 0; i < user->role_count; i++) {
        if (user->roles[i] == role_id) {
            /* 移动后面的元素 */
            for (int j = i; j < user->role_count - 1; j++) {
                user->roles[j] = user->roles[j + 1];
            }
            user->role_count--;
            pthread_rwlock_unlock(&mgr->rwlock);
            return 0;
        }
    }

    pthread_rwlock_unlock(&mgr->rwlock);
    return -1;
}

/* ============================================================
 * 角色-权限关联
 * ============================================================ */

int security_grant_permission(security_mgr_t *mgr, int role_id, permission_t perm)
{
    if (mgr == NULL || role_id < 0 || perm < 0 || perm >= PERM_MAX) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    role_t *role = find_role_by_id(mgr, role_id);
    if (role == NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    int result = add_permission_to_role(role, perm);
    pthread_rwlock_unlock(&mgr->rwlock);

    return result;
}

int security_revoke_permission(security_mgr_t *mgr, int role_id, permission_t perm)
{
    if (mgr == NULL || role_id < 0 || perm < 0 || perm >= PERM_MAX) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    role_t *role = find_role_by_id(mgr, role_id);
    if (role == NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    int result = remove_permission_from_role(role, perm);
    pthread_rwlock_unlock(&mgr->rwlock);

    return result;
}

/* ============================================================
 * 权限检查
 * ============================================================ */

static bool check_role_has_permission(security_mgr_t *mgr, role_t *role, permission_t perm)
{
    if (role == NULL) {
        return false;
    }

    /* 检查当前角色的权限 */
    for (int i = 0; i < role->perm_count; i++) {
        if (role->permissions[i] == (int)perm) {
            return true;
        }
    }

    /* 递归检查父角色 */
    if (role->parent_role_id >= 0) {
        role_t *parent = find_role_by_id(mgr, role->parent_role_id);
        if (parent != NULL) {
            return check_role_has_permission(mgr, parent, perm);
        }
    }

    return false;
}

bool security_check_permission(security_mgr_t *mgr, int user_id, permission_t perm)
{
    if (mgr == NULL || user_id < 0 || perm < 0 || perm >= PERM_MAX) {
        return false;
    }

    pthread_rwlock_rdlock(&mgr->rwlock);

    user_t *user = find_user_by_id(mgr, user_id);
    if (user == NULL || !user->enabled) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return false;
    }

    /* 检查用户的每个角色是否具有权限 */
    for (int i = 0; i < user->role_count; i++) {
        role_t *role = find_role_by_id(mgr, user->roles[i]);
        if (role != NULL && check_role_has_permission(mgr, role, perm)) {
            pthread_rwlock_unlock(&mgr->rwlock);
            return true;
        }
    }

    pthread_rwlock_unlock(&mgr->rwlock);
    return false;
}

/* ============================================================
 * ACL 管理
 * ============================================================ */

static int ensure_acl_capacity(security_mgr_t *mgr)
{
    if (mgr->acl_count < mgr->acl_capacity) {
        return 0;
    }

    int new_capacity = mgr->acl_capacity == 0 ? 16 : mgr->acl_capacity * 2;
    acl_entry_t *new_acls = (acl_entry_t *)realloc(mgr->acls, new_capacity * sizeof(acl_entry_t));
    if (new_acls == NULL) {
        return -1;
    }

    mgr->acls = new_acls;
    mgr->acl_capacity = new_capacity;
    return 0;
}

static acl_entry_t *find_acl_by_id(security_mgr_t *mgr, int acl_id)
{
    for (int i = 0; i < mgr->acl_count; i++) {
        if (mgr->acls[i].acl_id == acl_id) {
            return &mgr->acls[i];
        }
    }
    return NULL;
}

static int get_user_roles_internal(security_mgr_t *mgr, int user_id, int **roles_out, int *count_out)
{
    user_t *user = find_user_by_id(mgr, user_id);
    if (user == NULL || !user->enabled) {
        return -1;
    }

    if (user->role_count == 0) {
        *roles_out = NULL;
        *count_out = 0;
        return 0;
    }

    int *roles = (int *)malloc(user->role_count * sizeof(int));
    if (roles == NULL) {
        return -1;
    }

    memcpy(roles, user->roles, user->role_count * sizeof(int));
    *roles_out = roles;
    *count_out = user->role_count;
    return 0;
}

int security_create_acl(security_mgr_t *mgr, const acl_entry_t *entry)
{
    if (mgr == NULL || entry == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    /* 检查角色是否存在 */
    if (find_role_by_id(mgr, entry->role_id) == NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    /* 确保容量 */
    if (ensure_acl_capacity(mgr) != 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    acl_entry_t *acl = &mgr->acls[mgr->acl_count];
    memset(acl, 0, sizeof(acl_entry_t));

    acl->acl_id = mgr->next_acl_id++;
    acl->role_id = entry->role_id;
    acl->table_id = entry->table_id;
    acl->column_id = entry->column_id;
    acl->perm = entry->perm;
    acl->level = entry->level;
    if (entry->row_filter != NULL) {
        strncpy(acl->row_filter, entry->row_filter, sizeof(acl->row_filter) - 1);
        acl->row_filter[sizeof(acl->row_filter) - 1] = '\0';
    }

    mgr->acl_count++;
    pthread_rwlock_unlock(&mgr->rwlock);

    return acl->acl_id;
}

int security_drop_acl(security_mgr_t *mgr, int acl_id)
{
    if (mgr == NULL || acl_id < 0) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    int found_index = -1;
    for (int i = 0; i < mgr->acl_count; i++) {
        if (mgr->acls[i].acl_id == acl_id) {
            found_index = i;
            break;
        }
    }

    if (found_index < 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    /* 移动后面的 ACL 条目 */
    for (int i = found_index; i < mgr->acl_count - 1; i++) {
        mgr->acls[i] = mgr->acls[i + 1];
    }

    mgr->acl_count--;
    pthread_rwlock_unlock(&mgr->rwlock);

    return 0;
}

const char *security_get_row_filter(security_mgr_t *mgr, int user_id, int table_id)
{
    if (mgr == NULL || user_id < 0 || table_id < 0) {
        return NULL;
    }

    pthread_rwlock_rdlock(&mgr->rwlock);

    /* 获取用户的所有角色 */
    int *roles = NULL;
    int role_count = 0;
    if (get_user_roles_internal(mgr, user_id, &roles, &role_count) != 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return NULL;
    }

    /* 查找匹配的行级 ACL */
    static char result[256];
    result[0] = '\0';

    for (int i = 0; i < mgr->acl_count; i++) {
        acl_entry_t *acl = &mgr->acls[i];
        if (acl->level != ACL_ROW) {
            continue;
        }
        if (acl->table_id != table_id) {
            continue;
        }

        /* 检查用户是否有此 ACL 对应的角色 */
        bool has_role = false;
        for (int j = 0; j < role_count; j++) {
            if (roles[j] == acl->role_id) {
                has_role = true;
                break;
            }
        }

        if (has_role && acl->row_filter[0] != '\0') {
            strncpy(result, acl->row_filter, sizeof(result) - 1);
            result[sizeof(result) - 1] = '\0';
            break;
        }
    }

    free(roles);
    pthread_rwlock_unlock(&mgr->rwlock);

    return result[0] != '\0' ? result : NULL;
}

int *security_get_allowed_columns(security_mgr_t *mgr, int user_id, int table_id, int *count)
{
    if (mgr == NULL || user_id < 0 || table_id < 0 || count == NULL) {
        return NULL;
    }

    *count = 0;
    pthread_rwlock_rdlock(&mgr->rwlock);

    /* 获取用户的所有角色 */
    int *roles = NULL;
    int role_count = 0;
    if (get_user_roles_internal(mgr, user_id, &roles, &role_count) != 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return NULL;
    }

    /* 先收集所有允许的列 ID（去重） */
    int *columns = NULL;
    int columns_capacity = 0;
    int columns_count = 0;

    for (int i = 0; i < mgr->acl_count; i++) {
        acl_entry_t *acl = &mgr->acls[i];
        if (acl->level != ACL_COLUMN && acl->level != ACL_TABLE) {
            continue;
        }
        if (acl->table_id != table_id) {
            continue;
        }

        /* 检查用户是否有此 ACL 对应的角色 */
        bool has_role = false;
        for (int j = 0; j < role_count; j++) {
            if (roles[j] == acl->role_id) {
                has_role = true;
                break;
            }
        }

        if (!has_role) {
            continue;
        }

        if (acl->level == ACL_TABLE) {
            /* 表级权限意味着可以访问所有列（用 column_id = -1 表示） */
            free(columns);
            free(roles);
            *count = 0;
            pthread_rwlock_unlock(&mgr->rwlock);
            return NULL;  /* 返回 NULL 表示有表级权限，所有列都允许 */
        }

        if (acl->column_id < 0) {
            continue;
        }

        /* 检查是否已存在 */
        bool found = false;
        for (int k = 0; k < columns_count; k++) {
            if (columns[k] == acl->column_id) {
                found = true;
                break;
            }
        }

        if (!found) {
            if (columns_count >= columns_capacity) {
                int new_capacity = columns_capacity == 0 ? 16 : columns_capacity * 2;
                int *new_cols = (int *)realloc(columns, new_capacity * sizeof(int));
                if (new_cols == NULL) {
                    free(columns);
                    free(roles);
                    pthread_rwlock_unlock(&mgr->rwlock);
                    return NULL;
                }
                columns = new_cols;
                columns_capacity = new_capacity;
            }
            columns[columns_count++] = acl->column_id;
        }
    }

    free(roles);
    pthread_rwlock_unlock(&mgr->rwlock);

    *count = columns_count;
    return columns;
}

/* ============================================================
 * Audit Log 管理
 * ============================================================ */

static int ensure_audit_capacity(security_mgr_t *mgr)
{
    if (mgr->audit_count < mgr->audit_capacity) {
        return 0;
    }

    int new_capacity = mgr->audit_capacity == 0 ? 64 : mgr->audit_capacity * 2;
    audit_log_t *new_logs = (audit_log_t *)realloc(mgr->audit_logs, new_capacity * sizeof(audit_log_t));
    if (new_logs == NULL) {
        return -1;
    }

    mgr->audit_logs = new_logs;
    mgr->audit_capacity = new_capacity;
    return 0;
}

int security_log_operation(security_mgr_t *mgr, const audit_log_t *log)
{
    if (mgr == NULL || log == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    /* 确保容量 */
    if (ensure_audit_capacity(mgr) != 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    audit_log_t *dst = &mgr->audit_logs[mgr->audit_count];
    memset(dst, 0, sizeof(audit_log_t));

    dst->log_id = mgr->next_audit_id++;
    dst->user_id = log->user_id;
    dst->op = log->op;
    dst->table_id = log->table_id;
    dst->affected_rows = log->affected_rows;
    dst->timestamp = log->timestamp != 0 ? log->timestamp : time(NULL);
    dst->status = log->status;

    if (log->sql != NULL) {
        strncpy(dst->sql, log->sql, sizeof(dst->sql) - 1);
        dst->sql[sizeof(dst->sql) - 1] = '\0';
    }

    if (log->client_ip != NULL) {
        strncpy(dst->client_ip, log->client_ip, sizeof(dst->client_ip) - 1);
        dst->client_ip[sizeof(dst->client_ip) - 1] = '\0';
    }

    mgr->audit_count++;
    pthread_rwlock_unlock(&mgr->rwlock);

    return 0;
}

int security_query_audit(security_mgr_t *mgr, int user_id, time_t start, time_t end,
                        audit_log_t **results, int *count)
{
    if (mgr == NULL || results == NULL || count == NULL) {
        return -1;
    }

    *results = NULL;
    *count = 0;

    pthread_rwlock_rdlock(&mgr->rwlock);

    /* 先统计匹配的日志数量 */
    int matched = 0;
    for (int i = 0; i < mgr->audit_count; i++) {
        audit_log_t *log = &mgr->audit_logs[i];

        if (user_id >= 0 && log->user_id != user_id) {
            continue;
        }

        if (start > 0 && log->timestamp < start) {
            continue;
        }

        if (end > 0 && log->timestamp > end) {
            continue;
        }

        matched++;
    }

    if (matched == 0) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return 0;
    }

    /* 分配结果数组 */
    audit_log_t *res = (audit_log_t *)malloc(matched * sizeof(audit_log_t));
    if (res == NULL) {
        pthread_rwlock_unlock(&mgr->rwlock);
        return -1;
    }

    /* 填充结果 */
    int idx = 0;
    for (int i = 0; i < mgr->audit_count; i++) {
        audit_log_t *log = &mgr->audit_logs[i];

        if (user_id >= 0 && log->user_id != user_id) {
            continue;
        }

        if (start > 0 && log->timestamp < start) {
            continue;
        }

        if (end > 0 && log->timestamp > end) {
            continue;
        }

        memcpy(&res[idx++], log, sizeof(audit_log_t));
    }

    pthread_rwlock_unlock(&mgr->rwlock);

    *results = res;
    *count = matched;
    return 0;
}

int security_purge_old_logs(security_mgr_t *mgr, time_t before)
{
    if (mgr == NULL || before <= 0) {
        return -1;
    }

    pthread_rwlock_wrlock(&mgr->rwlock);

    int removed = 0;
    int i = 0;
    while (i < mgr->audit_count) {
        if (mgr->audit_logs[i].timestamp < before) {
            /* 移动后面的日志 */
            for (int j = i; j < mgr->audit_count - 1; j++) {
                mgr->audit_logs[j] = mgr->audit_logs[j + 1];
            }
            mgr->audit_count--;
            removed++;
        } else {
            i++;
        }
    }

    pthread_rwlock_unlock(&mgr->rwlock);

    return removed;
}
