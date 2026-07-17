import re
import logging
from typing import Optional
from app.services.aggregator.base import AggregatedItem


logger = logging.getLogger("processor.classifier")


class Classifier:
    """内容分类器：自动分类 + 标签提取"""

    # 分类关键词映射
    CATEGORY_KEYWORDS = {
        "db": {
            "database", "storage", "query", "index", "transaction", "sql",
            "nosql", "lsm", "btree", "raft", "paxos", "sharding", "replication",
            "postgresql", "mysql", "redis", "mongodb", "cassandra", "cockroachdb",
            "olap", "oltp", "data warehouse", "data lake", "delta lake",
            "acid", "base", "cap theorem", "consistency", "partition",
            "buffer pool", "wal", "checkpoint", "compaction", "bloom filter",
        },
        "ai": {
            "llm", "large language model", "gpt", "transformer", "attention",
            "prompt", "chain-of-thought", "rag", "retrieval augmented",
            "agent", "multimodal", "vision language", "diffusion",
            "fine-tuning", "rlhf", "instruction tuning", "quantization",
            "gpt-4", "claude", "gemini", "llama", "mistral",
            "context window", "token", "embedding", "vector database",
        },
        "ml": {
            "machine learning", "deep learning", "neural network",
            "reinforcement learning", "supervised", "unsupervised",
            "classification", "regression", "clustering", "dimensionality reduction",
            "cnn", "rnn", "lstm", "gan", "autoencoder",
            "gradient descent", "backpropagation", "loss function",
            "feature engineering", "model deployment", "mlops",
        },
        "infra": {
            "distributed system", "consensus", "load balancing",
            "container", "kubernetes", "docker", "microservice",
            "observability", "monitoring", "tracing", "logging",
            "cloud native", "serverless", "edge computing",
            "message queue", "kafka", "rabbitmq", "grpc",
            "caching", "cdn", "dns", "api gateway",
        },
        "sys": {
            "operating system", "kernel", "file system", "memory management",
            "scheduling", "virtual memory", "page cache", "io_uring",
            "compiler", "jit", "interpreter", "assembly",
            "network protocol", "tcp/ip", "http", "quic",
            "hardware", "gpu", "tpu", "fpga", "rdma",
        },
    }

    def classify(self, item: AggregatedItem) -> str:
        """根据标题和内容判断分类"""
        text = f"{item.title} {item.raw_content}".lower()

        scores = {}
        for category, keywords in self.CATEGORY_KEYWORDS.items():
            score = sum(1 for kw in keywords if kw.lower() in text)
            if score > 0:
                scores[category] = score

        if not scores:
            return "other"

        # 返回得分最高的分类
        return max(scores, key=scores.get)

    def extract_tags(self, item: AggregatedItem) -> list[str]:
        """从标题和内容中提取标签"""
        text = f"{item.title} {item.raw_content}".lower()
        tags = set()

        # 提取已知的技术关键词
        all_keywords = set()
        for keywords in self.CATEGORY_KEYWORDS.values():
            all_keywords.update(keywords)

        for kw in all_keywords:
            if kw in text:
                # 标准化标签名
                tag = kw.replace(" ", "-").replace("_", "-")
                tags.add(tag)

        # 限制标签数量
        return sorted(list(tags))[:8]

    def process(self, item: AggregatedItem) -> tuple[str, list[str]]:
        """对单条内容执行分类和标签提取"""
        category = self.classify(item)
        tags = self.extract_tags(item)
        return category, tags