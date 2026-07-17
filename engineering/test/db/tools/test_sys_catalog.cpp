/**
 * @file test_sys_catalog.cpp
 * @brief 系统表管理工具单元测试
 *
 * 测试 sys_catalog 模块的初始化/关闭、
 * 系统表查询等基本功能。
 */
#include <gtest/gtest.h>
#include "db/tools/sys_catalog.h"

/* ============================================================
 * 初始化/关闭测试
 * ============================================================ */

TEST(SysCatalogTest, InitShutdown)
{
    sys_catalog_t *catalog = sys_catalog_init();
    ASSERT_NE(catalog, nullptr);

    sys_catalog_shutdown(catalog);
    /* 成功关闭不应崩溃 */
}

TEST(SysCatalogTest, InitReturnsNonNull)
{
    sys_catalog_t *catalog = sys_catalog_init();
    ASSERT_NE(catalog, nullptr);
    sys_catalog_shutdown(catalog);
}

TEST(SysCatalogTest, ShutdownNullSafe)
{
    /* 传入 NULL 不应崩溃 */
    sys_catalog_shutdown(NULL);
}

/* ============================================================
 * 系统表查询测试
 * ============================================================ */

TEST(SysCatalogTest, GetAllClassesReturnsNonNegative)
{
    sys_catalog_t *catalog = sys_catalog_init();
    ASSERT_NE(catalog, nullptr);

    int count = sys_catalog_get_all_classes(catalog);
    EXPECT_GE(count, 0);

    sys_catalog_shutdown(catalog);
}

TEST(SysCatalogTest, GetAttributesWithInvalidOidReturnsZero)
{
    sys_catalog_t *catalog = sys_catalog_init();
    ASSERT_NE(catalog, nullptr);

    /* 对空 OID 返回 0 */
    int attr_count = sys_catalog_get_attributes(catalog, SYS_CATALOG_INVALID_OID);
    EXPECT_EQ(attr_count, 0);

    sys_catalog_shutdown(catalog);
}

TEST(SysCatalogTest, GetAttributesWithZeroOidReturnsZero)
{
    sys_catalog_t *catalog = sys_catalog_init();
    ASSERT_NE(catalog, nullptr);

    /* 显式传入 0 也应返回 0 */
    int attr_count = sys_catalog_get_attributes(catalog, 0);
    EXPECT_EQ(attr_count, 0);

    sys_catalog_shutdown(catalog);
}