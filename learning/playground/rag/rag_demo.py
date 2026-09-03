"""
MiniVecDB RAG Demo
基于 MiniVecDB 的检索增强生成示例应用
"""
import os
import sys
from pathlib import Path

# 添加 SDK 到路径
sys.path.insert(0, str(Path(__file__).parent.parent / "sdk" / "python"))

from minivecdb import MiniVecDBClient, Filter
import numpy as np


class RAGDemo:
    """RAG 应用"""

    def __init__(self, db_url: str = "http://localhost:8080"):
        self.client = MiniVecDBClient(db_url)
        self.collection_name = "rag_documents"
        self._setup()

    def _setup(self):
        """初始化集合"""
        # 如果集合已存在，删除
        try:
            self.client.delete_collection(self.collection_name)
        except:
            pass

        # 创建新集合
        self.client.create_collection(
            name=self.collection_name,
            dimension=384,  # 默认 embedding 维度
            metric_type="L2"
        )
        print(f"✓ Collection '{self.collection_name}' created")

    def add_document(
        self,
        text: str,
        doc_id: str,
        metadata: dict = None
    ) -> dict:
        """添加文档"""
        # 简化：使用文本的哈希作为向量
        # 实际应用中应使用真实的 embedding 模型
        vector = self._text_to_vector(text)

        collection = self.client.get_collection(self.collection_name)
        return collection.insert(
            vectors=[vector],
            ids=[doc_id],
            metadata=[metadata or {"text": text[:100]}]
        )

    def _text_to_vector(self, text: str) -> list:
        """文本转向量（简化实现）"""
        # 使用文本的字符 ASCII 码之和作为伪向量
        # 实际应用中应使用 sentence-transformers 等库
        np.random.seed(sum(ord(c) for c in text))
        return (np.random.randn(384) * 0.1).tolist()

    def search(
        self,
        query: str,
        top_k: int = 5,
        filter_metadata: dict = None
    ) -> list:
        """检索相关文档"""
        vector = self._text_to_vector(query)
        collection = self.client.get_collection(self.collection_name)

        f = None
        if filter_metadata:
            f = Filter()
            for k, v in filter_metadata.items():
                f.equal(k, v)
            f = f.to_dict()

        return collection.search(
            query=vector,
            top_k=top_k,
            filter=f,
            include_vector=False
        )

    def add_sample_data(self):
        """添加示例数据"""
        documents = [
            {
                "id": "doc1",
                "text": "MiniVecDB 是一个高性能的向量数据库，支持多种向量索引算法。",
                "category": "database"
            },
            {
                "id": "doc2",
                "text": "RAG（检索增强生成）是一种结合检索和生成的 AI 技术。",
                "category": "ai"
            },
            {
                "id": "doc3",
                "text": "向量数据库广泛应用于语义搜索、推荐系统和问答系统。",
                "category": "database"
            },
            {
                "id": "doc4",
                "text": "HNSW 是一种高效的近似最近邻搜索算法。",
                "category": "algorithm"
            },
            {
                "id": "doc5",
                "text": "Python 是一种广泛使用的高级编程语言。",
                "category": "programming"
            },
        ]

        for doc in documents:
            self.add_document(
                text=doc["text"],
                doc_id=doc["id"],
                metadata={"text": doc["text"], "category": doc["category"]}
            )
        print(f"✓ Added {len(documents)} sample documents")

    def interactive_demo(self):
        """交互式演示"""
        print("\n" + "=" * 60)
        print("MiniVecDB RAG Demo - Interactive Mode")
        print("=" * 60)

        self.add_sample_data()

        print("\n示例查询：")
        queries = [
            "向量数据库的特点",
            "AI 相关技术",
            "搜索算法"
        ]

        for i, query in enumerate(queries, 1):
            print(f"\n查询 {i}: {query}")
            results = self.search(query, top_k=2)
            for r in results:
                print(f"  - {r}")


def main():
    """主函数"""
    print("MiniVecDB RAG Demo")
    print("-" * 40)

    try:
        # 连接数据库
        db_url = os.environ.get("MINIVECDB_URL", "http://localhost:8080")
        rag = RAGDemo(db_url)

        # 运行交互式演示
        rag.interactive_demo()

    except Exception as e:
        print(f"\n✗ Error: {e}")
        print("\n请确保 MiniVecDB 服务器正在运行：")
        print("  ./AlgorithmPractice.exe --mode=server")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
