# Neo4j 查询与性能

## 学习目标

- 掌握 Cypher 查询语言的图案匹配
- 理解 Neo4j 的查询执行计划

## Cypher 查询

```cypher
-- 基础匹配
MATCH (p:Person {name: 'Alice'}) RETURN p;

-- 图案匹配
MATCH (a:Person)-[:KNOWS]->(b:Person)-[:KNOWS]->(c:Person)
WHERE a.name = 'Alice'
RETURN c.name;

-- 路径变量
MATCH path = (a:Person)-[:KNOWS*1..3]->(b:Person)
WHERE a.name = 'Alice'
RETURN path, length(path);

-- 聚合查询
MATCH (p:Person)-[r:KNOWS]->(friend:Person)
RETURN p.name, count(friend) AS friend_count
ORDER BY friend_count DESC;
```

## 执行计划

```cypher
-- 查看执行计划
EXPLAIN MATCH (p:Person {name: 'Alice'}) RETURN p;

-- 详细统计
PROFILE MATCH (p:Person {name: 'Alice'}) RETURN p;

-- 执行计划示例
// -> NodeByLabelScan          // 扫描标签
//    -> Filter                // 过滤属性
//       -> ProducingResults   // 返回结果
```

## 性能优化

```cypher
-- 使用参数而非字面量
MATCH (p:Person {name: $name}) RETURN p;

-- 使用索引
MATCH (p:Person) WHERE p.email = 'alice@example.com' RETURN p;

-- 限制结果数量
MATCH (p:Person) RETURN p LIMIT 100;

-- 提前过滤
MATCH (p:Person)
WHERE p.age > 25
WITH p LIMIT 1000
MATCH (p)-[:KNOWS]->(f:Person)
RETURN f;
```

## 要点总结

- Cypher 图案导向，声明式查询
- 使用 EXPLAIN/PROFILE 分析计划
- 索引是性能关键
- 参数化查询复用执行计划

## 思考题

1. Cypher 的图案匹配与 SQL 的 JOIN 在执行计划上有何不同？
2. 如何优化多层嵌套的图案查询？
3. Neo4j 的查询缓存机制是什么？