# Dgraph 动手实验

## 环境准备

```bash
# Docker 启动 Dgraph Standalone
docker run -d --name dgraph \
  -p 8080:8080 -p 9080:9080 -p 8000:8000 \
  dgraph/standalone:v23.1

# Ratel UI: http://localhost:8000
# gRPC: localhost:9080
# HTTP: localhost:8080
```

## 实验 1：创建 Schema

```graphql
# 创建 Schema
type Person {
  name: string @index(term)
  age: int @index(int)
  knows: [Person]
}

# 通过 HTTP API
curl -X POST localhost:8080/admin/schema --data-binary 'schema.graphql'
```

## 实验 2：插入数据

```graphql
mutation {
  set {
    <alice> <name> "Alice" .
    <alice> <age> "30" .
    <bob> <name> "Bob" .
    <bob> <age> "25" .
    <alice> <knows> <bob> .
  }
}
```

## 实验 3：查询

```graphql
# 基础查询
{
  query(func: eq(name, "Alice")) {
    name
    age
  }
}

# 图案匹配
{
  query(func: eq(name, "Alice")) {
    name
    knowsperson: knows {
      name
    }
  }
}
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 创建 Schema | Person 类型 | |
| 插入数据 | Alice, Bob 顶点 | |
| 图案匹配 | Alice 的朋友 Bob | |

## 要点总结

- Dgraph 提供 Ratel UI
- DQL 类似 GraphQL 语法
- 图案匹配天然支持
- 支持 trillion 级别数据