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
    user_t  *users;       /* 用户数组 */
    int      user_count;
    int      user_capacity;
    role_t  *roles;       /* 角色数组 */
    int      role_count;
    int      role_capacity;
    void    *acls;        /* 访问控制列表预留扩展 */
    int      acl_count;
    void    *audit_logs;  /* 审计日志预留扩展 */
    int      log_count;
    pthread_rwlock_t rwlock;
    int       next_user_id;
    int       next_role_id;
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
