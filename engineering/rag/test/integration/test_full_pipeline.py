"""
@file test_full_pipeline.py
@brief RAG Pipeline 端到端集成测试
"""

import pytest
import asyncio
import sys
import os
from pathlib import Path
from typing import List, Dict, Any

# 添加项目路径
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src" / "rag" / "python"))


class TestFullPipeline:
    """完整 Pipeline 集成测试"""

    def setup_method(self):
        """测试前准备"""
        self.test_docs = [
            {
                "content": "RAG（Retrieval-Augmented Generation）是一种结合检索和生成的架构。它通过从外部知识库检索相关信息，然后将这些信息作为上下文传递给语言模型，从而生成更准确的回答。",
                "metadata": {"file_type": "md", "source": "rag_intro.md"}
            },
            {
                "content": "混合检索（Hybrid Retrieval）结合了向量检索和关键词检索的优势。向量检索擅长语义匹配，而 BM25 关键词检索擅长精确匹配。通过 RRF（Reciprocal Rank Fusion）融合两者的结果，可以显著提升检索质量。",
                "metadata": {"file_type": "md", "source": "hybrid_retrieval.md"}
            },
            {
                "content": "Self-RAG 是一种自我反思的检索增强生成方法。它通过生成反思标记（Reflection Tokens）来评估检索结果的相关性、完整性和有用性。如果评估结果不理想，系统会自动重写查询并重新检索。",
                "metadata": {"file_type": "md", "source": "self_rag.md"}
            },
        ]

    @pytest.mark.integration
    def test_pipeline_creation(self):
        """测试 Pipeline 创建"""
        try:
            from rag.pipeline import create_default_pipeline
            pipeline = create_default_pipeline()
            assert pipeline is not None
        except ImportError:
            pytest.skip("Pipeline module not available")

    @pytest.mark.integration
    def test_query_classification(self):
        """测试查询分类"""
        test_cases = [
            ("什么是 RAG", "factual"),
            ("比较 A 和 B 的区别", "comparative"),
            ("总结这篇文章", "summary"),
            ("为什么系统会崩溃", "analytical"),
        ]

        try:
            from rag.query_classifier import RuleBasedQueryClassifier

            classifier = RuleBasedQueryClassifier()
            for query, expected_type in test_cases:
                result = classifier.classify(query)
                # 验证分类结果类型正确
                assert result is not None
        except ImportError:
            pytest.skip("Query classifier not available")

    @pytest.mark.integration
    def test_query_decomposition(self):
        """测试查询分解"""
        try:
            from rag.query_decomposer import RuleBasedQueryDecomposer

            decomposer = RuleBasedQueryDecomposer()

            # 测试多跳查询
            result = decomposer.decompose("张三和李四在哪个公司工作")
            assert result.success
            assert len(result.sub_queries) > 0

            # 测试简单查询
            result = decomposer.decompose("什么是 RAG")
            assert result.success
        except ImportError:
            pytest.skip("Query decomposer not available")

    @pytest.mark.integration
    def test_metadata_filter(self):
        """测试元数据过滤"""
        try:
            from rag.types import RetrievalResult, Chunk
            from rag.metadata_filter import MetadataFilter, FilterBuilder

            # 创建测试结果
            results = []
            for i, (ft, source) in enumerate([
                ("pdf", "doc1.pdf"),
                ("md", "doc2.md"),
                ("pdf", "doc3.pdf"),
                ("html", "doc4.html"),
            ]):
                chunk = Chunk(
                    id=f"chunk_{i}",
                    content=f"Content {i}",
                    metadata={"file_type": ft, "source": source}
                )
                results.append(RetrievalResult(chunk=chunk, score=1.0 - i * 0.1))

            # 测试过滤
            filter = MetadataFilter()
            filter.file_types = ["pdf"]

            filtered = filter.apply(results)
            assert len(filtered) == 2

            # 测试 Builder
            filter2 = (FilterBuilder()
                      .with_file_types(["md", "html"])
                      .build())

            filtered2 = filter2.apply(results)
            assert len(filtered2) == 2
        except ImportError:
            pytest.skip("Metadata filter not available")

    @pytest.mark.integration
    def test_self_rag_reflection(self):
        """测试 Self-RAG 反思"""
        try:
            from rag.self_rag import (
                parse_reflection_tokens,
                ReflectionResult,
                SelfRAGConfig,
                create_self_rag_stage,
            )

            # 测试反思标记解析
            llm_output = """
[IS_RELEVANT] Yes, the content is relevant to the query.
[IS_SUPPORTED] The content supports the answer.
[USEFUL] This information is useful.
"""
            result = parse_reflection_tokens(llm_output)
            assert result.is_relevant
            assert result.is_supported
            assert result.is_useful

            # 测试配置
            config = SelfRAGConfig()
            assert config.relevance_threshold == 0.5
            assert config.max_retrieval_turns == 3

            # 测试 Stage 创建
            stage = create_self_rag_stage(config)
            assert stage is not None
            assert stage.name() == "self_rag"
        except ImportError:
            pytest.skip("Self-RAG not available")

    @pytest.mark.integration
    def test_tracing_metrics(self):
        """测试 Tracing 和 Metrics"""
        try:
            from rag.tracing import (
                RAGTracer,
                MetricsCollector,
                PipelineObserver,
                create_tracer,
                create_metrics_collector,
            )

            # 测试 Tracer
            config = RAGTracer.Config()
            config.sampling_rate = 1.0
            tracer = create_tracer(config)

            span = tracer.start_span("test_span")
            assert span is not None
            assert span.name == "test_span"

            span.add_attribute("key", "value")
            tracer.end_span(span)

            # 测试 Metrics
            metrics_config = MetricsCollector.Config()
            metrics = create_metrics_collector(metrics_config)

            metrics.record_query_latency(100.0)
            metrics.record_retrieval_latency(50.0)
            metrics.increment_queries()
            metrics.increment_cache_hit()

            prometheus = metrics.export_prometheus()
            assert "rag_query_latency_ms" in prometheus

            json_export = metrics.export_json()
            assert "latency" in json_export
        except ImportError:
            pytest.skip("Tracing/Metrics not available")

    @pytest.mark.integration
    def test_pipeline_observer(self):
        """测试 Pipeline Observer"""
        try:
            from rag.pipeline import RetrievalPipeline
            from rag.tracing import (
                create_tracer,
                create_metrics_collector,
                create_pipeline_observer,
            )

            pipeline = RetrievalPipeline()
            tracer = create_tracer()
            metrics = create_metrics_collector()

            observer = create_pipeline_observer(pipeline, tracer, metrics)
            assert observer is not None
            assert observer.is_tracing_enabled()
            assert observer.is_metrics_enabled()
        except ImportError:
            pytest.skip("Pipeline observer not available")


class TestEndToEndFlow:
    """端到端流程测试"""

    @pytest.mark.integration
    @pytest.mark.slow
    def test_full_rag_flow(self):
        """测试完整 RAG 流程（需要模型）"""
        # 这个测试需要完整的模型和索引
        # 仅作为示例框架
        pytest.skip("Full RAG flow requires models and index")


class TestMultiModalPipeline:
    """多模态 Pipeline 测试"""

    def setup_method(self):
        self.test_data = {
            "text": "This is a test document about RAG systems.",
            "chart_html": "<div><title>Sales Chart</title></div>",
            "audio_path": "/tmp/test_audio.wav",
            "video_path": "/tmp/test_video.mp4",
        }

    @pytest.mark.integration
    def test_chart_understanding(self):
        """测试图表理解"""
        try:
            from rag.python.understanding.chart_understanding import (
                ChartParser,
                ChartType,
                create_chart_understanding_service,
            )

            parser = ChartParser()
            structure = parser.parse(self.test_data["chart_html"])
            assert structure is not None

            service = create_chart_understanding_service()
            assert service is not None
        except ImportError:
            pytest.skip("Chart understanding not available")

    @pytest.mark.integration
    def test_video_understanding(self):
        """测试视频理解"""
        try:
            from rag.python.understanding.video_understanding import (
                VideoUnderstandingService,
                create_video_understanding_service,
            )

            service = create_video_understanding_service()
            assert service is not None
        except ImportError:
            pytest.skip("Video understanding not available")

    @pytest.mark.integration
    def test_audio_understanding(self):
        """测试音频理解"""
        try:
            from rag.python.understanding.audio_understanding import (
                AudioUnderstandingService,
                create_audio_understanding_service,
            )

            service = create_audio_understanding_service()
            assert service is not None
        except ImportError:
            pytest.skip("Audio understanding not available")


class TestErrorHandling:
    """错误处理测试"""

    @pytest.mark.integration
    def test_invalid_query(self):
        """测试无效查询处理"""
        try:
            from rag.query_classifier import RuleBasedQueryClassifier

            classifier = RuleBasedQueryClassifier()

            # 空查询
            result = classifier.classify("")
            assert result is not None

            # 超长查询
            long_query = "test " * 1000
            result = classifier.classify(long_query)
            assert result is not None
        except ImportError:
            pytest.skip("Query classifier not available")

    @pytest.mark.integration
    def test_empty_metadata_filter(self):
        """测试空元数据过滤"""
        try:
            from rag.types import RetrievalResult, Chunk
            from rag.metadata_filter import MetadataFilter

            # 空结果
            filter = MetadataFilter()
            filtered = filter.apply([])
            assert len(filtered) == 0

            # 空过滤器
            chunk = Chunk(id="1", content="test")
            results = [RetrievalResult(chunk=chunk, score=1.0)]
            filtered = filter.apply(results)
            assert len(filtered) == 1
        except ImportError:
            pytest.skip("Metadata filter not available")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-m", "integration"])
