/**
 * @file include/db/security/security_manager.h
 * @brief 安全管理系统统一入口
 */
#ifndef DB_SECURITY_SECURITY_MANAGER_H
#define DB_SECURITY_SECURITY_MANAGER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

typedef struct security_manager security_mgr_t;

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

#ifdef __cplusplus
}
#endif

#endif /* DB_SECURITY_SECURITY_MANAGER_H */
