/**
 * @file sys_catalog.h
 * @brief 系统表管理工具公共接口
 *
 * 提供对系统表（pg_class, pg_attribute 等）的查询与管理功能，
 * 包括系统表初始化、关闭、元数据查询等操作。
 */
#ifndef DB_TOOLS_SYS_CATALOG_H
#define DB_TOOLS_SYS_CATALOG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 对象标识符类型 */
typedef uint32_t Oid;

/** 无效的 OID */
#define SYS_CATALOG_INVALID_OID ((Oid)0)

/** 系统表句柄 */
typedef struct sys_catalog_s sys_catalog_t;

/* ============================================================
 * 系统表生命周期管理
 * ============================================================ */

/**
 * @brief 初始化系统表管理器
 * @return 系统表句柄，失败返回 NULL
 */
sys_catalog_t *sys_catalog_init(void);

/**
 * @brief 关闭系统表管理器并释放资源
 * @param catalog 系统表句柄
 */
void sys_catalog_shutdown(sys_catalog_t *catalog);

/* ============================================================
 * 系统表查询接口
 * ============================================================ */

/**
 * @brief 获取所有表（class）的计数
 * @param catalog 系统表句柄
 * @return 表的数量，失败返回 -1
 */
int sys_catalog_get_all_classes(sys_catalog_t *catalog);

/**
 * @brief 获取指定表的属性（列）数量
 * @param catalog 系统表句柄
 * @param rel_oid 表的 OID
 * @return 属性数量，无效 OID 返回 0
 */
int sys_catalog_get_attributes(sys_catalog_t *catalog, Oid rel_oid);

#ifdef __cplusplus
}
#endif

#endif /* DB_TOOLS_SYS_CATALOG_H */
