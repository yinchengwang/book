# SurrealDB 动手实验

## 学习目标

- 掌握 SurrealDB 的 Docker 部署方法
- 通过实验验证多模态功能

## 实验一：Docker 部署 SurrealDB

```bash
# 启动 SurrealDB
docker run -d --name surrealdb \
    -p 8000:8000 \
    surrealdb/surrealdb:latest start \
    --user root --pass root \
    file:/data
```

## 实验二：SurrealQL 操作

```sql
-- 连接数据库
CONNECT http://localhost:8000;
USE NS test DB test;

-- 创建文档
CREATE person SET
    name = '张三',
    age = 25,
    email = 'zhangsan@example.com';

-- 查询文档
SELECT * FROM person WHERE age > 20;

-- 创建关系
RELATE person:1->friend->person:2 SET since = '2024-01-01';

-- 图遍历
SELECT ->friend->person.* FROM person:1;
```

## 实验三：权限管理

```sql
-- 创建 Scope
DEFINE SCOPE app_scope
    SESSION 24h
    SIGNUP (CREATE user SET email = $email)
    SIGNIN (SELECT * FROM user WHERE email = $email);

-- 定义表权限
DEFINE TABLE person
    PERMISSIONS FOR select WHERE true
    FOR create WHERE $auth != NONE;
```

## 要点总结

- Docker 部署简单快速
- SurrealQL 语法类似 SQL，易于学习
- 图遍历查询使用 -> 箭头语法

## 思考题

1. 如何在 SurrealDB 中实现聚合查询？
2. 实时订阅的 WebSocket 连接如何管理？
