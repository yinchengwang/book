-- Build My DB 示例 SQL 脚本
-- 用于演示数据库的基本 CRUD 操作

-- ============================================================
-- 1. 表创建
-- ============================================================

-- 创建用户表
CREATE TABLE users (
    id INT,
    name VARCHAR(100),
    email VARCHAR(200)
);

-- 创建产品表
CREATE TABLE products (
    id INT,
    name VARCHAR(200),
    price DECIMAL(10,2),
    stock INT
);

-- 创建订单表
CREATE TABLE orders (
    order_id INT,
    user_id INT,
    product_id INT,
    quantity INT,
    total DECIMAL(10,2)
);

-- ============================================================
-- 2. 数据插入
-- ============================================================

-- 插入用户
INSERT INTO users VALUES (1, 'Alice Smith', 'alice@example.com');
INSERT INTO users VALUES (2, 'Bob Johnson', 'bob@example.com');
INSERT INTO users VALUES (3, 'Charlie Brown', 'charlie@example.com');
INSERT INTO users VALUES (4, 'Diana Prince', 'diana@example.com');
INSERT INTO users VALUES (5, 'Edward Norton', 'edward@example.com');

-- 插入产品
INSERT INTO products VALUES (1, 'Laptop Pro 15', 9999.00, 50);
INSERT INTO products VALUES (2, 'Wireless Mouse', 199.00, 200);
INSERT INTO products VALUES (3, 'Mechanical Keyboard', 599.00, 150);
INSERT INTO products VALUES (4, 'USB-C Hub', 299.00, 300);
INSERT INTO products VALUES (5, 'Monitor 27inch', 2499.00, 30);

-- 插入订单
INSERT INTO orders VALUES (1001, 1, 1, 1, 9999.00);
INSERT INTO orders VALUES (1002, 1, 2, 2, 398.00);
INSERT INTO orders VALUES (1003, 2, 3, 1, 599.00);
INSERT INTO orders VALUES (1004, 3, 1, 2, 19998.00);
INSERT INTO orders VALUES (1005, 4, 5, 1, 2499.00);

-- ============================================================
-- 3. 查询操作
-- ============================================================

-- 查询所有用户
SELECT * FROM users;

-- 查询所有产品
SELECT * FROM products;

-- 查询所有订单
SELECT * FROM orders;

-- 条件查询：查找 ID 大于 2 的用户
SELECT * FROM users WHERE id > 2;

-- 条件查询：查找价格小于 1000 的产品
SELECT * FROM products WHERE price < 1000;

-- 组合条件：查找 ID 在 1-3 范围内的用户
SELECT * FROM users WHERE id >= 1 AND id <= 3;

-- OR 条件：查找 ID 为 1 或 3 的用户
SELECT * FROM users WHERE id = 1 OR id = 3;

-- NULL 检查：查找 email 为 NULL 的用户（如果支持）
SELECT * FROM users WHERE email IS NULL;

-- NOT 条件：查找 ID 不等于 2 的用户
SELECT * FROM users WHERE NOT id = 2;

-- ============================================================
-- 4. 更新操作
-- ============================================================

-- 更新单行：修改用户邮箱
UPDATE users SET email = 'alice.new@example.com' WHERE id = 1;

-- 更新多列
UPDATE users SET name = 'Robert Smith', email = 'robert@example.com' WHERE id = 1;

-- 无条件更新：所有产品打 9 折
UPDATE products SET price = price * 0.9;

-- 条件更新：库存低于 100 的产品补货
UPDATE products SET stock = 100 WHERE stock < 100;

-- ============================================================
-- 5. 删除操作
-- ============================================================

-- 删除指定订单
DELETE FROM orders WHERE order_id = 1005;

-- 删除所有订单（慎用）
-- DELETE FROM orders;

-- ============================================================
-- 6. 验证结果
-- ============================================================

SELECT * FROM users;
SELECT * FROM products;
SELECT * FROM orders;

-- ============================================================
-- 7. 清理
-- ============================================================

-- 删除测试表
-- DROP TABLE orders;
-- DROP TABLE products;
-- DROP TABLE users;