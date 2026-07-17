/**
 * @file sys_catalog.c
 * @brief 系统表管理工具 stub 实现
 *
 * 为后续工具模块提供可编译的占位实现。
 * 当前阶段仅返回固定值，后续将接入实际的系统表存储。
 */
#include "db/tools/sys_catalog.h"
#include <stdlib.h>

/* ============================================================
 * 内部结构定义
 * ============================================================ */

struct sys_catalog_s {
    int initialized;  /**< 初始化标记 */
};

/* ============================================================
 * 系统表生命周期管理
 * ============================================================ */

sys_catalog_t *sys_catalog_init(void)
{
    sys_catalog_t *catalog = (sys_catalog_t *)malloc(sizeof(sys_catalog_t));
    if (catalog == NULL) {
        return NULL;
    }
    catalog->initialized = 1;
    return catalog;
}

void sys_catalog_shutdown(sys_catalog_t *catalog)
{
    if (catalog != NULL) {
        catalog->initialized = 0;
        free(catalog);
    }
}

/* ============================================================
 * 系统表查询接口
 * ============================================================ */

int sys_catalog_get_all_classes(sys_catalog_t *catalog)
{
    (void)catalog;
    /* stub 实现：返回 0，表示暂无可查询的系统表 */
    return 0;
}

int sys_catalog_get_attributes(sys_catalog_t *catalog, Oid rel_oid)
{
    (void)catalog;
    /* stub 实现：对空 OID 返回 0 */
    if (rel_oid == SYS_CATALOG_INVALID_OID) {
        return 0;
    }
    return 0;
}