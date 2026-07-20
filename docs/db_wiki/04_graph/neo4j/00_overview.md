# Neo4j 项目概览

## 学习目标

- 了解 Neo4j 作为原生图数据库的定位
- 掌握 Neo4j 的属性图模型和 Cypher 查询语言

## 项目定位

> Neo4j 是世界领先的原生生图数据库，以节点和边表示关联关系，擅长处理高度互联的数据。

**基本信息**：
- 开发方：Neo4j, Inc.（原 Neo Technology）
- 首次发布：2007 年
- 开源协议：GPL v3（社区版）/Commercial（企业版）
- GitHub Stars：约 14k

## 核心设计

```mermaid
graph TD
    A[Neo4j] --> B[原生图存储<br/>邻接链表]
    A --> C[属性图模型<br/>Node + Relationship]
    A --> D[Cypher 查询语言<br/>模式匹配]
    A --> E[ACID 事务<br/>原生支持]
    A --> F[索引<br/>BTree/Label]

    B --> B1[O(1) 邻居遍历]
    C --> C1[节点带属性]
    C --> C2[边带属性和类型]
    D --> D1[SQL 风格语法]
    D --> D2[图案导向]
```

## 属性图模型

```cypher
// 节点：带标签和属性
(:Person {name: "Alice", age: 30})

// 关系：带类型和属性
[:KNOWS {since: 2020}]

// 完整示例：社交网络
CREATE (alice:Person {name: "Alice"})
CREATE (bob:Person {name: "Bob"})
CREATE (alice)-[:KNOWS {since: 2020}]->(bob)
CREATE (bob)-[:WORKS_AT {company: "TechCorp"}]->(company:Company {name: "TechCorp"})
```

## 要点总结

- 原生图存储，O(1) 邻居遍历
- 属性图模型支持丰富的语义
- Cypher 查询语言图案导向
- ACID 事务保证数据一致性

## 思考题

1. Neo4j 的原生图存储相比关系数据库的外键连接有何优势？
2. Cypher 的图案匹配与 SQL 的 JOIN 在语义上有何不同？
3. Neo4j 的 Label Index 如何加速节点查询？