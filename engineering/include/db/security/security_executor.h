/**
 * @file include/db/security/security_executor.h
 * @brief 安全执行器 - 将安全检查集成到执行节点
 */
#ifndef DB_SECURITY_SECURITY_EXECUTOR_H
#define DB_SECURITY_SECURITY_EXECUTOR_H

#include <db/security/security_manager.h>
#include <db/executor/exec_node.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief 安全检查回调函数类型
 * @param user_id 用户ID
 * @param perm 权限类型
 * @param arg 回调参数
 */
typedef void (*security_check_callback_t)(int user_id, permission_t perm, void *arg);

/**
 * @brief 安全执行节点状态
 */
typedef struct {
    ExecNode              *child;           /**< 子执行节点 */
    security_mgr_t        *security_mgr;    /**< 安全管理器 */
    int                    user_id;         /**< 当前用户ID */
    permission_t           required_perm;   /**< 所需权限 */
    security_check_callback_t callback;     /**< 检查回调 */
    void                  *callback_arg;    /**< 回调参数 */
    bool                   check_passed;    /**< 检查是否通过 */
} SecurityExecState;

/* ============================================================
 * 函数声明
 * ============================================================ */

/**
 * @brief 创建带安全检查的执行节点
 * @param mgr 安全管理器指针
 * @param child 子执行节点
 * @param user_id 用户ID
 * @param required_perm 所需权限
 * @return 成功返回执行节点指针，失败返回 NULL
 */
ExecNode *exec_create_with_security(security_mgr_t *mgr,
                                    ExecNode *child,
                                    int user_id,
                                    permission_t required_perm);

/**
 * @brief 创建带安全检查和回调的执行节点
 * @param mgr 安全管理器指针
 * @param child 子执行节点
 * @param user_id 用户ID
 * @param required_perm 所需权限
 * @param callback 检查通过后的回调函数
 * @param callback_arg 回调参数
 * @return 成功返回执行节点指针，失败返回 NULL
 */
ExecNode *exec_create_with_security_callback(security_mgr_t *mgr,
                                             ExecNode *child,
                                             int user_id,
                                             permission_t required_perm,
                                             security_check_callback_t callback,
                                             void *callback_arg);

#ifdef __cplusplus
}
#endif

#endif /* DB_SECURITY_SECURITY_EXECUTOR_H */
