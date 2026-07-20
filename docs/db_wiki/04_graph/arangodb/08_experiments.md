# ArangoDB 动手实验

## 环境准备

```bash
# Docker 启动 ArangoDB
docker run -d --name arangodb \
  -p 8529:8529 \
  -e ARANGO_ROOT_PASSWORD=test \
  arangodb:3.11

# 访问 http://localhost:8529
# 用户名: root
# 密码: test
```

## 实验 1：创建集合

```aql
// 创建文档集合
CREATE COLLECTION users;

// 创建边集合（图边）
CREATE GRAPH social {
  edgeCollections: {
    knows: {
      from: [users],
      to: [users]
    }
  }
}
```

## 实验 2：插入文档

```aql
// 插入文档
INSERT {
  name: "Alice",
  age: 30,
  hobbies: ["reading", "traveling"]
} INTO users

// 批量插入
FOR i IN 1..100
  INSERT {
    name: CONCAT("User", i),
    age: 20 + (i % 50)
  } INTO users
```

## 实验 3：图遍历

```aql
// 图遍历
FOR v, e, p IN 1..3 OUTBOUND "users/123" GRAPH "social"
  RETURN { vertex: v, edge: e, path: p }

// 路径查询
FOR v, e, p IN OUTBOUND K_SHORTEST_PATHS
  "users/123" TO "users/456"
  GRAPH "social"
  RETURN p
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 创建集合 | users 集合 | |
| 插入文档 | 100 个用户文档 | |
| 图遍历 | 路径信息 | |

## 要点总结

- ArangoDB 一套系统支持多模型
- AQL 统一查询文档和图
- 边集合自动维护 from/to 约束
- 图遍历支持深度限制