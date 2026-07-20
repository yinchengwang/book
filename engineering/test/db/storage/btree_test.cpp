/**
 * @file btree_test.cpp
 * @brief BTree 页面格式扩展测试（Task W1.1）
 *
 * 测试 btpage.h 中新增的字段和标志位。
 */
#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/access/btree/btpage.h"
}

/**
 * @brief 测试 BTPageHeader 结构体包含 btpo_rightlink 字段
 */
TEST(BTreePageTest, HeaderHasRightlink) {
    BTPageHeaderData header;
    memset(&header, 0, sizeof(header));

    // 验证 btpo_rightlink 字段存在且可写入
    header.btpo_rightlink = 42;
    EXPECT_EQ(header.btpo_rightlink, 42u);

    // 验证 btpo_cycleid 字段存在且可写入
    header.btpo_cycleid = 123;
    EXPECT_EQ(header.btpo_cycleid, 123u);
}

/**
 * @brief 测试 btpo_flags 标志位组合
 */
TEST(BTreePageTest, FlagsCombination) {
    BTPageHeaderData header;
    memset(&header, 0, sizeof(header));

    // 验证 btpo_flags 可组合赋值
    header.btpo_flags = BTP_LEAF | BTP_ROOT;
    EXPECT_TRUE(header.btpo_flags & BTP_LEAF);
    EXPECT_TRUE(header.btpo_flags & BTP_ROOT);
    EXPECT_FALSE(header.btpo_flags & BTP_INTERNAL);
    EXPECT_FALSE(header.btpo_flags & BTP_HALF_DEAD);

    // 测试内部节点标志
    header.btpo_flags = BTP_INTERNAL;
    EXPECT_TRUE(header.btpo_flags & BTP_INTERNAL);
    EXPECT_FALSE(header.btpo_flags & BTP_LEAF);

    // 测试半删除状态标志
    header.btpo_flags = BTP_HALF_DEAD;
    EXPECT_TRUE(header.btpo_flags & BTP_HALF_DEAD);
}

/**
 * @brief 测试所有标志位定义
 */
TEST(BTreePageTest, AllFlagsDefined) {
    // 验证所有标志位的值
    EXPECT_EQ(BTP_LEAF, 0x01);
    EXPECT_EQ(BTP_ROOT, 0x02);
    EXPECT_EQ(BTP_INTERNAL, 0x04);
    EXPECT_EQ(BTP_HALF_DEAD, 0x08);
    EXPECT_EQ(BTP_META, 0x10);
    EXPECT_EQ(BTP_DELETED, 0x20);

    // 验证标志位可以组合使用
    uint16_t combined = BTP_LEAF | BTP_ROOT | BTP_INTERNAL | BTP_HALF_DEAD;
    EXPECT_EQ(combined, 0x0F);  // 0x01 | 0x02 | 0x04 | 0x08 = 0x0F
}

/**
 * @brief 测试辅助宏定义
 */
TEST(BTreePageTest, HelperMacros) {
    BTPageHeaderData header;
    memset(&header, 0, sizeof(header));

    // 测试 BT_PAGE_IS_LEAF 宏
    header.btpo_flags = BTP_LEAF;
    EXPECT_TRUE(BT_PAGE_IS_LEAF(&header));
    EXPECT_FALSE(BT_PAGE_IS_ROOT(&header));
    EXPECT_FALSE(BT_PAGE_IS_INTERNAL(&header));
    EXPECT_FALSE(BT_PAGE_IS_HALF_DEAD(&header));

    // 测试 BT_PAGE_IS_ROOT 宏
    header.btpo_flags = BTP_ROOT;
    EXPECT_FALSE(BT_PAGE_IS_LEAF(&header));
    EXPECT_TRUE(BT_PAGE_IS_ROOT(&header));

    // 测试 BT_PAGE_IS_INTERNAL 宏
    header.btpo_flags = BTP_INTERNAL;
    EXPECT_TRUE(BT_PAGE_IS_INTERNAL(&header));

    // 测试 BT_PAGE_IS_HALF_DEAD 宏
    header.btpo_flags = BTP_HALF_DEAD;
    EXPECT_TRUE(BT_PAGE_IS_HALF_DEAD(&header));

    // 测试组合状态
    header.btpo_flags = BTP_LEAF | BTP_ROOT;
    EXPECT_TRUE(BT_PAGE_IS_LEAF(&header));
    EXPECT_TRUE(BT_PAGE_IS_ROOT(&header));
}

/**
 * @brief 测试结构体大小合理性
 */
TEST(BTreePageTest, StructSize) {
    // BTPageHeaderData 应该是合理的大小（不超过页面头部大小）
    // 字段：btpo_flags(2) + btpo_level(2) + btpo_prev(4) + btpo_next(4) +
    //       btpo_rightlink(4) + btpo_cycleid(4) + btpo_xact(4) +
    //       btpo_offset(2) + btpo_count(2) = 28 字节
    EXPECT_LE(sizeof(BTPageHeaderData), 64u);
}

/**
 * @brief 测试 btpo_rightlink 与 btpo_next 独立性
 */
TEST(BTreePageTest, RightlinkIndependent) {
    BTPageHeaderData header;
    memset(&header, 0, sizeof(header));

    // 设置 btpo_next
    header.btpo_next = 100;
    EXPECT_EQ(header.btpo_next, 100u);
    EXPECT_EQ(header.btpo_rightlink, 0u);

    // 设置 btpo_rightlink（独立于 btpo_next）
    header.btpo_rightlink = 200;
    EXPECT_EQ(header.btpo_rightlink, 200u);
    EXPECT_EQ(header.btpo_next, 100u);  // btpo_next 不受影响

    // 两者可以独立设置
    header.btpo_next = 300;
    header.btpo_rightlink = 400;
    EXPECT_EQ(header.btpo_next, 300u);
    EXPECT_EQ(header.btpo_rightlink, 400u);
}

/**
 * @brief 测试 btpo_cycleid 用途
 */
TEST(BTreePageTest, CycleIdUsage) {
    BTPageHeaderData header;
    memset(&header, 0, sizeof(header));

    // btpo_cycleid 用于标识并发分裂周期
    // 通常从 1 开始递增
    header.btpo_cycleid = 1;
    EXPECT_EQ(header.btpo_cycleid, 1u);

    // 可以设置为较大的值
    header.btpo_cycleid = 0xFFFFFFFF;
    EXPECT_EQ(header.btpo_cycleid, 0xFFFFFFFFu);
}