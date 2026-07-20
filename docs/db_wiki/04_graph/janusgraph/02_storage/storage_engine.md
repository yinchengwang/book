# JanusGraph 存储与索引

## 学习目标

- 理解 JanusGraph 的后端存储配置
- 掌握 JanusGraph 的 Gremlin 索引机制

## 后端存储配置

```java
// Cassandra 后端配置
graph = JanusGraphFactory.build()
    .set("storage.backend", "cql")
    .set("storage.hostname", "127.0.0.1")
    .set("storage.cql.keyspace", "janusgraph")
    .set("index.search.backend", "elasticsearch")
    .set("index.search.hostname", "127.0.0.1:9200")
    .open();

// HBase 后端配置
graph = JanusGraphFactory.build()
    .set("storage.backend", "hbase")
    .set("storage.hostname", "127.0.0.1")
    .set("storage.hbase.table", "janusgraph")
    .open();
```

## 顶点存储结构

```java
// 顶点存储为一行
// Key:   [partition_key][vertex_id]
// Value: [edge_info][property_info]

// Edge 存储在顶点中
// 每个顶点的边通过链表连接
// ID + Type + Properties + Next Pointer
```

## 索引配置

```java
// 复合索引（精确匹配）
mgmt = graph.openManagement()
name = mgmt.getPropertyKey("name")
mgmt.buildIndex("nameIdx", Vertex.class).addKey(name).buildCompositeIndex()

// 混合索引（范围/全文/地理）
mgmt.buildIndex("nameIdx", Vertex.class)
    .addKey(name, Mapping.STRING.asParameter())
    .buildMixedIndex("search")

// 边索引
mgmt.buildIndex("knowsIdx", Edge.class)
    .addKey(mgmt.getPropertyKey("since"))
    .buildCompositeIndex()
```

## Gremlin 索引查询

```groovy
// 使用索引
g.V().has('person', 'name', 'Alice')

// 强制使用索引
g.V().has('person', 'name', textContains('Alice'))

// 查看索引使用
g.V().has('person', 'name', 'Alice').explain()
```

## 要点总结

- 后端存储完全可插拔
- 索引分为复合索引和混合索引
- Cassandra/HBase 分布式可扩展
- Elasticsearch 提供全文搜索