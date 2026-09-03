"""
RAG 评估模块

提供检索质量和生成质量的评估指标。
"""

import logging
from typing import Optional

logger = logging.getLogger(__name__)


class RetrievalMetrics:
    """检索评估指标"""

    @staticmethod
    def precision_at_k(retrieved: list, relevant: list, k: int = 10) -> float:
        """
        Precision@K

        Args:
            retrieved: 检索结果 ID 列表
            relevant: 相关文档 ID 列表
            k: 取前 K 个结果

        Returns:
            Precision@K
        """
        if not retrieved or k <= 0:
            return 0.0

        top_k = retrieved[:k]
        hits = sum(1 for r in top_k if r in relevant)
        return hits / k

    @staticmethod
    def recall_at_k(retrieved: list, relevant: list, k: int = 10) -> float:
        """
        Recall@K

        Args:
            retrieved: 检索结果 ID 列表
            relevant: 相关文档 ID 列表
            k: 取前 K 个结果

        Returns:
            Recall@K
        """
        if not relevant:
            return 1.0 if not retrieved else 0.0

        top_k = retrieved[:k]
        hits = sum(1 for r in top_k if r in relevant)
        return hits / len(relevant)

    @staticmethod
    def hit_rate(retrieved: list, relevant: list) -> float:
        """
        Hit Rate (命中率)

        Args:
            retrieved: 检索结果 ID 列表
            relevant: 相关文档 ID 列表

        Returns:
            Hit Rate (0.0 - 1.0)
        """
        if not retrieved:
            return 0.0
        return 1.0 if any(r in relevant for r in retrieved) else 0.0

    @staticmethod
    def mrr(retrieved: list, relevant: list) -> float:
        """
        Mean Reciprocal Rank (MRR)

        Args:
            retrieved: 检索结果 ID 列表
            relevant: 相关文档 ID 列表

        Returns:
            MRR (0.0 - 1.0)
        """
        for i, r in enumerate(retrieved, 1):
            if r in relevant:
                return 1.0 / i
        return 0.0


class RAGEvaluator:
    """
    RAG 系统评估器

    支持检索质量和端到端质量的评估。
    """

    def __init__(self):
        self.retrieval_metrics = RetrievalMetrics()

    def evaluate_retrieval(
        self,
        retrieved_ids: list,
        relevant_ids: list,
        k_values: list = None,
    ) -> dict:
        """
        评估检索质量

        Args:
            retrieved_ids: 检索结果 ID 列表
            relevant_ids: 相关文档 ID 列表
            k_values: 评估的 K 值列表

        Returns:
            评估指标字典
        """
        if k_values is None:
            k_values = [1, 5, 10, 20]

        results = {
            "hit_rate": self.retrieval_metrics.hit_rate(retrieved_ids, relevant_ids),
            "mrr": self.retrieval_metrics.mrr(retrieved_ids, relevant_ids),
        }

        for k in k_values:
            results[f"precision@{k}"] = self.retrieval_metrics.precision_at_k(
                retrieved_ids, relevant_ids, k
            )
            results[f"recall@{k}"] = self.retrieval_metrics.recall_at_k(
                retrieved_ids, relevant_ids, k
            )

        logger.info(f"检索评估: hit_rate={results['hit_rate']:.3f}, mrr={results['mrr']:.3f}")
        return results

    def evaluate_end_to_end(
        self,
        query: str,
        generated_answer: str,
        reference_answer: str = None,
        retrieved_docs: list = None,
    ) -> dict:
        """
        端到端评估

        Args:
            query: 用户查询
            generated_answer: 生成的回答
            reference_answer: 参考回答（用于人工评估）
            retrieved_docs: 检索到的文档列表

        Returns:
            评估指标字典
        """
        results = {
            "query": query,
            "answer_length": len(generated_answer),
            "has_answer": len(generated_answer) > 10,
        }

        if retrieved_docs:
            results["retrieved_count"] = len(retrieved_docs)

        # 如果有参考回答，可以计算简单的相似度
        if reference_answer:
            from difflib import SequenceMatcher
            similarity = SequenceMatcher(
                None, generated_answer, reference_answer
            ).ratio()
            results["answer_similarity"] = similarity

        return results