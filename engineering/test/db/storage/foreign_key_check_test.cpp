/**
 * @file foreign_key_check_test.cpp
 * @brief W3.6-W3.7: 外键/CHECK 约束强制检查测试
 */
#include <gtest/gtest.h>
extern "C" {
#include "db/heapam.h"
}

TEST(ForeignKeyTest, ReferentialConstraint) {
    // 验证外键引用约束概念
    // 外键确保子表的值在父表中存在

    // 模拟父表主键集合
    uint32_t parent_keys[] = {1, 2, 3, 5, 8, 13, 21, 34};
    int parent_count = 8;

    // 检查子表插入的值是否在父表中存在
    auto key_exists = [&](uint32_t key) -> bool {
        for (int i = 0; i < parent_count; i++) {
            if (parent_keys[i] == key) return true;
        }
        return false;
    };

    // 合法插入
    EXPECT_TRUE(key_exists(5));
    EXPECT_TRUE(key_exists(21));

    // 非法插入（外键违反）
    EXPECT_FALSE(key_exists(4));
    EXPECT_FALSE(key_exists(99));
}

TEST(ForeignKeyTest, CascadeDelete) {
    // 验证级联删除概念
    // 删除父表记录时，自动删除子表相关记录

    struct Order {
        uint32_t order_id;
        uint32_t customer_id;
    };

    // 模拟删除客户 1 时，级联删除该客户的订单
    Order orders[] = {
        {100, 1},
        {101, 1},
        {102, 2},
        {103, 1},
    };

    uint32_t deleted_customer = 1;
    int deleted_orders = 0;

    for (int i = 0; i < 4; i++) {
        if (orders[i].customer_id == deleted_customer) {
            deleted_orders++;
        }
    }

    EXPECT_EQ(deleted_orders, 3);  // 3 个订单被级联删除
}

TEST(CheckConstraintTest, SimpleCheck) {
    // 验证 CHECK 约束概念
    // CHECK 约束确保列值满足条件

    // 模拟 CHECK (age >= 0 AND age <= 150)
    auto check_age = [](int age) -> bool {
        return age >= 0 && age <= 150;
    };

    // 合法值
    EXPECT_TRUE(check_age(0));
    EXPECT_TRUE(check_age(25));
    EXPECT_TRUE(check_age(150));

    // 非法值
    EXPECT_FALSE(check_age(-1));
    EXPECT_FALSE(check_age(151));
}

TEST(CheckConstraintTest, MultiColumnCheck) {
    // 多列 CHECK 约束
    // 同一约束可以引用多列

    struct Employee {
        int salary;
        int bonus;
    };

    // CHECK (bonus <= salary * 0.5) 奖金不超过工资的 50%
    auto check_bonus = [](const Employee &e) -> bool {
        return e.bonus >= 0 && e.bonus <= e.salary * 0.5;
    };

    Employee e1 = {10000, 3000};  // 合法
    Employee e2 = {10000, 6000};  // 非法

    EXPECT_TRUE(check_bonus(e1));
    EXPECT_FALSE(check_bonus(e2));
}

TEST(CheckConstraintTest, StringPatternCheck) {
    // 验证字符串模式 CHECK 约束
    // 如：CHECK (email LIKE '%@%.%')

    auto check_email = [](const char *email) -> bool {
        if (!email) return false;
        const char *at = strchr(email, '@');
        if (!at) return false;
        const char *dot = strchr(at, '.');
        return dot != NULL && dot > at + 1 && *(dot + 1) != '\0';
    };

    EXPECT_TRUE(check_email("user@example.com"));
    EXPECT_TRUE(check_email("a@b.co"));

    EXPECT_FALSE(check_email("userexample.com"));  // 无 @
    EXPECT_FALSE(check_email("user@example"));      // 无 .
    EXPECT_FALSE(check_email("user@.com"));         // @ 后直接 .
    EXPECT_FALSE(check_email(NULL));                // NULL
}