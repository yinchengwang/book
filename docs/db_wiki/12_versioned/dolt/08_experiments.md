# Dolt 动手实验

## 学习目标

- 掌握 Dolt 的安装和基本操作
- 通过实验验证版本控制功能

## 实验一：安装 Dolt

```bash
# macOS
brew install dolt

# Windows
scoop install dolt

# 验证安装
dolt version
```

## 实验二：创建数据库和分支

```bash
# 初始化数据库
mkdir mydb && cd mydb
dolt init

# 创建表
dolt sql -q "CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(100), age INT)"

# 插入数据
dolt sql -q "INSERT INTO users VALUES (1, '张三', 25), (2, '李四', 30)"

# 创建分支
dolt branch feature-1
dolt checkout feature-1
```

## 实验三：Commit 和 Diff

```bash
# 查看状态
dolt status

# 提交变更
dolt add .
dolt commit -m "初始数据"

# 修改数据
dolt sql -q "UPDATE users SET age = 26 WHERE id = 1"

# 查看 diff
dolt diff

# 提交
dolt add .
dolt commit -m "更新张三年龄"
```

## 实验四：时间旅行查询

```sql
-- 查看提交历史
SELECT * FROM dolt_log;

-- 查询历史数据
SELECT * FROM users AS OF 'HEAD~1';
```

## 要点总结

- dolt CLI 操作类似 Git
- 支持标准 SQL 查询
- 时间旅行查询功能强大

## 思考题

1. 如何解决分支合并冲突？
2. 如何将 Dolt 仓库推送到 DoltHub？
