# Neo4j 动手实验

## 环境准备

```bash
# Docker 启动 Neo4j
docker run -d --name neo4j \
  -p 7474:7474 -p 7687:7687 \
  -e NEO4J_AUTH=neo4j/password \
  neo4j:5

# 访问 http://localhost:7474
# 浏览器操作或使用 cypher-shell
```

## 实验 1：创建社交网络

```cypher
-- 创建人物节点
CREATE (alice:Person {name: 'Alice', age: 30})
CREATE (bob:Person {name: 'Bob', age: 25})
CREATE (carol:Person {name: 'Carol', age: 28})
CREATE (dave:Person {name: 'Dave', age: 35})

-- 创建关系
CREATE (alice)-[:KNOWS {since: 2020}]->(bob)
CREATE (bob)-[:KNOWS {since: 2019}]->(carol)
CREATE (carol)-[:KNOWS {since: 2021}]->(dave)
CREATE (alice)-[:KNOWS {since: 2018}]->(carol)
CREATE (dave)-[:KNOWS {since: 2022}]->(alice)

-- 查看所有节点
MATCH (n) RETURN n
```

## 实验 2：图案匹配查询

```cypher
-- 查找 Alice 的朋友
MATCH (alice:Person {name: 'Alice'})-[:KNOWS]->(friend)
RETURN friend.name, friend.age

-- 查找朋友的朋友
MATCH (alice:Person {name: 'Alice'})-[:KNOWS]->()-[:KNOWS]->(fof)
RETURN DISTINCT fof.name

-- 查找最短路径
MATCH path = shortestPath((alice:Person {name: 'Alice'})-[:KNOWS*]-(dave:Person {name: 'Dave'}))
RETURN path
```

## 实验 3：创建索引

```cypher
-- 创建标签索引
CREATE INDEX FOR (p:Person) ON (p.name)

-- 创建属性索引
CREATE INDEX FOR (p:Person) ON (p.age)

-- 查看所有索引
SHOW INDEXES

-- 删除索引
DROP INDEX index_name
```

## 实验 4：执行计划分析

```cypher
-- 查看执行计划
EXPLAIN MATCH (p:Person {name: 'Alice'}) RETURN p

-- 执行并查看计划
PROFILE MATCH (p:Person {name: 'Alice'})-[:KNOWS]->(friend)
RETURN friend.name
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 创建节点 | 4 个 Person 节点 | |
| 创建关系 | 5 条 KNOWS 关系 | |
| 朋友查询 | Bob, Carol | |
| 路径查询 | Alice 到 Dave 的路径 | |
| 索引效果 | EXPLAIN 显示索引使用 | |

## 要点总结

- 使用 Cypher 创建节点和关系
- 图案匹配支持多层遍历
- 索引加速属性查询
- PROFILE 分析执行计划