# Chroma 配置选项

## 学习目标

- 掌握 Chroma 的核心配置项
- 理解持久化、Embedding、过滤等配置的用法
- 学会根据场景调整配置参数

## 持久化配置

Chroma 支持两种运行模式，通过客户端类型选择：

```python
# 内存模式：数据仅存在于进程内存，关闭后丢失
client = chromadb.Client()

# 持久化模式：数据存储在磁盘，重启后可恢复
client = chromadb.PersistentClient(path="./chroma_data")
```

### 持久化参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `path` | str | `./chroma_data` | 数据存储目录路径 |
| `settings` | Settings | 默认设置 | 详细配置对象 |

### Settings 配置

```python
from chromadb.config import Settings

# 自定义持久化配置
client = chromadb.PersistentClient(
    path="./my_chroma_db",
    settings=Settings(
        anonymized_telemetry=False,    # 关闭匿名遥测
        allow_reset=True,              # 允许重置数据库
        is_persistent=True,            # 开启持久化
        persist_directory="./my_chroma_db"  # 与 path 对应
    )
)

# HTTP 客户端模式（远程 Chroma 服务）
client = chromadb.HttpClient(
    host="localhost",
    port=8000
)
```

### 存储目录结构

```
chroma_data/
├── chroma.sqlite3          # 元数据存储（SQLite）
├── <uuid>/                 # 每个集合一个目录
│   ├── header.bin          # 集合头信息
│   ├── length.bin          # 向量数量
│   ├── link_list.bin       # HNSW 图结构
│   └── vectors.bin         # 向量数据
└── ...                     # 其他集合目录
```

## Embedding Function 配置

### 默认 Embedding 函数

Chroma 使用 `all-MiniLM-L6-v2` 作为默认 Embedding 模型，维度 384。

```python
# 默认自动使用
collection = client.create_collection(name="my_collection")

# 等价于显式指定
from chromadb.utils import embedding_functions
default_ef = embedding_functions.DefaultEmbeddingFunction()
collection = client.create_collection(
    name="my_collection",
    embedding_function=default_ef
)
```

### 自定义 Embedding 函数

```python
# 1. OpenAI Embedding
from chromadb.utils import embedding_functions

openai_ef = embedding_functions.OpenAIEmbeddingFunction(
    api_key="sk-...",
    model_name="text-embedding-3-small"  # 维度 1536
)

# 2. HuggingFace Embedding
hf_ef = embedding_functions.HuggingFaceEmbeddingFunction(
    api_key="hf_...",
    model_name="sentence-transformers/all-MiniLM-L6-v2"
)

# 3. ONNX 模型（本地运行）
onnx_ef = embedding_functions.ONNXEmbeddingFunction(
    model_path="./model.onnx",
    tokenizer_path="./tokenizer"
)

# 4. 自定义函数（任意逻辑）
def custom_embedding(texts: list[str]) -> list[list[float]]:
    """接收文本列表，返回向量列表"""
    # 可以是调用远程 API、本地模型、甚至是随机向量
    return [[0.1] * 128 for _ in texts]

collection = client.create_collection(
    name="custom_collection",
    embedding_function=custom_embedding
)
```

### Embedding Function 选择

| 模型 | 维度 | 速度 | 质量 | 是否需要网络 |
|------|------|------|------|-------------|
| 默认 (all-MiniLM-L6-v2) | 384 | 快 | 中等 | 首次下载 |
| OpenAI text-embedding-3-small | 1536 | 慢（API） | 高 | 是 |
| OpenAI text-embedding-3-large | 3072 | 慢（API） | 最高 | 是 |
| HuggingFace 模型 | 各异 | 中等 | 中-高 | 首次下载 |
| ONNX 本地模型 | 各异 | 快 | 中-高 | 否 |

## Metadata 过滤配置

Chroma 的 Metadata 过滤支持多种操作符：

```python
# 精确匹配
collection.query(
    where={"category": "vectordb"}
)

# 逻辑运算符
where = {"$and": [
    {"category": {"$eq": "vectordb"}},
    {"score": {"$gte": 0.8}},
    {"version": {"$ne": "deprecated"}}
]}

# 包含关系
where = {"tags": {"$in": ["python", "database"]}}
where = {"tags": {"$nin": ["experimental"]}}

# 范围过滤
where = {
    "$and": [
        {"price": {"$gte": 10, "$lte": 100}},
        {"rating": {"$gte": 4.0}}
    ]
}
```

### 过滤操作符

| 操作符 | 含义 | 示例 |
|--------|------|------|
| `$eq` | 等于 | `{"key": {"$eq": "val"}}` |
| `$ne` | 不等于 | `{"key": {"$ne": "val"}}` |
| `$gt` | 大于 | `{"key": {"$gt": 10}}` |
| `$gte` | 大于等于 | `{"key": {"$gte": 10}}` |
| `$lt` | 小于 | `{"key": {"$lt": 10}}` |
| `$lte` | 小于等于 | `{"key": {"$lte": 10}}` |
| `$in` | 在集合中 | `{"key": {"$in": ["a","b"]}}` |
| `$nin` | 不在集合中 | `{"key": {"$nin": ["a","b"]}}` |
| `$and` | 逻辑与 | `{"$and": [cond1, cond2]}` |
| `$or` | 逻辑或 | `{"$or": [cond1, cond2]}` |

## 批处理参数

批量操作时，`batch_size` 控制每次写入的文档数：

```python
# 批量添加
collection.add(
    documents=large_doc_list,      # 大量文档
    metadatas=large_meta_list,
    ids=large_id_list,
    batch_size=1000                 # 每批 1000 条
)

# 批量更新
collection.update(
    ids=ids_to_update,
    documents=new_docs,
    batch_size=500
)

# 批量删除
collection.delete(
    where={"status": "obsolete"},
    batch_size=1000
)
```

### batch_size 影响

| batch_size | 优点 | 缺点 | 适合场景 |
|-----------|------|------|---------|
| 小（100-500） | 内存占用低 | 事务开销大 | 资源受限环境 |
| 中（500-2000） | 平衡 | 平衡 | 推荐默认 |
| 大（2000+） | 吞吐量高 | 内存占用高 | 批量导入 |

## 要点总结

- 持久化通过 `PersistentClient` 实现，数据存储在 SQLite + 二进制文件
- Embedding Function 支持默认、OpenAI、HuggingFace、ONNX、自定义等多种模式
- Metadata 过滤支持 `$eq/$gte/$and/$or/$in` 等操作符
- `batch_size` 控制批量写入粒度，默认值适合多数场景
- 配置选择需根据使用场景（原型/生产）、资源（内存/网络）和数据规模决定

## 思考题

1. Chroma 的持久化实现中，为什么元数据用 SQLite 而向量用二进制文件？
2. 如果使用自定义 Embedding Function，如何保证插入和查询时的一致性？
3. Metadata 过滤是在向量搜索之前还是之后执行？这对性能有什么影响？
4. 在批量导入 100 万条数据时，如何选择合适的 batch_size？