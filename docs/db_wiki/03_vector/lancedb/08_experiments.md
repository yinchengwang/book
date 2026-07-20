# 动手实验: LanceDB 部署与使用

## 学习目标

- 能够快速部署和使用 LanceDB
- 掌握 LanceDB SDK 的基本操作
- 了解多模态数据和版本控制的使用方法

## 安装

```bash
# Python 安装
pip install lancedb

# 依赖项
pip install lancedb pandas numpy
```

## 基本操作

```python
import lancedb
import numpy as np

# 1. 连接数据库（嵌入式，自动创建目录）
db = lancedb.connect("./lancedb_data")

# 2. 创建表
data = [
    {"vector": np.random.rand(128).tolist(), "id": i, "label": f"item_{i}"}
    for i in range(100)
]
table = db.create_table("vectors", data)

# 3. 插入数据
new_data = [
    {"vector": np.random.rand(128).tolist(), "id": 100, "label": "new_item"}
]
table.add(new_data)

# 4. 向量搜索
query_vector = np.random.rand(128).tolist()
results = table.search(query_vector).limit(5).to_pandas()
print(results)

# 5. 标量过滤
results = table.search(query_vector).where("id > 50").limit(10).to_pandas()

# 6. 删除数据
table.delete("id = 100")

# 7. 更新数据（通过 where 条件）
table.update(where="id = 0", values={"label": "updated_item"})

# 8. 查看表结构
print(table.schema)
```

## 多模态数据写入和检索

```python
import lancedb
import numpy as np

db = lancedb.connect("./multimodal_db")

# 创建多模态表
from PIL import Image
import io

# 图像嵌入函数（示例）
def get_image_embedding(image_path):
    return np.random.rand(512).tolist()

# 创建包含图像和文本的表
multimodal_data = [
    {
        "vector": get_image_embedding("img1.jpg"),
        "image_uri": "s3://bucket/img1.jpg",
        "caption": "一只猫在草地上玩耍",
        "width": 1920,
        "height": 1080,
        "created_at": "2024-01-01T10:00:00"
    },
    {
        "vector": get_image_embedding("img2.jpg"),
        "image_uri": "s3://bucket/img2.jpg",
        "caption": "城市的夜景",
        "width": 3840,
        "height": 2160,
        "created_at": "2024-01-02T11:00:00"
    },
    {
        "vector": get_image_embedding("img3.jpg"),
        "image_uri": "s3://bucket/img3.jpg",
        "caption": "海边日落的风景",
        "width": 2560,
        "height": 1440,
        "created_at": "2024-01-03T12:00:00"
    }
]

table = db.create_table("images", multimodal_data)

# 创建向量索引
table.create_index(
    "vector",
    index_type="IVF_PQ",
    metric="cosine",
    num_partitions=2,
    num_sub_vectors=8
)

# 多模态检索：文本→图像
text_embedding = get_text_embedding("美丽的风景")
results = table.search(text_embedding) \
    .where("width > 1920") \
    .limit(5) \
    .to_pandas()

print(results[["image_uri", "caption", "width", "height"]])
```

## 版本控制操作

```python
import lancedb
import datetime

db = lancedb.connect("./versioned_db")
table = db.create_table("versioned_data", [
    {"vector": [0.1] * 128, "id": i, "value": i * 10}
    for i in range(100)
])

# 查看当前版本
print(f"当前版本: {table.version}")

# 追加数据（创建新版本）
table.add([
    {"vector": [0.1] * 128, "id": 100, "value": 1000}
])
print(f"新版本: {table.version}")

# 查看版本历史
versions = table.list_versions()
for v in versions[:5]:  # 显示前 5 个版本
    print(f"版本: {v.version}, 时间: {v.timestamp}, 操作: {v.operation}")

# 查询历史版本
old_table = db.open_table("versioned_data", version=1)
old_data = old_table.to_pandas()
print(f"版本 1 数据量: {len(old_data)}")

# 按时间点查询
past_time = datetime.datetime.now() - datetime.timedelta(hours=1)
try:
    past_table = db.open_table("versioned_data", version=past_time)
    print(f"1 小时前版本: {past_table.version}")
except Exception as e:
    print(f"无法找到该时间点的版本: {e}")

# 版本清理（删除旧版本）
table.cleanup_old_versions(older_than=datetime.timedelta(days=7))
```

## 性能基准测试

```python
import lancedb
import numpy as np
import time

db = lancedb.connect("./benchmark_db")

# 生成测试数据
def generate_data(n):
    return [
        {"vector": np.random.rand(128).tolist(), "id": i, "category": f"cat_{i % 10}"}
        for i in range(n)
    ]

# 写入性能测试
sizes = [1000, 10000, 100000]

for size in sizes:
    data = generate_data(size)
    
    # 创建表
    start = time.time()
    table = db.create_table(f"bench_{size}", data)
    write_time = time.time() - start
    print(f"写入 {size} 条数据: {write_time:.2f}s, QPS: {size / write_time:.0f}")
    
    # 创建索引
    start = time.time()
    table.create_index("vector", index_type="IVF_PQ", num_partitions=16, num_sub_vectors=8)
    index_time = time.time() - start
    print(f"创建索引: {index_time:.2f}s")
    
    # 查询性能测试
    query_vec = np.random.rand(128).tolist()
    iterations = 100
    
    start = time.time()
    for _ in range(iterations):
        _ = table.search(query_vec).limit(10).to_pandas()
    query_time = time.time() - start
    qps = iterations / query_time
    print(f"查询 QPS: {qps:.0f}")
    
    # 过滤查询测试
    start = time.time()
    for _ in range(iterations):
        _ = table.search(query_vec).where("category = 'cat_5'").limit(10).to_pandas()
    filter_query_time = time.time() - start
    filter_qps = iterations / filter_query_time
    print(f"过滤查询 QPS: {filter_qps:.0f}")
    
    print("-" * 40)
```

## 与 SQLite/BTree 格式对比

```python
import lancedb
import sqlite3
import numpy as np
import time

# SQLite 传统存储
def sqlite_benchmark(n=10000):
    conn = sqlite3.connect(":memory:")
    conn.execute("""
        CREATE TABLE vectors (
            id INTEGER PRIMARY KEY,
            embedding BLOB,
            category TEXT
        )
    """)
    
    # 写入
    start = time.time()
    for i in range(n):
        vec = np.random.rand(128).astype(np.float32).tobytes()
        conn.execute("INSERT INTO vectors VALUES (?, ?, ?)", (i, vec, f"cat_{i % 10}"))
    conn.commit()
    write_time = time.time() - start
    print(f"SQLite 写入 {n} 条: {write_time:.2f}s")
    
    # 查询（需要读取所有向量计算距离）
    start = time.time()
    cursor = conn.execute("SELECT id, embedding FROM vectors")
    rows = cursor.fetchall()
    query_time = time.time() - start
    print(f"SQLite 扫描 {n} 条: {query_time:.4f}s")
    
    conn.close()

# LanceDB 存储
def lancedb_benchmark(n=10000):
    db = lancedb.connect("./bench_compare")
    
    # 写入
    data = [
        {"vector": np.random.rand(128).tolist(), "id": i, "category": f"cat_{i % 10}"}
        for i in range(n)
    ]
    start = time.time()
    table = db.create_table("vectors", data)
    write_time = time.time() - start
    print(f"LanceDB 写入 {n} 条: {write_time:.2f}s")
    
    # 创建索引
    table.create_index("vector", index_type="IVF_PQ", num_partitions=16, num_sub_vectors=8)
    
    # 查询（向量索引加速）
    query_vec = np.random.rand(128).tolist()
    start = time.time()
    results = table.search(query_vec).limit(10).to_pandas()
    query_time = time.time() - start
    print(f"LanceDB 向量查询: {query_time:.4f}s")

print("SQLite 基准:")
sqlite_benchmark()
print("\nLanceDB 基准:")
lancedb_benchmark()
```

## 要点总结

- 安装简单：`pip install lancedb` 即可使用
- 基本操作：connect → create_table → add → search → delete/update
- 多模态数据：直接存储图像/文本/视频等原始数据
- 版本控制：Time Travel 查询历史版本
- 性能优势：向量索引加速，比传统行存快数倍

## 思考题

1. 多模态数据写入时，如何选择合适的嵌入模型？
2. 版本控制的历史版本如何设计清理策略？
3. LanceDB 与 Pinecone/Weaviate 相比，在什么场景下更有优势？