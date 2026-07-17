"""
MiniVecDB Python SDK 使用示例
"""
from minivecdb import MiniVecDBClient, Filter

# =============================================================================
# 基本使用
# =============================================================================

def basic_usage():
    """基本使用示例"""
    # 连接服务器
    client = MiniVecDBClient("http://localhost:8080")

    # 健康检查
    print(client.health())

    # 创建集合
    client.create_collection(
        name="test_vectors",
        dimension=128,
        metric_type="L2"
    )

    # 获取集合
    collection = client.get_collection("test_vectors")

    # 插入向量
    collection.insert([
        [0.1] * 128,
        [0.2] * 128,
        [0.3] * 128,
    ])

    # 搜索
    results = collection.search([0.1] * 128, top_k=10)
    print(f"Found {len(results)} results")

    # 删除集合
    client.delete_collection("test_vectors")


# =============================================================================
# 带过滤的搜索
# =============================================================================

def filtered_search():
    """带元数据过滤的搜索"""
    client = MiniVecDBClient("http://localhost:8080")

    # 创建集合
    client.create_collection("filtered", dimension=64)

    collection = client.get_collection("filtered")

    # 插入带元数据的向量
    collection.insert(
        vectors=[
            [0.1] * 64,
            [0.2] * 64,
            [0.3] * 64,
        ],
        metadata=[
            {"category": "A", "score": 0.9},
            {"category": "B", "score": 0.8},
            {"category": "A", "score": 0.7},
        ]
    )

    # 使用过滤器搜索
    f = Filter().equal("category", "A")
    results = collection.search([0.1] * 64, top_k=10, filter=f.to_dict())
    print(f"Found {len(results)} results with category=A")


# =============================================================================
# 上下文管理器
# =============================================================================

def with_context():
    """使用上下文管理器"""
    with MiniVecDBClient("http://localhost:8080") as client:
        collections = client.list_collections()
        print(f"Total collections: {len(collections)}")


# =============================================================================
# 错误处理
# =============================================================================

def error_handling():
    """错误处理示例"""
    from minivecdb.exceptions import (
        MiniVecDBError, NotFoundError, APIError
    )

    client = MiniVecDBClient("http://localhost:8080")

    try:
        # 尝试获取不存在的集合
        client.get_collection("nonexistent")
    except NotFoundError as e:
        print(f"Collection not found: {e}")
    except APIError as e:
        print(f"API error: {e.code} - {e.message}")
    except MiniVecDBError as e:
        print(f"MiniVecDB error: {e}")


if __name__ == "__main__":
    basic_usage()
