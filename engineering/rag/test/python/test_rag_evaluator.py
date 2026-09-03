"""
RAG 评估器单元测试
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from rag_evaluator import RetrievalMetrics, RAGEvaluator


class TestRetrievalMetrics:
    """RetrievalMetrics 单元测试"""

    def test_precision_at_k(self):
        """Precision@K"""
        retrieved = [1, 2, 3, 4, 5]
        relevant = [2, 4, 6]

        precision = RetrievalMetrics.precision_at_k(retrieved, relevant, k=5)
        assert precision == 2 / 5

    def test_recall_at_k(self):
        """Recall@K"""
        retrieved = [1, 2, 3, 4, 5]
        relevant = [2, 4, 6]

        recall = RetrievalMetrics.recall_at_k(retrieved, relevant, k=5)
        assert recall == 2 / 3

    def test_hit_rate_positive(self):
        """命中"""
        retrieved = [1, 2, 3]
        relevant = [2]
        assert RetrievalMetrics.hit_rate(retrieved, relevant) == 1.0

    def test_hit_rate_negative(self):
        """未命中"""
        retrieved = [1, 2, 3]
        relevant = [4, 5]
        assert RetrievalMetrics.hit_rate(retrieved, relevant) == 0.0

    def test_mrr(self):
        """MRR"""
        retrieved = [1, 2, 3]
        relevant = [2]
        mrr = RetrievalMetrics.mrr(retrieved, relevant)
        assert abs(mrr - 0.5) < 0.01

    def test_mrr_no_match(self):
        """MRR 无匹配"""
        retrieved = [1, 2]
        relevant = [3]
        assert RetrievalMetrics.mrr(retrieved, relevant) == 0.0

    def test_empty_retrieved(self):
        """空检索结果"""
        assert RetrievalMetrics.precision_at_k([], [1], k=5) == 0.0
        assert RetrievalMetrics.recall_at_k([], [1], k=5) == 0.0
        assert RetrievalMetrics.hit_rate([], [1]) == 0.0


class TestRAGEvaluator:
    """RAGEvaluator 单元测试"""

    def setup_method(self):
        self.evaluator = RAGEvaluator()

    def test_evaluate_retrieval(self):
        """检索评估"""
        result = self.evaluator.evaluate_retrieval(
            retrieved_ids=[1, 2, 3],
            relevant_ids=[2, 4],
            k_values=[1, 3],
        )
        assert "hit_rate" in result
        assert "precision@1" in result
        assert "precision@3" in result
        assert "recall@3" in result

    def test_evaluate_end_to_end(self):
        """端到端评估"""
        result = self.evaluator.evaluate_end_to_end(
            query="测试查询",
            generated_answer="这是一个测试回答。",
            reference_answer="这是参考回答。",
        )
        assert result["has_answer"] is True
        assert result["answer_length"] > 0
        assert "answer_similarity" in result

    def test_evaluate_with_retrieved(self):
        """带检索结果的评估"""
        result = self.evaluator.evaluate_end_to_end(
            query="q",
            generated_answer="a",
            retrieved_docs=["doc1", "doc2"],
        )
        assert result["retrieved_count"] == 2