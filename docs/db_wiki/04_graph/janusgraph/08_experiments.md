# JanusGraph 动手实验

## 环境准备

```bash
# Docker 启动 JanusGraph + Cassandra + Elasticsearch
docker run -d --name janusgraph \
  -p 8182:8182 \
  janusgraph/janusgraph:latest

# Gremlin Console
docker exec -it janusgraph ./bin/gremlin.sh

# 连接
:remote connect tinkerpop.server conf/remote.yaml
:remote console
```

## 实验 1：创建 Schema

```groovy
// 获取管理 API
mgmt = graph.openManagement()

// 定义属性
name = mgmt.makePropertyKey('name').dataType(String.class).make()
age = mgmt.makePropertyKey('age').dataType(Integer.class).make()

// 定义顶点标签
person = mgmt.makeVertexLabel('person').make()

// 定义边标签
knows = mgmt.makeEdgeLabel('knows').make()

// 提交
mgmt.commit()
```

## 实验 2：插入和查询

```groovy
// 插入顶点
alice = graph.addVertex(label, 'person', 'name', 'Alice', 'age', 30).next()
bob = graph.addVertex(label, 'person', 'name', 'Bob', 'age', 25).next()

// 创建边
alice.addEdge('knows', bob, 'since', 2020)

// 提交事务
graph.tx().commit()

// 查询
g.V().has('person', 'name', 'Alice').outE('knows').inV().values('name')
```

## 实验 3：图遍历

```groovy
// 路径遍历
g.V().has('person', 'name', 'Alice').repeat(out()).times(3).path()

// 聚合
g.V().hasLabel('person').count()

// 过滤
g.V().hasLabel('person').has('age', gt(25))
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 创建 Schema | person 标签, knows 边 | |
| 插入数据 | Alice, Bob 顶点 | |
| 图遍历 | Alice 认识 Bob | |

## 要点总结

- JanusGraph 后端完全可插拔
- Gremlin 是图遍历语言
- 需要手动管理事务
- 支持 OLAP 批处理