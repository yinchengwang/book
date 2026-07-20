# ArangoDB 存储与事务

## 学习目标

- 理解 ArangoDB 的 MMFiles 和 RocksDB 存储引擎
- 掌握 ArangoDB 的 AQL 查询语言

## 存储引擎对比

| 特性 | MMFiles | RocksDB |
|------|---------|---------|
| 类型 | 内存映射文件 | LSM-Tree |
| 写入性能 | 一般 | 高 |
| 读取性能 | 高 | 较低 |
| 压缩 | 无 | 支持 |
| 适用场景 | 小数据 | 大数据 |

## 文档存储

```aql
// 插入文档
INSERT {
  name: "Alice",
  age: 30,
  address: {
    city: "Beijing",
    country: "China"
  },
  hobbies: ["reading", "traveling"]
} INTO users

// 更新文档
UPDATE "user_key" WITH {
  age: 31
} IN users

// 查询
FOR u IN users
  FILTER u.age > 25 AND u.address.city == "Beijing"
  RETURN u
```

## 事务机制

```aql
// 单集合事务
LET doc = FIRST(FOR u IN users FILTER u.name == "Alice" RETURN u)
UPDATE doc WITH { age: 32 } IN users

// 多集合事务（需开启
// 事务开始
LET result = (
  FOR u IN users
    FILTER u.active == true
    RETURN u
)
// 事务结束

// 隐式事务：单文档操作自动事务
```

## AQL 图查询

```aql
// 图遍历
FOR v, e, p IN 1..3 OUTBOUND "users/Alice" GRAPH "friends"
  RETURN { vertex: v, edge: e, path: p }

// 最短路径
FOR v, e IN OUTBOUND K_SHORTEST_PATHS "users/Alice" TO "users/Bob" GRAPH "friends"
  RETURN { vertex: v, edge: e }

// 模式匹配
FOR u IN users
  FOR friend, knows IN OUTBOUND u._id GRAPH "friends"
    FILTER u.age > 25
    RETURN { user: u.name, friend: friend.name }
```

## 要点总结

- MMFiles 和 RocksDB 两种引擎
- AQL 统一查询文档和图
- 支持单文档隐式事务
- 图遍历支持深度限制