/*
 * sql_integration.cpp - SQL 集成测试
 *
 * 所有测试基于 SQL 语句，通过 CLI 执行端到端验证
 */

#include <gtest/gtest.h>
#include <db/cli/cli.h>
#include <cstdio>
#include <cstring>
#include <string>

/* ─────────────────────────────────────────────────────────────────
 * 测试夹具
 * ───────────────────────────────────────────────────────────────── */

class SQLIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 生成唯一的数据库文件路径
        snprintf(db_path_, sizeof(db_path_), "test_integration_%d.db", test_id_++);

        // 删除已存在的数据库
        std::remove(db_path_);
        std::remove("test_integration.db");  // 清理旧文件

        // 创建 CLI
        db_cli_config_t config = DB_CLI_DEFAULT_CONFIG;
        config.db_path = db_path_;
        config.echo = false;
        config.show_timing = false;
        config.json_output = false;

        cli_ = db_cli_create(&config);
        ASSERT_NE(nullptr, cli_);
    }

    void TearDown() override {
        if (cli_) {
            db_cli_destroy(cli_);
        }
        // 清理数据库文件
        std::remove(db_path_);
    }

    // 执行 SQL 并检查返回码
    int exec_sql(const char* sql) {
        return db_cli_exec(cli_, sql);
    }

    // 检查 SQL 是否执行成功（返回 0）
    bool exec_ok(const char* sql) {
        return exec_sql(sql) == 0;
    }

    // 创建测试表
    void create_users_table() {
        EXPECT_TRUE(exec_ok("CREATE TABLE users (id INT, name TEXT, email TEXT)"));
    }

    void create_orders_table() {
        EXPECT_TRUE(exec_ok(
            "CREATE TABLE orders ("
            "id INT, customer_id INT, product TEXT, "
            "quantity INT, price REAL, status TEXT"
            ")"
        ));
    }

private:
    db_cli_t* cli_ = nullptr;
    char db_path_[256];
    static int test_id_;
};

int SQLIntegrationTest::test_id_ = 0;

/* ═══════════════════════════════════════════════════════════════════
 * 1. DDL 测试 - CREATE/DROP TABLE
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, CreateTable_Simple) {
    EXPECT_TRUE(exec_ok("CREATE TABLE users (id INT, name TEXT)"));
    EXPECT_TRUE(exec_ok("CREATE TABLE orders (order_id INT, amount REAL)"));
}

TEST_F(SQLIntegrationTest, CreateTable_AllTypes) {
    EXPECT_TRUE(exec_ok(
        "CREATE TABLE products ("
        "  id INT,"
        "  name TEXT,"
        "  price REAL,"
        "  stock INT,"
        "  description BLOB"
        ")"
    ));
}

TEST_F(SQLIntegrationTest, CreateTable_PrimaryKey) {
    EXPECT_TRUE(exec_ok(
        "CREATE TABLE users ("
        "  id INT PRIMARY KEY,"
        "  name TEXT"
        ")"
    ));
}

TEST_F(SQLIntegrationTest, CreateTable_NotNull) {
    EXPECT_TRUE(exec_ok(
        "CREATE TABLE users ("
        "  id INT NOT NULL,"
        "  name TEXT NOT NULL"
        ")"
    ));
}

TEST_F(SQLIntegrationTest, CreateTable_ComplexSchema) {
    EXPECT_TRUE(exec_ok(
        "CREATE TABLE employees ("
        "  id INT,"
        "  name TEXT,"
        "  department TEXT,"
        "  salary REAL,"
        "  hire_date TEXT,"
        "  is_active INT"
        ")"
    ));
}

TEST_F(SQLIntegrationTest, DropTable) {
    EXPECT_TRUE(exec_ok("CREATE TABLE t1 (id INT)"));
    EXPECT_TRUE(exec_ok("DROP TABLE t1"));
}

TEST_F(SQLIntegrationTest, DropTable_IfExists) {
    // 不存在的表也不会报错
    EXPECT_TRUE(exec_ok("DROP TABLE IF EXISTS nonexistent"));
}

TEST_F(SQLIntegrationTest, CreateAndDropMultipleTables) {
    EXPECT_TRUE(exec_ok("CREATE TABLE t1 (id INT)"));
    EXPECT_TRUE(exec_ok("CREATE TABLE t2 (id INT)"));
    EXPECT_TRUE(exec_ok("CREATE TABLE t3 (id INT)"));
    EXPECT_TRUE(exec_ok("DROP TABLE t1"));
    EXPECT_TRUE(exec_ok("DROP TABLE t2"));
    EXPECT_TRUE(exec_ok("DROP TABLE t3"));
}

/* ═══════════════════════════════════════════════════════════════════
 * 2. DML 测试 - INSERT
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Insert_SingleRow) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
}

TEST_F(SQLIntegrationTest, Insert_MultipleRows) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'bob@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (3, 'Charlie', 'charlie@test.com')"));
}

TEST_F(SQLIntegrationTest, Insert_WithColumnList) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users (id, name) VALUES (1, 'Alice')"));
}

TEST_F(SQLIntegrationTest, Insert_NullValues) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', NULL)"));
}

TEST_F(SQLIntegrationTest, Insert_NegativeNumbers) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (-1, 'Negative', 'neg@test.com')"));
}

TEST_F(SQLIntegrationTest, Insert_FloatNumbers) {
    EXPECT_TRUE(exec_ok("CREATE TABLE temps (id INT, value REAL)"));
    EXPECT_TRUE(exec_ok("INSERT INTO temps VALUES (1, 3.14159)"));
    EXPECT_TRUE(exec_ok("INSERT INTO temps VALUES (2, -0.001)"));
    EXPECT_TRUE(exec_ok("INSERT INTO temps VALUES (3, 100.5)"));
}

TEST_F(SQLIntegrationTest, Insert_EscapedQuote) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice''s Shop', 'alice@test.com')"));
}

TEST_F(SQLIntegrationTest, Insert_ChineseCharacters) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, '你好世界', 'hello@世界.com')"));
}

/* ═══════════════════════════════════════════════════════════════════
 * 3. DML 测试 - UPDATE
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Update_Simple) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("UPDATE users SET name = 'Bob' WHERE id = 1"));
}

TEST_F(SQLIntegrationTest, Update_MultipleColumns) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("UPDATE users SET name = 'Bob', email = 'bob@test.com' WHERE id = 1"));
}

TEST_F(SQLIntegrationTest, Update_AllRows) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'bob@test.com')"));
    EXPECT_TRUE(exec_ok("UPDATE users SET email = 'updated@test.com'"));  // 无 WHERE
}

TEST_F(SQLIntegrationTest, Update_WithCondition) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'b@test.com')"));
    EXPECT_TRUE(exec_ok("UPDATE users SET name = 'Updated' WHERE id > 1"));
}

/* ═══════════════════════════════════════════════════════════════════
 * 4. DML 测试 - DELETE
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Delete_WithCondition) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'bob@test.com')"));
    EXPECT_TRUE(exec_ok("DELETE FROM users WHERE id = 1"));
}

TEST_F(SQLIntegrationTest, Delete_AllRows) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("DELETE FROM users"));
}

TEST_F(SQLIntegrationTest, Delete_MultipleRows) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'b@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (3, 'Charlie', 'c@test.com')"));
    EXPECT_TRUE(exec_ok("DELETE FROM users WHERE id < 3"));
}

/* ═══════════════════════════════════════════════════════════════════
 * 5. SELECT 测试 - 基本查询
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Select_AllColumns) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("SELECT * FROM users"));
}

TEST_F(SQLIntegrationTest, Select_SpecificColumns) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("SELECT id, name FROM users"));
}

TEST_F(SQLIntegrationTest, Select_EmptyTable) {
    create_users_table();
    EXPECT_TRUE(exec_ok("SELECT * FROM users"));  // 空表查询
}

TEST_F(SQLIntegrationTest, Select_MultipleTables) {
    create_users_table();
    create_orders_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'alice@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO orders VALUES (1, 1, 'Product A', 2, 10.0, 'pending')"));
}

/* ═══════════════════════════════════════════════════════════════════
 * 6. WHERE 条件测试
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Where_Equals) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'b@test.com')"));
    EXPECT_TRUE(exec_ok("SELECT * FROM users WHERE id = 1"));
}

TEST_F(SQLIntegrationTest, Where_NotEquals) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'b@test.com')"));
    EXPECT_TRUE(exec_ok("SELECT * FROM users WHERE id <> 1"));
}

TEST_F(SQLIntegrationTest, Where_LessThan) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'b@test.com')"));
    EXPECT_TRUE(exec_ok("SELECT * FROM users WHERE id < 2"));
}

TEST_F(SQLIntegrationTest, Where_GreaterThan) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'b@test.com')"));
    EXPECT_TRUE(exec_ok("SELECT * FROM users WHERE id > 1"));
}

TEST_F(SQLIntegrationTest, Where_And) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'b@test.com')"));
    EXPECT_TRUE(exec_ok("SELECT * FROM users WHERE id > 0 AND name = 'Alice'"));
}

TEST_F(SQLIntegrationTest, Where_Or) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Bob', 'b@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (3, 'Charlie', 'c@test.com')"));
    EXPECT_TRUE(exec_ok("SELECT * FROM users WHERE id = 1 OR id = 3"));
}

TEST_F(SQLIntegrationTest, Where_ComplexConditions) {
    create_orders_table();
    EXPECT_TRUE(exec_ok("INSERT INTO orders VALUES (1, 1, 'Product A', 2, 100.0, 'completed')"));
    EXPECT_TRUE(exec_ok("INSERT INTO orders VALUES (2, 1, 'Product B', 1, 200.0, 'pending')"));
    EXPECT_TRUE(exec_ok("INSERT INTO orders VALUES (3, 2, 'Product C', 3, 150.0, 'completed')"));
    EXPECT_TRUE(exec_ok(
        "SELECT * FROM orders WHERE status = 'completed' AND price > 120"
    ));
}

/* ═══════════════════════════════════════════════════════════════════
 * 7. 端到端场景测试
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, E2E_UserRegistration) {
    // 模拟用户注册流程
    EXPECT_TRUE(exec_ok("CREATE TABLE users ("
        "id INT PRIMARY KEY,"
        "username TEXT NOT NULL,"
        "email TEXT,"
        "created_at TEXT"
        ")"));

    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'alice', 'alice@test.com', '2024-01-01')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'bob', 'bob@test.com', '2024-01-02')"));

    // 查询用户
    EXPECT_TRUE(exec_ok("SELECT * FROM users WHERE username = 'alice'"));
    EXPECT_TRUE(exec_ok("UPDATE users SET email = 'alice.new@test.com' WHERE username = 'alice'"));
}

TEST_F(SQLIntegrationTest, E2E_OrderProcessing) {
    // 创建表
    EXPECT_TRUE(exec_ok("CREATE TABLE customers (id INT, name TEXT)"));
    EXPECT_TRUE(exec_ok("CREATE TABLE orders (id INT, customer_id INT, total REAL)"));

    // 插入客户
    EXPECT_TRUE(exec_ok("INSERT INTO customers VALUES (1, 'Alice')"));
    EXPECT_TRUE(exec_ok("INSERT INTO customers VALUES (2, 'Bob')"));

    // 创建订单
    EXPECT_TRUE(exec_ok("INSERT INTO orders VALUES (1, 1, 100.0)"));
    EXPECT_TRUE(exec_ok("INSERT INTO orders VALUES (2, 1, 200.0)"));
    EXPECT_TRUE(exec_ok("INSERT INTO orders VALUES (3, 2, 150.0)"));

    // 查询客户的订单
    EXPECT_TRUE(exec_ok("SELECT * FROM orders WHERE customer_id = 1"));

    // 更新订单
    EXPECT_TRUE(exec_ok("UPDATE orders SET total = 180.0 WHERE id = 2"));

    // 删除订单
    EXPECT_TRUE(exec_ok("DELETE FROM orders WHERE id = 3"));
}

TEST_F(SQLIntegrationTest, E2E_ProductCatalog) {
    // 创建商品目录
    EXPECT_TRUE(exec_ok("CREATE TABLE categories (id INT, name TEXT)"));
    EXPECT_TRUE(exec_ok("CREATE TABLE products ("
        "id INT, name TEXT, category_id INT, "
        "price REAL, stock INT, description TEXT"
        ")"));

    // 添加分类
    EXPECT_TRUE(exec_ok("INSERT INTO categories VALUES (1, 'Electronics')"));
    EXPECT_TRUE(exec_ok("INSERT INTO categories VALUES (2, 'Books')"));

    // 添加商品
    EXPECT_TRUE(exec_ok("INSERT INTO products VALUES (1, 'Laptop', 1, 999.99, 10, 'High-end laptop')"));
    EXPECT_TRUE(exec_ok("INSERT INTO products VALUES (2, 'Mouse', 1, 29.99, 100, 'Wireless mouse')"));
    EXPECT_TRUE(exec_ok("INSERT INTO products VALUES (3, 'Python Book', 2, 49.99, 50, 'Learn Python')"));

    // 查询
    EXPECT_TRUE(exec_ok("SELECT * FROM products WHERE category_id = 1"));
    EXPECT_TRUE(exec_ok("SELECT * FROM products WHERE price < 100"));

    // 更新库存
    EXPECT_TRUE(exec_ok("UPDATE products SET stock = stock - 1 WHERE id = 1"));
}

/* ═══════════════════════════════════════════════════════════════════
 * 8. 边界测试
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, EdgeCase_SpecialCharacters) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Hello\\nWorld', 'test@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (2, 'Tab\\there', 'tab@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (3, 'Quote''s Test', 'quote@test.com')"));
}

TEST_F(SQLIntegrationTest, EdgeCase_LongStrings) {
    create_users_table();
    std::string long_name(1000, 'x');
    std::string sql = "INSERT INTO users VALUES (1, '" + long_name + "', 'long@test.com')";
    EXPECT_TRUE(exec_ok(sql.c_str()));
}

TEST_F(SQLIntegrationTest, EdgeCase_ZeroAndNegative) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (0, 'Zero', 'zero@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (-1, 'Negative', 'neg@test.com')"));
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (-999, 'LargeNeg', 'ln@test.com')"));
}

TEST_F(SQLIntegrationTest, EdgeCase_LargeNumbers) {
    EXPECT_TRUE(exec_ok("CREATE TABLE numbers (id INT, value REAL)"));
    EXPECT_TRUE(exec_ok("INSERT INTO numbers VALUES (1, 999999999.99)"));
    EXPECT_TRUE(exec_ok("INSERT INTO numbers VALUES (2, -999999999.99)"));
    EXPECT_TRUE(exec_ok("INSERT INTO numbers VALUES (3, 0.000001)"));
}

/* ═══════════════════════════════════════════════════════════════════
 * 9. 错误处理测试
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Error_TableNotFound) {
    EXPECT_FALSE(exec_ok("SELECT * FROM nonexistent_table"));
}

TEST_F(SQLIntegrationTest, Error_InvalidSQL) {
    EXPECT_FALSE(exec_ok("SELEKT * FORM users"));  // 拼写错误
}

TEST_F(SQLIntegrationTest, Error_MissingTable) {
    EXPECT_FALSE(exec_ok("INSERT INTO users VALUES (1)"));  // 表不存在
}

TEST_F(SQLIntegrationTest, Error_SyntaxError) {
    EXPECT_FALSE(exec_ok("INSERT INTO users VALUES"));  // 缺少值
}

TEST_F(SQLIntegrationTest, Error_InvalidOperator) {
    create_users_table();
    EXPECT_FALSE(exec_ok("SELECT * FROM users WHERE id === 1"));  // 无效操作符
}

/* ═══════════════════════════════════════════════════════════════════
 * 10. 压力测试
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Stress_ManyInserts) {
    EXPECT_TRUE(exec_ok("CREATE TABLE test (id INT, data TEXT)"));

    // 插入 100 条记录
    for (int i = 0; i < 100; i++) {
        char sql[128];
        snprintf(sql, sizeof(sql), "INSERT INTO test VALUES (%d, 'data_%d')", i, i);
        EXPECT_TRUE(exec_ok(sql));
    }

    // 查询验证
    EXPECT_TRUE(exec_ok("SELECT * FROM test WHERE id = 50"));
}

TEST_F(SQLIntegrationTest, Stress_ManyTables) {
    // 创建 10 个表
    for (int i = 0; i < 10; i++) {
        char sql[128];
        snprintf(sql, sizeof(sql), "CREATE TABLE table_%d (id INT, data TEXT)", i);
        EXPECT_TRUE(exec_ok(sql));
    }
}

TEST_F(SQLIntegrationTest, Stress_ComplexQueries) {
    create_orders_table();

    // 插入测试数据
    for (int i = 0; i < 50; i++) {
        char sql[256];
        snprintf(sql, sizeof(sql),
            "INSERT INTO orders VALUES (%d, %d, 'Product_%d', %d, %.2f, '%s')",
            i, i % 5, i, (i % 5) + 1, (i % 100) + 1.0,
            i % 2 == 0 ? "completed" : "pending");
        EXPECT_TRUE(exec_ok(sql));
    }

    // 复杂查询
    EXPECT_TRUE(exec_ok(
        "SELECT * FROM orders WHERE status = 'completed' AND quantity > 2"
    ));
}

/* ═══════════════════════════════════════════════════════════════════
 * 11. 事务测试（基础）
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Transaction_Basic) {
    create_users_table();
    EXPECT_TRUE(exec_ok("INSERT INTO users VALUES (1, 'Alice', 'a@test.com')"));
    // 注意：当前简化实现可能不支持显式事务
}

/* ═══════════════════════════════════════════════════════════════════
 * 12. 数据持久化测试
 * ═══════════════════════════════════════════════════════════════════ */

TEST_F(SQLIntegrationTest, Persistence_DataSurvivesClose) {
    {
        // 第一个会话：创建表并插入数据
        db_cli_config_t config = DB_CLI_DEFAULT_CONFIG;
        config.db_path = "test_persistence.db";
        config.echo = false;
        config.show_timing = false;

        db_cli_t* cli = db_cli_create(&config);
        ASSERT_NE(nullptr, cli);

        EXPECT_EQ(0, db_cli_exec(cli, "CREATE TABLE users (id INT, name TEXT)"));
        EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO users VALUES (1, 'Alice')"));
        EXPECT_EQ(0, db_cli_exec(cli, "INSERT INTO users VALUES (2, 'Bob')"));

        db_cli_destroy(cli);
    }

    {
        // 第二个会话：验证数据存在
        db_cli_config_t config = DB_CLI_DEFAULT_CONFIG;
        config.db_path = "test_persistence.db";
        config.echo = false;
        config.show_timing = false;

        db_cli_t* cli = db_cli_create(&config);
        ASSERT_NE(nullptr, cli);

        EXPECT_EQ(0, db_cli_exec(cli, "SELECT * FROM users"));  // 应该成功

        db_cli_destroy(cli);
    }

    // 清理
    std::remove("test_persistence.db");
}