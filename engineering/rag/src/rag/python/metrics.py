"""
Prometheus 指标定义和埋点模块
"""

import logging
import time
from typing import Optional

logger = logging.getLogger(__name__)


# ==================== 指标定义 ====================

# 尝试导入 prometheus_client，失败则使用模拟
try:
    from prometheus_client import Counter, Histogram, Gauge

    # 检索质量指标
    retrieval_precision = Histogram(
        'rag_retrieval_precision',
        '检索结果与 query 的相关性评分',
        ['modality', 'tenant_id'],
        buckets=[0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0],
    )

    retrieval_recall = Histogram(
        'rag_retrieval_recall',
        '检索召回率',
        ['modality', 'tenant_id'],
    )

    hit_rate = Counter(
        'rag_hit_total',
        '命中计数',
        ['modality', 'tenant_id'],
    )

    miss_rate = Counter(
        'rag_miss_total',
        '未命中计数',
        ['modality', 'tenant_id'],
    )

    # 性能指标
    query_latency = Histogram(
        'rag_query_latency_seconds',
        '查询延迟分布',
        ['stage', 'tenant_id'],
        buckets=[0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0],
    )

    indexing_latency = Histogram(
        'rag_indexing_latency_seconds',
        '文档入库延迟分布',
        ['modality', 'tenant_id'],
        buckets=[1.0, 5.0, 10.0, 30.0, 60.0, 120.0, 300.0],
    )

    # 系统指标
    active_tenants = Gauge(
        'rag_active_tenants',
        '活跃租户数',
    )

    queue_depth = Gauge(
        'rag_ingestion_queue_depth',
        '入库任务队列深度',
        ['queue_name'],
    )

    embedding_service_health = Gauge(
        'rag_embedding_service_health',
        'Embedding 服务健康状态',
        ['service_name'],
    )

    llm_provider_health = Gauge(
        'rag_llm_provider_health',
        'LLM Provider 健康状态',
        ['provider'],
    )

    # 用户反馈指标
    user_feedback = Counter(
        'rag_user_feedback_total',
        '用户反馈计数',
        ['type', 'tenant_id'],
    )

    answer_quality_score = Histogram(
        'rag_answer_quality_score',
        '用户评分（1-5分）',
        ['tenant_id'],
        buckets=[1, 2, 3, 4, 5],
    )

    PROMETHEUS_AVAILABLE = True

except ImportError:
    logger.warning("prometheus_client 未安装，使用模拟指标")
    PROMETHEUS_AVAILABLE = False

    # 模拟指标类
    class _MockMetric:
        def labels(self, **kwargs):
            return self
        def observe(self, value):
            pass
        def inc(self, value=1):
            pass
        def set(self, value):
            pass

    retrieval_precision = _MockMetric()
    retrieval_recall = _MockMetric()
    hit_rate = _MockMetric()
    miss_rate = _MockMetric()
    query_latency = _MockMetric()
    indexing_latency = _MockMetric()
    active_tenants = _MockMetric()
    queue_depth = _MockMetric()
    embedding_service_health = _MockMetric()
    llm_provider_health = _MockMetric()
    user_feedback = _MockMetric()
    answer_quality_score = _MockMetric()


# ==================== 埋点装饰器 ====================

class InstrumentedRetriever:
    """检索层埋点"""

    def __init__(self, retriever, tenant_id: str = "system"):
        self.retriever = retriever
        self.tenant_id = tenant_id

    async def retrieve(self, *args, **kwargs):
        """埋点后的检索"""
        start = time.time()

        try:
            results = await self.retriever.retrieve(*args, **kwargs)

            latency = time.time() - start
            query_latency.labels(stage='retrieval', tenant_id=self.tenant_id).observe(latency)

            # 记录检索质量
            for r in results:
                modality = getattr(r, "chunk_type", "text")
                retrieval_precision.labels(
                    modality=modality,
                    tenant_id=self.tenant_id,
                ).observe(getattr(r, "score", 0))

            logger.info(f"检索埋点: {len(results)} 结果, 延迟 {latency:.3f}s")
            return results

        except Exception as e:
            latency = time.time() - start
            query_latency.labels(stage='retrieval', tenant_id=self.tenant_id).observe(latency)
            logger.error(f"检索失败: {e}")
            raise


class InstrumentedLLMProvider:
    """生成层埋点"""

    def __init__(self, provider, tenant_id: str = "system"):
        self.provider = provider
        self.tenant_id = tenant_id

    def generate(self, *args, **kwargs):
        """埋点后的生成"""
        start = time.time()
        success = True

        try:
            response = self.provider.generate(*args, **kwargs)
            return response
        except Exception as e:
            success = False
            raise
        finally:
            latency = time.time() - start
            query_latency.labels(stage='generation', tenant_id=self.tenant_id).observe(latency)
            if not success:
                logger.error(f"生成层埋点: 失败, 延迟 {latency:.3f}s")
            else:
                logger.info(f"生成层埋点: 成功, 延迟 {latency:.3f}s")