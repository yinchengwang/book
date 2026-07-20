# Chroma 动手实验

## 学习目标

- 通过实战掌握 Chroma 的使用

## 安装与使用

```bash
pip install chromadb
```

```python
import chromadb
from chromadb.utils import embedding_functions

# 创建客户端
client = chromadb.Client()

# 创建集合
collection = client.create_collection("demo")

# 添加数据
collection.add(
    documents=[
        "Python 是一种高级编程语言",
        "C 语言是一种系统编程语言",
        "Rust 是一种安全的系统语言",
        "Java 是一种面向对象语言",
    ],
    metadatas=[
        {"type": "interpreted", "year": 1991},
        {"type": "compiled", "year": 1972},
        {"type": "compiled", "year": 2015},
        {"type": "compiled", "year": 1995},
    ],
    ids=["py", "c", "rust", "java"]
)

# 查询
results = collection.query(
    query_texts=["哪种语言最安全？"],
    n_results=2,
    where={"type": "compiled"}
)

for doc, dist in zip(results['documents'][0], results['distances'][0]):
    print(f"文档: {doc}, 距离: {dist:.4f}")

# 持久化
client = chromadb.PersistentClient(path="./chroma_db")
```

## 要点总结

- pip install 即刻使用，无需任何基础设施
- 自动处理文本向量化
- 支持 metadata 过滤