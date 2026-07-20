# Qdrant 使用场景

## 学习目标

- 掌握 Qdrant 的典型应用场景
- 理解场景与特性的对应关系

## 推荐系统多样性

```mermaid
graph LR
    USER[用户请求] --> REC[推荐系统]
    REC --> EMB[用户向量化]
    EMB --> QDRANT[Qdrant 分组搜索<br/>group_by=category_id]
    QDRANT --> R1[每个类别取 Top-3]
    R1 --> RES[混合排序后推荐]
```

- 分组搜索保证推荐结果的多样性
- 每个类别只取少量结果
- 避免所有推荐都是同一类别

## 语义搜索 + 精确过滤

```python
# 电商搜索：语义 + 价格 + 品牌
results = client.search(
    collection_name="products",
    query_vector=text_embedding,
    query_filter=models.Filter(
        must=[
            models.FieldCondition(
                key="price",
                range=models.Range(gte=100, lte=500)
            ),
            models.FieldCondition(
                key="brand",
                match=models.MatchValue(value="Apple")
            ),
            models.FieldCondition(
                key="in_stock",
                match=models.MatchValue(value=True)
            )
        ]
    ),
    limit=20
)
```

## 地理位置搜索

```python
# 附近门店搜索
results = client.search(
    collection_name="stores",
    query_vector=embedding,
    query_filter=models.Filter(
        must=[
            models.FieldCondition(
                key="location",
                geo_radius=models.GeoRadius(
                    center=models.GeoPoint(lon=116.4, lat=39.9),
                    radius=5000
                )
            )
        ]
    ),
    limit=10
)
```

## 场景选择矩阵

| 场景 | Qdrant 特性 | 推荐理由 |
|------|-------------|---------|
| 推荐系统 | 分组搜索 | 多样性保障 |
| 电商搜索 | Payload 过滤 | 价格/品牌等多维过滤 |
| 位置服务 | 地理过滤 | 原生地理索引 |
| 高并发 API | Rust 性能 | 单机 QPS 极高 |
| 多模态检索 | 多向量 | 文本+图像联合 |

## 要点总结

- 分组搜索在推荐系统中实现多样性
- Payload 过滤让语义搜索结合精确条件
- 地理搜索是 LBS 应用的利器
- Rust 性能适合高并发场景

## 思考题

1. 推荐系统中分组搜索的 group_size 设置多大比较合理？
2. 电商搜索中，多个过滤条件的执行顺序如何影响性能？
3. 地理搜索中 Geo Radius 和 Geo Bounding Box 各适合什么场景？