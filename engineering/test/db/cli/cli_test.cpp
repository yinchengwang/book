/*
 * cli_test.cpp - CLI 交互界面测试
 *
 * 注意：测试通过 db_cli API 进行，不直接访问内部结构
 */

#include <gtest/gtest.h>
#include <db/cli/cli.h>
#include <cstdio>
#include <cstring>

class CLITest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove("test_cli.db");
        std::remove("test_cli2.db");
    }

    void TearDown() override {
        std::remove("test_cli.db");
        std::remove("test_cli2.db");
    }

    db_cli_t* create_cli(const char* db_path) {
        db_cli_config_t config = DB_CLI_DEFAULT_CONFIG;
        config.db_path = db_path;
        config.echo = false;
        config.show_timing = false;
        return db_cli_create(&config);
    }
};

TEST_F(CLITest, CreateAndDestroy) {
    db_cli_t* cli = create_cli("test_cli.db");
    ASSERT_NE(nullptr, cli);
    db_cli_destroy(cli);
}

TEST_F(CLITest, ExecuteSimpleSQL) {
    db_cli_t* cli = create_cli("test_cli.db");
    ASSERT_NE(nullptr, cli);

    // 创建表
    EXPECT_EQ(0, db_cli_exec(cli, "CREATE TABLE users (id INT, name TEXT)"));

    // 插入数据
    EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO users VALUES (1, 'Alice')"));

    // 查询
    EXPECT_EQ(0, db_cli_exec(cli, "SELECT * FROM users"));

    db_cli_destroy(cli);
}

TEST_F(CLITest, ExecuteMultipleSQL) {
    db_cli_t* cli = create_cli("test_cli.db");
    ASSERT_NE(nullptr, cli);

    EXPECT_EQ(0, db_cli_exec(cli, "CREATE TABLE t1 (id INT)"));
    EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO t1 VALUES (1)"));
    EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO t1 VALUES (2)"));
    EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO t1 VALUES (3)"));
    EXPECT_EQ(0, db_cli_exec(cli, "SELECT * FROM t1 WHERE id > 1"));

    db_cli_destroy(cli);
}

TEST_F(CLITest, InvalidSQL) {
    db_cli_t* cli = create_cli("test_cli.db");
    ASSERT_NE(nullptr, cli);

    // 无效 SQL 应该返回非零
    EXPECT_NE(0, db_cli_exec(cli, "INVALID SQL SYNTAX"));
    EXPECT_NE(0, db_cli_exec(cli, "SELECT * FROM nonexistent"));

    db_cli_destroy(cli);
}

TEST_F(CLITest, SeparateDatabases) {
    // 两个独立的数据库文件
    db_cli_t* cli1 = create_cli("test_cli.db");
    db_cli_t* cli2 = create_cli("test_cli2.db");

    ASSERT_NE(nullptr, cli1);
    ASSERT_NE(nullptr, cli2);

    // 在 cli1 中创建表
    EXPECT_EQ(0, db_cli_exec(cli1, "CREATE TABLE users (id INT)"));
    EXPECT_EQ(0, db_cli_exec(cli1, "INSERT INTO users VALUES (1)"));

    // cli2 看不到 cli1 的数据
    EXPECT_NE(0, db_cli_exec(cli2, "SELECT * FROM users"));  // 表不存在

    db_cli_destroy(cli1);
    db_cli_destroy(cli2);
}

TEST_F(CLITest, DDLAndDML) {
    db_cli_t* cli = create_cli("test_cli.db");
    ASSERT_NE(nullptr, cli);

    // DDL
    EXPECT_EQ(0, db_cli_exec(cli, "CREATE TABLE users (id INT, name TEXT)"));

    // DML
    EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO users VALUES (1, 'Alice')"));
    EXPECT_EQ(0, db_cli_exec(cli, "UPDATE users SET name = 'Bob' WHERE id = 1"));
    EXPECT_EQ(0, db_cli_exec(cli, "SELECT * FROM users"));
    EXPECT_EQ(0, db_cli_exec(cli, "DELETE FROM users WHERE id = 1"));

    // DDL
    EXPECT_EQ(0, db_cli_exec(cli, "DROP TABLE users"));

    db_cli_destroy(cli);
}

TEST_F(CLITest, DataPersistence) {
    {
        // 第一个会话：创建表并插入数据
        db_cli_t* cli = create_cli("test_cli.db");
        ASSERT_NE(nullptr, cli);

        EXPECT_EQ(0, db_cli_exec(cli, "CREATE TABLE users (id INT, name TEXT)"));
        EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO users VALUES (1, 'Alice')"));
        EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO users VALUES (2, 'Bob')"));

        db_cli_destroy(cli);
    }

    {
        // 第二个会话：验证数据存在
        db_cli_t* cli = create_cli("test_cli.db");
        ASSERT_NE(nullptr, cli);

        EXPECT_EQ(0, db_cli_exec(cli, "SELECT * FROM users"));  // 应该成功

        db_cli_destroy(cli);
    }
}

TEST_F(CLITest, ComplexSchema) {
    db_cli_t* cli = create_cli("test_cli.db");
    ASSERT_NE(nullptr, cli);

    // 创建复杂表
    EXPECT_EQ(0, db_cli_exec(cli,
        "CREATE TABLE orders ("
        "id INT, customer_id INT, product TEXT, "
        "quantity INT, price REAL, status TEXT"
        ")"));

    // 插入测试数据
    EXPECT_EQ(0, db_cli_exec(cli,
        "INSERT INTO orders VALUES (1, 1, 'Laptop', 1, 999.99, 'completed')"));
    EXPECT_EQ(0, db_cli_exec(cli,
        "INSERT INTO orders VALUES (2, 1, 'Mouse', 2, 29.99, 'pending')"));
    EXPECT_EQ(0, db_cli_exec(cli,
        "INSERT INTO orders VALUES (3, 2, 'Keyboard', 1, 79.99, 'completed')"));

    // 查询
    EXPECT_EQ(0, db_cli_exec(cli, "SELECT * FROM orders WHERE status = 'completed'"));

    db_cli_destroy(cli);
}

TEST_F(CLITest, UpdateAndDelete) {
    db_cli_t* cli = create_cli("test_cli.db");
    ASSERT_NE(nullptr, cli);

    EXPECT_EQ(0, db_cli_exec(cli, "CREATE TABLE test (id INT, value INT)"));
    EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO test VALUES (1, 100)"));
    EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO test VALUES (2, 200)"));
    EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO test VALUES (3, 300)"));

    // 更新
    EXPECT_EQ(0, db_cli_exec(cli, "UPDATE test SET value = 150 WHERE id = 1"));

    // 删除
    EXPECT_EQ(0, db_cli_exec(cli, "DELETE FROM test WHERE id = 3"));

    db_cli_destroy(cli);
}