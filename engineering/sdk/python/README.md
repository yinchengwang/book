# MiniVecDB Python SDK

MiniVecDB 的 Python 客户端库，提供简洁的 API 来管理向量集合和执行向量搜索。

## 安装

```bash
pip install minivecdb
```

或者从源码安装：

```bash
cd engineering/sdk/python
pip install -e .
```

## 快速开始

```python
from minivecdb import MiniVecDBClient

# 连接服务器
client = MiniVecDBClient("http://localhost:8080")

# 创建集合
client.create_collection(
    name="my_vectors",
    dimension=128,
    metric_type="L2"
)

# 获取集合
collection = client.get_collection("my_vectors")

# 插入向量
collection.insert([
    [0.1, 0.2, ...],  # 128 维向量
    [0.3, 0.4, ...],
])

# 搜索
results = collection.search([0.1, 0.2, ...], top_k=10)
```

## API 参考

### 客户端

#### `MiniVecDBClient(url, timeout, retry)`

创建客户端实例。

- `url`: 服务器地址，默认 `http://localhost:8080`
- `timeout`: 请求超时（秒），默认 30
- `retry`: 重试次数，默认 3

#### 集合操作

- `create_collection(name, dimension, metric_type)` - 创建集合
- `get_collection(name)` - 获取集合
- `list_collections()` - 列出所有集合
- `delete_collection(name)` - 删除集合

#### 健康检查

- `health()` - 健康检查
- `ready()` - 就绪检查
- `live()` - 活跃检查
- `metrics()` - Prometheus 指标

### 集合

通过 `client.get_collection(name)` 获取。

#### 插入

```python
collection.insert(
    vectors=[[0.1, 0.2, ...], ...],
    ids=["id1", "id2", ...],  # 可选
    metadata=[{"key": "value"}, ...]  # 可选
)
```

#### 搜索

```python
results = collection.search(
    query=[0.1, 0.2, ...],  # 查询向量
    top_k=10,               # 返回数量
    filter={"field": value},  # 可选过滤
)
```

## 过滤

使用 `Filter` 类构建复杂查询：

```python
from minivecdb import Filter

# 精确匹配
f = Filter().equal("category", "A")

# 范围查询
f = Filter().greater_than("score", 0.8)

# 组合条件
f = Filter().equal("category", "A").and_filter(
    Filter().less_than("price", 100)
)

# OR 条件
f = Filter().or_filter(
    Filter().equal("category", "A"),
    Filter().equal("category", "B")
)

# 使用过滤器
results = collection.search(query, filter=f.to_dict())
```

## 错误处理

```python
from minivecdb import MiniVecDBClient
from minivecdb.exceptions import NotFoundError, APIError

client = MiniVecDBClient("http://localhost:8080")

try:
    collection = client.get_collection("nonexistent")
except NotFoundError:
    print("Collection not found")
except APIError as e:
    print(f"API Error: {e.code} - {e.message}")
```

## 许可证

MIT License
