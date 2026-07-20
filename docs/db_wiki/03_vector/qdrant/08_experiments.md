# 动手实验: Qdrant 部署与使用

## 学习目标

- 能够快速部署 Qdrant
- 掌握 Qdrant SDK 的基本使用

## Docker 部署

```bash
# 单机部署
docker run -d --name qdrant -p 6333:6333 -p 6334:6334 qdrant/qdrant

# 验证
curl http://localhost:6333/telemetry
```

## Python SDK 使用

```bash
pip install qdrant-client
```

```python
from qdrant_client import QdrantClient, models
import numpy as np

# 1. 连接
client = QdrantClient(host="localhost", port=6333)

# 2. 创建集合
client.create_collection(
    collection_name="demo",
    vectors_config=models.VectorParams(
        size=128,
        distance=models.Distance.COSINE
    )
)

# 3. 插入数据
points = [
    models.PointStruct(
        id=i,
        vector=np.random.rand(128).tolist(),
        payload={
            "category": f"cat_{i % 10}",
            "price": i * 10,
            "city": ["北京", "上海", "深圳"][i % 3]
        }
    ) for i in range(1000)
]
client.upsert(collection_name="demo", points=points)

# 4. 创建 Payload 索引
client.create_payload_index("demo", "city", models.PayloadIndexType.KEYWORD)
client.create_payload_index("demo", "price", models.PayloadIndexType.FLOAT)

# 5. 搜索
results = client.search(
    collection_name="demo",
    query_vector=np.random.rand(128).tolist(),
    query_filter=models.Filter(
        must=[
            models.FieldCondition(
                key="city",
                match=models.MatchValue(value="北京")
            ),
            models.FieldCondition(
                key="price",
                range=models.Range(gte=20, lte=80)
            )
        ]
    ),
    limit=5,
    with_payload=True
)

# 6. 分组搜索
group_results = client.search_groups(
    collection_name="demo",
    query_vector=np.random.rand(128).tolist(),
    group_by="category",
    group_size=3,
    limit=100
)

# 7. 查看集合信息
collection_info = client.get_collection("demo")
print(f"Points count: {collection_info.points_count}")
print(f"Segments count: {collection_info.segments_count}")

# 清理
client.delete_collection("demo")
```

## 性能基准测试

```python
import time

# QPS 测试
start = time.time()
for _ in range(100):
    _ = client.search(
        collection_name="demo",
        query_vector=vec,
        limit=10
    )
qps = 100 / (time.time() - start)
print(f"QPS: {qps:.0f}")
```

## 要点总结

- Docker 方式即刻部署 Qdrant
- 操作流程：创建集合 → 插入数据 → 创建索引 → 搜索
- Payload 索引需手动创建
- 分组搜索是 Qdrant 的独特能力

## 思考题

1. 测试中 Payload 索引对过滤性能的影响有多大？
2. 1000 条数据的 QPS 和 100 万条数据的 QPS 差距有多大？
3. 不同的 distance 类型（COSINE/DOT/IP）对搜索精度有什么影响？