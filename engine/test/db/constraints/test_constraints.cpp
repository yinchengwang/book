/*
 * test_constraints.cpp - 数据库约束检查测试
 */

#include <gtest/gtest.h>
#include <db/constraints/constraints.h>

/* ─────────────────────────────────────────────────────────────────
 * 约束检查器测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Constraints, CreateAndDestroy)
{
    constraint_checker_t *checker = constraint_checker_create(1);
    ASSERT_NE(nullptr, checker);

    constraint_checker_destroy(checker);
}

TEST(Constraints, AddPrimaryKeyConstraint)
{
    constraint_checker_t *checker = constraint_checker_create(1);

    constraint_def_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = CONSTRAINT_PRIMARY_KEY;
    constraint.name = strdup("pk_users_id");
    constraint.column_id = 0;
    constraint.num_columns = 1;
    constraint.deferrable = false;
    constraint.initially_deferred = false;

    int result = constraint_checker_add(checker, &constraint);
    EXPECT_EQ(0, result);

    free(constraint.name);
    constraint_checker_destroy(checker);
}

TEST(Constraints, AddUniqueConstraint)
{
    constraint_checker_t *checker = constraint_checker_create(1);

    constraint_def_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = CONSTRAINT_UNIQUE;
    constraint.name = strdup("uq_users_email");
    constraint.column_id = 1;
    constraint.num_columns = 1;
    constraint.deferrable = false;
    constraint.initially_deferred = false;

    int result = constraint_checker_add(checker, &constraint);
    EXPECT_EQ(0, result);

    free(constraint.name);
    constraint_checker_destroy(checker);
}

TEST(Constraints, AddForeignKeyConstraint)
{
    constraint_checker_t *checker = constraint_checker_create(1);

    constraint_def_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = CONSTRAINT_FOREIGN_KEY;
    constraint.name = strdup("fk_orders_user_id");
    constraint.column_id = 1;
    constraint.num_columns = 1;
    constraint.ref_table_id = 2;
    constraint.ref_column_id = 0;
    constraint.deferrable = false;
    constraint.initially_deferred = false;

    int result = constraint_checker_add(checker, &constraint);
    EXPECT_EQ(0, result);

    free(constraint.name);
    constraint_checker_destroy(checker);
}

/* ─────────────────────────────────────────────────────────────────
 * 约束检查测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Constraints, CheckNotNull)
{
    constraint_checker_t *checker = constraint_checker_create(1);

    /* 非 NULL 值应该通过 */
    int value = 42;
    constraint_error_t err = constraint_check_not_null(checker, 0, &value);
    EXPECT_EQ(CONSTRAINT_OK, err);

    /* NULL 值应该失败 */
    err = constraint_check_not_null(checker, 0, NULL);
    EXPECT_EQ(CONSTRAINT_ERR_NULL, err);

    constraint_checker_destroy(checker);
}

TEST(Constraints, CheckPrimaryKey)
{
    constraint_checker_t *checker = constraint_checker_create(1);

    /* 添加主键约束 */
    constraint_def_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = CONSTRAINT_PRIMARY_KEY;
    constraint.name = strdup("pk_id");
    constraint.column_id = 0;
    constraint.num_columns = 1;
    constraint_checker_add(checker, &constraint);
    free(constraint.name);

    /* 第一个值应该通过 */
    int key1 = 1;
    constraint_error_t err = constraint_check_primary_key(checker, &key1, sizeof(key1), 0);
    EXPECT_EQ(CONSTRAINT_OK, err);

    /* 重复值应该失败 */
    int key2 = 1;
    err = constraint_check_primary_key(checker, &key2, sizeof(key2), 0);
    EXPECT_EQ(CONSTRAINT_ERR_PRIMARY_KEY, err);

    constraint_checker_destroy(checker);
}

TEST(Constraints, CheckUnique)
{
    constraint_checker_t *checker = constraint_checker_create(1);

    /* 添加唯一约束 */
    constraint_def_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = CONSTRAINT_UNIQUE;
    constraint.name = strdup("uq_email");
    constraint.column_id = 1;
    constraint.num_columns = 1;
    constraint_checker_add(checker, &constraint);
    free(constraint.name);

    /* 第一个值应该通过 */
    char email1[] = "first@test.com";
    constraint_error_t err = constraint_check_unique(checker, 1, email1, 0);
    EXPECT_EQ(CONSTRAINT_OK, err);

    /* 不同的值应该通过 */
    char email2[] = "second@test.com";
    err = constraint_check_unique(checker, 1, email2, 0);
    EXPECT_EQ(CONSTRAINT_OK, err);

    constraint_checker_destroy(checker);
}

TEST(Constraints, CheckPrimaryKeyDuplicate)
{
    constraint_checker_t *checker = constraint_checker_create(1);

    /* 添加主键约束 */
    constraint_def_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = CONSTRAINT_PRIMARY_KEY;
    constraint.name = strdup("pk_id");
    constraint.column_id = 0;
    constraint.num_columns = 1;
    constraint_checker_add(checker, &constraint);
    free(constraint.name);

    /* 第一个值应该通过 */
    int key1 = 42;
    constraint_error_t err = constraint_check_primary_key(checker, &key1, sizeof(key1), 0);
    EXPECT_EQ(CONSTRAINT_OK, err);

    /* 重复值应该失败 */
    int key2 = 42;
    err = constraint_check_primary_key(checker, &key2, sizeof(key2), 0);
    EXPECT_EQ(CONSTRAINT_ERR_PRIMARY_KEY, err);

    constraint_checker_destroy(checker);
}

TEST(Constraints, CheckForeignKey)
{
    /* 创建父表检查器 */
    constraint_checker_t *parent = constraint_checker_create(2);

    constraint_def_t pk_constraint;
    memset(&pk_constraint, 0, sizeof(pk_constraint));
    pk_constraint.type = CONSTRAINT_PRIMARY_KEY;
    pk_constraint.name = strdup("pk_users_id");
    pk_constraint.column_id = 0;
    pk_constraint.num_columns = 1;
    constraint_checker_add(parent, &pk_constraint);
    free(pk_constraint.name);

    /* 插入主键值（使用同一变量，确保地址相同） */
    int user_id = 100;
    constraint_error_t pk_err = constraint_check_primary_key(parent, &user_id, sizeof(user_id), 0);
    EXPECT_EQ(CONSTRAINT_OK, pk_err);

    /* 创建子表检查器 */
    constraint_checker_t *child = constraint_checker_create(1);

    constraint_def_t fk_constraint;
    memset(&fk_constraint, 0, sizeof(fk_constraint));
    fk_constraint.type = CONSTRAINT_FOREIGN_KEY;
    fk_constraint.name = strdup("fk_user_id");
    fk_constraint.column_id = 1;
    fk_constraint.num_columns = 1;
    fk_constraint.ref_table_id = 2;
    fk_constraint.ref_column_id = 0;
    constraint_checker_add(child, &fk_constraint);
    free(fk_constraint.name);

    /* 存在的引用应该通过（使用同一变量） */
    constraint_error_t err = constraint_check_foreign_key(child, parent, &user_id);
    EXPECT_EQ(CONSTRAINT_OK, err);

    /* 不存在的引用应该失败 */
    int invalid_ref = 999;
    err = constraint_check_foreign_key(child, parent, &invalid_ref);
    EXPECT_EQ(CONSTRAINT_ERR_NO_REF, err);

    /* NULL 引用应该通过（外键允许为空） */
    err = constraint_check_foreign_key(child, parent, NULL);
    EXPECT_EQ(CONSTRAINT_OK, err);

    constraint_checker_destroy(child);
    constraint_checker_destroy(parent);
}

/* ─────────────────────────────────────────────────────────────────
 * 错误消息测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Constraints, ErrorMessages)
{
    EXPECT_STREQ("OK", constraint_error_msg(CONSTRAINT_OK));
    EXPECT_STREQ("NOT NULL constraint violation",
                 constraint_error_msg(CONSTRAINT_ERR_NULL));
    EXPECT_STREQ("PRIMARY KEY constraint violation",
                 constraint_error_msg(CONSTRAINT_ERR_PRIMARY_KEY));
    EXPECT_STREQ("UNIQUE constraint violation",
                 constraint_error_msg(CONSTRAINT_ERR_UNIQUE));
    EXPECT_STREQ("FOREIGN KEY constraint violation",
                 constraint_error_msg(CONSTRAINT_ERR_FOREIGN_KEY));
    EXPECT_STREQ("FOREIGN KEY has no matching reference",
                 constraint_error_msg(CONSTRAINT_ERR_NO_REF));
}

/* ─────────────────────────────────────────────────────────────────
 * 简化 API 测试
 * ───────────────────────────────────────────────────────────────── */

TEST(Constraints, ValidateConstraints)
{
    constraint_def_t constraints[1];
    memset(&constraints[0], 0, sizeof(constraints[0]));
    constraints[0].type = CONSTRAINT_NOT_NULL;
    constraints[0].name = strdup("nn_name");
    constraints[0].column_id = 0;
    constraints[0].num_columns = 1;
    constraints[0].deferrable = false;
    constraints[0].initially_deferred = false;

    const void *values[] = { "test" };

    constraint_error_t err = validate_constraints(
        1, constraints, 1, values, 1, 0);

    EXPECT_EQ(CONSTRAINT_OK, err);

    free(constraints[0].name);
}