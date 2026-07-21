/**
 * @file partition_create_test.cpp
 * @brief W4.3: 分区表创建测试
 *
 * 测试 catalog_create_partitioned_table、catalog_create_partition、
 * catalog_get_partitions、catalog_get_partition_parent 等 API。
 */
#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "db/catalog.h"
}

namespace {

/**
 * @brief 测试分区表元数据
 */
class PartitionCreateTest : public ::testing::Test {
protected:
    void SetUp() override {
        catalog_init();
    }

    void TearDown() override {
        catalog_shutdown();
    }
};

/**
 * @brief 测试创建分区表
 */
TEST_F(PartitionCreateTest, CreatePartitionedTable) {
    column_def_t cols[2];
    strncpy(cols[0].name, "id", NAMEDATALEN - 1);
    cols[0].type_oid = 1;  /* int */
    cols[0].not_null = true;

    strncpy(cols[1].name, "name", NAMEDATALEN - 1);
    cols[1].type_oid = 3;  /* text */

    int16_t part_attrs[] = {1};  /* 按 id 分区 */

    Oid oid = catalog_create_partitioned_table("orders",
        cols, 2, PARTITION_STRATEGY_RANGE, part_attrs, 1);

    ASSERT_NE(oid, InvalidOid);

    /* 验证表信息 */
    table_info_t *info = catalog_get_table(oid);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->relkind, 'p');  /* 分区表 */
    EXPECT_EQ(info->part_strategy, PARTITION_STRATEGY_RANGE);
    EXPECT_EQ(info->partnatts, 1);
    EXPECT_EQ(info->partattrs[0], 1);
    EXPECT_EQ(info->nparts, 0);
    EXPECT_EQ(info->natts, 2);

    catalog_free_table(info);
}

/**
 * @brief 测试创建分区表 — 无效参数
 */
TEST_F(PartitionCreateTest, CreatePartitionedTableInvalid) {
    column_def_t cols[1];
    strncpy(cols[0].name, "id", NAMEDATALEN - 1);
    cols[0].type_oid = 1;

    /* 分区键列数为 0 */
    Oid oid = catalog_create_partitioned_table("t1",
        cols, 1, PARTITION_STRATEGY_RANGE, NULL, 0);
    EXPECT_EQ(oid, InvalidOid);

    /* NULL 表名 */
    oid = catalog_create_partitioned_table(NULL,
        cols, 1, PARTITION_STRATEGY_RANGE, NULL, 0);
    EXPECT_EQ(oid, InvalidOid);
}

/**
 * @brief 测试创建分区子表
 */
TEST_F(PartitionCreateTest, CreatePartition) {
    column_def_t cols[2];
    strncpy(cols[0].name, "id", NAMEDATALEN - 1);
    cols[0].type_oid = 1;
    strncpy(cols[1].name, "name", NAMEDATALEN - 1);
    cols[1].type_oid = 3;

    int16_t part_attrs[] = {1};

    /* 创建分区表 */
    Oid parent_oid = catalog_create_partitioned_table("orders",
        cols, 2, PARTITION_STRATEGY_RANGE, part_attrs, 1);
    ASSERT_NE(parent_oid, InvalidOid);

    /* 创建分区子表 */
    Oid part1 = catalog_create_partition(parent_oid, "orders_2024", 20240101);
    ASSERT_NE(part1, InvalidOid);
    ASSERT_NE(part1, parent_oid);

    Oid part2 = catalog_create_partition(parent_oid, "orders_2025", 20250101);
    ASSERT_NE(part2, InvalidOid);

    /* 验证分区子表信息 */
    table_info_t *pinfo = catalog_get_table(part1);
    ASSERT_NE(pinfo, nullptr);
    EXPECT_EQ(pinfo->relkind, 'r');  /* 物理表 */
    EXPECT_EQ(pinfo->parent_oid, parent_oid);

    catalog_free_table(pinfo);
}

/**
 * @brief 测试获取分区列表
 */
TEST_F(PartitionCreateTest, GetPartitions) {
    column_def_t cols[1];
    strncpy(cols[0].name, "id", NAMEDATALEN - 1);
    cols[0].type_oid = 1;

    int16_t part_attrs[] = {1};

    Oid parent_oid = catalog_create_partitioned_table("logs",
        cols, 1, PARTITION_STRATEGY_RANGE, part_attrs, 1);
    ASSERT_NE(parent_oid, InvalidOid);

    /* 创建 3 个分区 */
    Oid p1 = catalog_create_partition(parent_oid, "logs_jan", 100);
    Oid p2 = catalog_create_partition(parent_oid, "logs_feb", 200);
    Oid p3 = catalog_create_partition(parent_oid, "logs_mar", 300);
    ASSERT_NE(p1, InvalidOid);
    ASSERT_NE(p2, InvalidOid);
    ASSERT_NE(p3, InvalidOid);

    /* 获取分区列表 */
    int nparts = 0;
    Oid *parts = catalog_get_partitions(parent_oid, &nparts);

    ASSERT_NE(parts, nullptr);
    EXPECT_EQ(nparts, 3);

    /* 验证分区 OID */
    bool found[3] = {false, false, false};
    for (int i = 0; i < nparts; i++) {
        if (parts[i] == p1) found[0] = true;
        if (parts[i] == p2) found[1] = true;
        if (parts[i] == p3) found[2] = true;
    }
    EXPECT_TRUE(found[0]);
    EXPECT_TRUE(found[1]);
    EXPECT_TRUE(found[2]);

    free(parts);
}

/**
 * @brief 测试获取分区父表
 */
TEST_F(PartitionCreateTest, GetPartitionParent) {
    column_def_t cols[1];
    strncpy(cols[0].name, "id", NAMEDATALEN - 1);
    cols[0].type_oid = 1;

    int16_t part_attrs[] = {1};

    Oid parent_oid = catalog_create_partitioned_table("t",
        cols, 1, PARTITION_STRATEGY_HASH, part_attrs, 1);
    ASSERT_NE(parent_oid, InvalidOid);

    /* 非分区表应返回 InvalidOid */
    Oid normal_oid = catalog_create_table("normal", cols, 1);
    ASSERT_NE(normal_oid, InvalidOid);
    EXPECT_EQ(catalog_get_partition_parent(normal_oid), InvalidOid);

    /* 分区子表应返父表 OID */
    Oid part_oid = catalog_create_partition(parent_oid, "t_p0", 0);
    ASSERT_NE(part_oid, InvalidOid);
    EXPECT_EQ(catalog_get_partition_parent(part_oid), parent_oid);
}

/**
 * @brief 测试非分区表获取分区列表返回空
 */
TEST_F(PartitionCreateTest, NonPartitionedTableGetPartitions) {
    column_def_t cols[1];
    strncpy(cols[0].name, "id", NAMEDATALEN - 1);
    cols[0].type_oid = 1;

    Oid oid = catalog_create_table("regular", cols, 1);
    ASSERT_NE(oid, InvalidOid);

    int nparts = 0;
    Oid *parts = catalog_get_partitions(oid, &nparts);
    EXPECT_EQ(parts, nullptr);
    EXPECT_EQ(nparts, 0);
}

/**
 * @brief 测试分区表名称去重
 */
TEST_F(PartitionCreateTest, PartitionNameUnique) {
    column_def_t cols[1];
    strncpy(cols[0].name, "id", NAMEDATALEN - 1);
    cols[0].type_oid = 1;

    int16_t part_attrs[] = {1};

    Oid oid = catalog_create_partitioned_table("dup_test",
        cols, 1, PARTITION_STRATEGY_LIST, part_attrs, 1);
    ASSERT_NE(oid, InvalidOid);

    /* 同名分区表应失败 */
    Oid dup = catalog_create_partitioned_table("dup_test",
        cols, 1, PARTITION_STRATEGY_LIST, part_attrs, 1);
    EXPECT_EQ(dup, InvalidOid);
}

}  // namespace