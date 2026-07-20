# 图数据库学习资源

## 学习目标

- 收集图数据库学习的优质资源
- 提供源码阅读路径

## 官方资源

| 数据库 | GitHub | 文档 |
|--------|--------|------|
| Neo4j | [neo4j/neo4j](https://github.com/neo4j/neo4j) | [neo4j.com/docs](https://neo4j.com/docs/) |
| NebulaGraph | [vesoft-inc/nebula](https://github.com/vesoft-inc/nebula) | [docs.nebula-graph.io](https://docs.nebula-graph.io/) |
| ArangoDB | [arangodb/arangodb](https://github.com/arangodb/arangodb) | [www.arangodb.com/docs](https://www.arangodb.com/docs/) |
| Dgraph | [dgraph-io/dgraph](https://github.com/dgraph-io/dgraph) | [dgraph.io/docs](https://dgraph.io/docs/) |
| JanusGraph | [JanusGraph/janusgraph](https://github.com/JanusGraph/janusgraph) | [docs.janusgraph.org](https://docs.janusgraph.org/) |

## 源码阅读路径

```
Neo4j:
src/                        # 核心存储和查询
├── store/                  # 存储引擎
├── cypher/                 # Cypher 解析和执行
└── kernel/                 # 事务和锁

NebulaGraph:
src/                        # C++ 实现
├── meta/                   # Meta 服务
├── storage/                # Storage 服务
├── graph/                  # Graph 服务
└── common/                 # 公共库

Dgraph:
dgraph/                     # Go 实现
├── alpha/                  # Alpha 存储节点
├── zero/                   # Zero 协调器
└── badger/                 # Badger 存储
```

## 推荐书籍

| 书名 | 说明 |
|------|------|
| 《图数据库》| GQL 标准参考 |
| 《Neo4j 实战》| Neo4j 最佳实践 |
| 《Graph Data Management》| 图数据库系统论文集 |

## 要点总结

- 官方文档是最佳学习资源
- 源码阅读从存储引擎开始
- 图论基础有助于理解查询优化
- 实践是最好的学习方式