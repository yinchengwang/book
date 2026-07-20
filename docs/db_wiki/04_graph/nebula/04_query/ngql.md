# NebulaGraph 查询与索引

## 学习目标

- 掌握 nGQL 查询语言
- 理解 NebulaGraph 的索引机制

## nGQL 查询

```ngql
-- 基础查询
LOOKUP ON person WHERE person.name == "Alice" YIELD person.name, person.age;

-- 图遍历
GO FROM "Alice" OVER know YIELD dst(edge) AS friend;

-- 多跳查询
GO 2 STEPS FROM "Alice" OVER know YIELD dst(edge) AS friend;

-- 模式匹配
MATCH (a:Person)-[:KNOWS]->(b:Person)
WHERE a.name == "Alice"
RETURN b.name;
```

## 索引机制

```ngql
-- 创建索引
CREATE TAG INDEX idx_person_name ON person(name(10));
CREATE EDGE INDEX idx_know_likeness ON know(likeness);

-- 重建索引
REBUILD TAG INDEX idx_person_name;

-- 查看索引
SHOW TAG INDEXES;

-- 使用索引
LOOKUP ON person WHERE person.name == "Alice";
```

## 查询优化

```ngql
-- 使用管道
GO FROM "Alice" OVER know YIELD dst(edge) AS fid | GO FROM $-.fid OVER know YIELD dst(edge) AS friend;

-- 聚合
YIELD dst(edge) AS fid | GROUP BY $-.fid YIELD count(*) AS cnt;

-- LIMIT
GO FROM "Alice" OVER know YIELD dst(edge) AS friend | LIMIT 10;
```

## 要点总结

- nGQL 类 SQL，易学易用
- LOOKUP 利用索引加速
- 管道操作支持链式查询
- 支持模式匹配（MATCH）