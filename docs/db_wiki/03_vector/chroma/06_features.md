# Chroma 关键特性

## 学习目标

- 掌握 Chroma 的核心 API 和用法
- 理解 Chroma 的自动化设计

## 极简 API

```python
import chromadb

# 创建客户端（内存模式）
client = chromadb.Client()

# 或持久化模式
client = chromadb.PersistentClient(path="./chroma_data")

# 创建集合
collection = client.create_collection("my_collection")

# 添加文档（自动向量化）
collection.add(
    documents=[
        "Chroma 是一个向量数据库",
        "Milvus 是云原生向量数据库",
        "Qdrant 用 Rust 实现"
    ],
    metadatas=[
        {"source": "wiki", "category": "vectordb"},
        {"source": "wiki", "category": "vectordb"},
        {"source": "wiki", "category": "vectordb"}
    ],
    ids=["doc1", "doc2", "doc3"]
)

# 搜索（自动向量化查询文本）
results = collection.query(
    query_texts=["什么是向量数据库？"],
    n_results=2,
    where={"source": "wiki"}
)
```

## Embedding 函数

```python
# 默认 Embedding 函数（all-MiniLM-L6-v2）
collection = client.create_collection(
    name="default",
    embedding_function=chromadb.EmbeddingFunction()  # 默认
)

# 自定义 Embedding 函数
def my_embedding(texts):
    return [[0.1] * 128 for _ in texts]

collection = client.create_collection(
    name="custom",
    embedding_function=my_embedding
)
```

## Metadata 过滤

```python
# 过滤
results = collection.query(
    query_texts=["query"],
    where={"category": "vectordb"}          # 精确匹配
)

results = collection.query(
    query_texts=["query"],
    where={"$and": [                        # 组合条件
        {"category": {"$eq": "vectordb"}},
        {"score": {"$gte": 0.5}}
    ]}
)
```

## LLM 框架集成

```python
# LangChain 集成
from langchain_chroma import Chroma
from langchain_openai import OpenAIEmbeddings

vector_store = Chroma(
    collection_name="knowledge",
    embedding_function=OpenAIEmbeddings()
)
```

## 要点总结

- API 极简：add()/query() 几行代码
- 自动向量化：Embedding Function 插件
- Metadata 过滤：$eq/$gte/$and/$or
- LangChain/LlamaIndex 深度集成

## 思考题

1. Chroma 的自动向量化对性能有什么影响？
2. Metadata 过滤的 $and/$or 嵌套深度有限制吗？
3. 从 Chroma 迁移到 Milvus 的成本有多大？