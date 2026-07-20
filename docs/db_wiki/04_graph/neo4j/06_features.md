# Neo4j 关键特性与使用场景

## 关键特性

```mermaid
graph TB
    A[Neo4j 特性] --> B[原生图存储]
    A --> C[Cypher 查询]
    A --> D[ACID 事务]
    A --> E[集群支持]
    A --> F[APOC 库]
    A --> G[GraphQL 支持]

    B --> B1[O(1) 遍历]
    C --> C1[图案匹配]
    C --> C2[路径查询]
    D --> D1[WAL 保证]
    E --> E1[Causal Cluster]
    F --> F1[1800+ 存储过程]
```

## 使用场景

| 场景 | 说明 | 示例 |
|------|------|------|
| 社交网络 | 好友关系、推荐 | 用户关系图 |
| 知识图谱 | 实体关系 | 百科知识 |
| 欺诈检测 | 异常模式 | 交易关联 |
| 推荐系统 | 协同过滤 | 商品推荐 |
| 网络安全 | 攻击路径 | 威胁分析 |

## 欺诈检测示例

```cypher
-- 检测可疑交易模式
MATCH (a:Account)-[:SENT]->(t:Transaction)-[:TO]->(b:Account)
WHERE t.amount > 10000
WITH a, count(t) AS tx_count, collect(t) AS transactions
WHERE tx_count > 10
MATCH path = (a)-[*1..3]-(suspicious:Account)
WHERE suspicious.flag = true
RETURN a.name, tx_count, length(path) AS risk_level
```

## 要点总结

- APOC 库提供丰富的扩展功能
- Causal Cluster 支持强一致性
- GraphQL API 便于前端集成
- 适合高度关联的数据场景