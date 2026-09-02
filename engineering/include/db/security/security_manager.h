/**
 * @file include/db/security/security_manager.h
 * @brief 安全管理系统统一入口
 */
#ifndef DB_SECURITY_SECURITY_MANAGER_H
#define DB_SECURITY_SECURITY_MANAGER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

typedef struct security_manager security_mgr_t;

/* ============================================================
 * RBAC 类型定义
 * ============================================================ */

/** 权限类型 */
typedef enum {
    PERM_READ = 0,
    PERM_WRITE,
    PERM_DELETE,
    PERM_CREATE,
    PERM_DROP,
    PERM_GRANT,
    PERM_ADMIN,
    PERM_MAX
} permission_t;

/** 用户结构 */
typedef struct {
    int      user_id;
    char     username[64];
    char     password_hash[128];
    int     *roles;
    int      role_count;
    bool     enabled;
} user_t;

/** 角色结构 */
typedef struct {
    int      role_id;
    char     name[64];
    int     *permissions;
    int      perm_count;
    int      parent_role_id;
} role_t;

/* ============================================================
 * 生命周期管理
 * ============================================================ */

/**
 * @brief 创建安全管理器实例
 * @return 成功返回管理器指针，失败返回 NULL
 */
security_mgr_t *security_manager_create(void);

/**
 * @brief 销毁安全管理器实例
 * @param mgr 管理器指针
 */
void security_manager_destroy(security_mgr_t *mgr);

/* ============================================================
 * 用户管理
 * ============================================================ */

/**
 * @brief 创建新用户
 * @param mgr 管理器指针
 * @param username 用户名
 * @param password 密码
 * @return 成功返回用户ID，失败返回 -1
 */
int security_create_user(security_mgr_t *mgr, const char *username, const char *password);

/**
 * @brief 删除用户
 * @param mgr 管理器指针
 * @param user_id 用户ID
 * @return 成功返回 0，失败返回 -1
 */
int security_drop_user(security_mgr_t *mgr, int user_id);

/* ============================================================
 * 角色管理
 * ============================================================ */

/**
 * @brief 创建新角色
 * @param mgr 管理器指针
 * @param name 角色名
 * @param parent_role_id 父角色ID，-1 表示无父角色
 * @return 成功返回角色ID，失败返回 -1
 */
int security_create_role(security_mgr_t *mgr, const char *name, int parent_role_id);

/**
 * @brief 删除角色
 * @param mgr 管理器指针
 * @param role_id 角色ID
 * @return 成功返回 0，失败返回 -1
 */
int security_drop_role(security_mgr_t *mgr, int role_id);

/* ============================================================
 * 用户-角色关联
 * ============================================================ */

/**
 * @brief 为用户授予角色
 * @param mgr 管理器指针
 * @param user_id 用户ID
 * @param role_id 角色ID
 * @return 成功返回 0，失败返回 -1
 */
int security_grant_role(security_mgr_t *mgr, int user_id, int role_id);

/**
 * @brief 撤销用户的角色
 * @param mgr 管理器指针
 * @param user_id 用户ID
 * @param role_id 角色ID
 * @return 成功返回 0，失败返回 -1
 */
int security_revoke_role(security_mgr_t *mgr, int user_id, int role_id);

/* ============================================================
 * 角色-权限关联
 * ============================================================ */

/**
 * @brief 为角色授予权限
 * @param mgr 管理器指针
 * @param role_id 角色ID
 * @param perm 权限类型
 * @return 成功返回 0，失败返回 -1
 */
int security_grant_permission(security_mgr_t *mgr, int role_id, permission_t perm);

/**
 * @brief 撤销角色的权限
 * @param mgr 管理器指针
 * @param role_id 角色ID
 * @param perm 权限类型
 * @return 成功返回 0，失败返回 -1
 */
int security_revoke_permission(security_mgr_t *mgr, int role_id, permission_t perm);

/* ============================================================
 * 权限检查
 * ============================================================ */

/**
 * @brief 检查用户是否具有指定权限
 * @param mgr 管理器指针
 * @param user_id 用户ID
 * @param perm 权限类型
 * @return 有权限返回 true，否则返回 false
 */
bool security_check_permission(security_mgr_t *mgr, int user_id, permission_t perm);

#ifdef __cplusplus
}
#endif

#endif /* DB_SECURITY_SECURITY_MANAGER_H */
