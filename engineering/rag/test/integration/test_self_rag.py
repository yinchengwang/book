"""
@file test_self_rag.py
@brief Self-RAG / Corrective-RAG 集成测试
"""

import pytest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "rag" / "python"))


class TestSelfRAGIntegration:
    """Self-RAG 集成测试"""

    def setup_method(self):
        """测试配置"""
        from rag.self_rag import SelfRAGConfig
        self.config = SelfRAGConfig()
        self.config.enable_self_check = True
        self.config.max_retrieval_turns = 3
        self.config.relevance_threshold = 0.5
        self.config.support_threshold = 0.3
        self.config.usefulness_threshold = 0.5

    @pytest.mark.integration
    def test_reflection_token_workflow(self):
        """测试反思标记完整工作流"""
        try:
            from rag.self_rag import (
                ReflectionResult,
                parse_reflection_tokens,
                SelfRAGStage,
                create_self_rag_stage,
            )

            # 创建 Stage
            stage = create_self_rag_stage(self.config)

            # 模拟 LLM 输出
            test_cases = [
                # Case 1: 完全相关
                {
                    "input": """
[IS_RELEVANT] Yes, directly relevant.
[IS_SUPPORTED] Strongly supported by context.
[IS_USEFUL] Very useful information.
""",
                    "expected": {
                        "is_relevant": True,
                        "is_supported": True,
                        "is_useful": True,
                    }
                },
                # Case 2: 部分相关
                {
                    "input": """
[IS_RELEVANT] Somewhat relevant.
[IS_NOT_SUPPORTED] Not enough evidence.
""",
                    "expected": {
                        "is_relevant": True,
                        "is_supported": False,
                        "is_useful": False,
                    }
                },
                # Case 3: 不相关
                {
                    "input": """
[IS_NOT_RELEVANT] This is not related to the query.
""",
                    "expected": {
                        "is_relevant": False,
                        "is_supported": False,
                        "is_useful": False,
                    }
                },
            ]

            for case in test_cases:
                result = parse_reflection_tokens(case["input"])

                for key, expected_value in case["expected"].items():
                    assert getattr(result, key) == expected_value, \
                        f"Failed for {key}: got {getattr(result, key)}, expected {expected_value}"

        except ImportError:
            pytest.skip("Self-RAG not available")

    @pytest.mark.integration
    def test_corrective_action_workflow(self):
        """测试纠错动作工作流"""
        try:
            from rag.self_rag import (
                CorrectiveAction,
                CorrectiveRAG,
                ReflectionResult,
            )

            corrective = CorrectiveRAG()

            # Case 1: 高质量结果 -> PASS
            result_high = ReflectionResult(
                is_relevant=True,
                is_supported=True,
                is_useful=True,
                relevance_score=0.9,
                support_score=0.85,
                usefulness_score=0.8,
            )

            # Case 2: 中等质量 -> REWRITE
            result_mid = ReflectionResult(
                is_relevant=True,
                is_supported=False,
                is_useful=True,
                relevance_score=0.6,
                support_score=0.4,
                usefulness_score=0.55,
            )

            # Case 3: 低质量 -> REPEAT
            result_low = ReflectionResult(
                is_relevant=False,
                is_supported=False,
                is_useful=False,
                relevance_score=0.2,
                support_score=0.1,
                usefulness_score=0.15,
            )

            # 测试查询重写
            rewritten = corrective.rewrite_query(
                "什么是 RAG？",
                "RAG 是检索增强生成",
            )
            assert rewritten is not None
            assert len(rewritten) > 0

        except ImportError:
            pytest.skip("Self-RAG not available")


class TestMultiHopDecomposition:
    """多跳查询分解测试"""

    @pytest.mark.integration
    def test_two_hop_query(self):
        """测试两跳查询"""
        try:
            from rag.query_decomposer import (
                RuleBasedQueryDecomposer,
                DecompositionResult,
            )

            decomposer = RuleBasedQueryDecomposer()

            # 测试两跳查询
            result = decomposer.decompose(
                "张三的公司在哪个城市？"
            )

            assert result.success
            assert len(result.sub_queries) >= 1

            # 验证子查询结构
            for sq in result.sub_queries:
                assert sq.sub_query is not None
                assert len(sq.sub_query) > 0

        except ImportError:
            pytest.skip("Query decomposer not available")

    @pytest.mark.integration
    def test_comparative_query(self):
        """测试比较型查询"""
        try:
            from rag.query_decomposer import RuleBasedQueryDecomposer

            decomposer = RuleBasedQueryDecomposer()

            result = decomposer.decompose(
                "Python 和 Java 的区别是什么？"
            )

            assert result.success
            # 比较型查询应该分解为多个子查询
            assert len(result.sub_queries) >= 1

        except ImportError:
            pytest.skip("Query decomposer not available")


class TestGraphRAGIntegration:
    """Graph RAG 集成测试"""

    @pytest.mark.integration
    def test_community_detection(self):
        """测试社区检测"""
        try:
            from rag.community import (
                CommunityDetector,
                Community,
            )

            detector = CommunityDetector()
            assert detector is not None

        except ImportError:
            pytest.skip("Community not available")

    @pytest.mark.integration
    def test_community_summary(self):
        """测试社区摘要"""
        try:
            from rag.community import CommunitySummarizer

            summarizer = CommunitySummarizer()
            assert summarizer is not None

        except ImportError:
            pytest.skip("Community summarizer not available")


class TestMultimodalIntegration:
    """多模态集成测试"""

    @pytest.mark.integration
    def test_chart_analysis_flow(self):
        """测试图表分析流程"""
        try:
            from rag.python.understanding.chart_understanding import (
                ChartParser,
                ChartType,
                ChartAnalysis,
                create_chart_understanding_service,
            )

            # 解析 HTML 图表
            html = """
            <div class="chart">
                <title>Monthly Sales Report</title>
                <x-axis>Month</x-axis>
                <y-axis>Revenue ($)</y-axis>
                <data>[{"month": "Jan", "value": 5000}]</data>
            </div>
            """

            parser = ChartParser()
            structure = parser.parse(html)

            assert structure is not None
            assert structure.title == "Monthly Sales Report"

            # 创建分析服务
            service = create_chart_understanding_service()
            assert service is not None

        except ImportError:
            pytest.skip("Chart understanding not available")

    @pytest.mark.integration
    def test_video_segment_processing(self):
        """测试视频片段处理"""
        try:
            from rag.python.understanding.video_understanding import (
                VideoSegment,
                VideoIndex,
                SegmentType,
            )

            # 创建测试片段
            segment = VideoSegment(
                start_time=0.0,
                end_time=10.0,
                segment_type=SegmentType.VISUAL,
                transcript="Test transcript",
                confidence=0.95,
            )

            assert segment.start_time == 0.0
            assert segment.end_time == 10.0

        except ImportError:
            pytest.skip("Video understanding not available")


class TestMetricsIntegration:
    """Metrics 集成测试"""

    @pytest.mark.integration
    def test_latency_recording(self):
        """测试延迟记录"""
        try:
            from rag.tracing import (
                MetricsCollector,
                create_metrics_collector,
            )

            collector = create_metrics_collector()

            # 记录多个延迟值
            latencies = [10.0, 50.0, 100.0, 200.0, 500.0]
            for lat in latencies:
                collector.record_query_latency(lat)

            # 验证统计
            metrics = collector.metrics()
            assert metrics.query_latency_ms.count() == 5

        except ImportError:
            pytest.skip("Metrics not available")

    @pytest.mark.integration
    def test_prometheus_export(self):
        """测试 Prometheus 导出"""
        try:
            from rag.tracing import (
                MetricsCollector,
                create_metrics_collector,
            )

            collector = create_metrics_collector()

            # 记录数据
            collector.record_query_latency(100.0)
            collector.increment_queries()
            collector.increment_cache_hit()
            collector.increment_modality("text")
            collector.increment_modality("image")

            # 导出
            output = collector.export_prometheus()

            assert "rag_query_latency_ms" in output
            assert "rag_queries_total" in output
            assert "rag_cache_hits_total" in output
            assert 'rag_modality_total{modality="text"}' in output
            assert 'rag_modality_total{modality="image"}' in output

        except ImportError:
            pytest.skip("Metrics not available")

    @pytest.mark.integration
    def test_json_export(self):
        """测试 JSON 导出"""
        try:
            from rag.tracing import (
                MetricsCollector,
                create_metrics_collector,
            )

            collector = create_metrics_collector()

            # 记录数据
            collector.record_query_latency(150.0)
            collector.increment_queries()

            # 导出
            output = collector.export_json()

            import json
            data = json.loads(output)

            assert "latency" in data
            assert "throughput" in data
            assert "cache" in data
            assert "modality" in data
            assert "errors" in data

        except ImportError:
            pytest.skip("Metrics not available")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-m", "integration"])
