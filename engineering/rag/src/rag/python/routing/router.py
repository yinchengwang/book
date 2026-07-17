"""
分层路由模块

路由决策树：规则引擎 → LLM 意图分类 → 向量检索 + RRF 融合
"""

import logging
import re
from typing import Optional

logger = logging.getLogger(__name__)


class RoutingDecision:
    """路由决策结果"""
    def __init__(
        self,
        strategy: str = "semantic",
        modalities: list = None,
        top_k: int = 10,
        threshold: float = 0.0,
        sql_query: Optional[str] = None,
        direct_answer: Optional[str] = None,
    ):
        self.strategy = strategy    # direct/sql/bm25/semantic/multimodal
        self.modalities = modalities or ["text", "table", "image", "code"]
        self.top_k = top_k
        self.threshold = threshold
        self.sql_query = sql_query
        self.direct_answer = direct_answer


class RuleEngine:
    """
    规则引擎

    通过关键词模式快速匹配简单查询，无需 LLM 参与。
    """

    # 规则模式: (pattern, strategy, description)
    RULES = [
        # R0: 直接回答（时间、日期等）
        (r'^(周|星期|几号|今天|明天|后天|现在|时间|日期)', 'direct', '直接回答'),
        (r'^(你好|嗨|hello|hi)\b', 'direct', '问候'),
        (r'^(谢谢|感谢|再见|bye)\b', 'direct', '礼貌用语'),

        # R1: SQL 查询（统计、汇总等）
        (r'^(多少|统计|总计|求和|平均|最大值|最小值|计数)', 'sql', 'SQL 查询'),
        (r'^(前|top|排名|排序|最高|最低)', 'sql', 'SQL 排序查询'),

        # R2: BM25 关键词检索
        (r'(长度|字符|关键词|包含|匹配|查找|搜索).*(文档|文件|内容)', 'bm25', '关键词检索'),

        # R5: 图像检索
        (r'(图|图片|图像|截图|架构图|流程图|照片)', 'image', '图像检索'),

        # R4: 表格检索
        (r'(表|表格|数据表|对比|对比表|销售额|利润|收入|成本)', 'table', '表格检索'),

        # R6: 代码检索
        (r'(代码|函数|类|方法|接口|API|实现)', 'code', '代码检索'),
    ]

    def __init__(self):
        self._compiled = [
            (re.compile(p), strategy, desc)
            for p, strategy, desc in self.RULES
        ]

    def match(self, query: str) -> Optional[RoutingDecision]:
        """
        匹配规则

        Args:
            query: 用户查询

        Returns:
            匹配到的 RoutingDecision，无匹配返回 None
        """
        for pattern, strategy, desc in self._compiled:
            match = pattern.search(query)
            if match:
                logger.info(f"规则引擎命中: {desc} (strategy={strategy})")

                if strategy == 'direct':
                    return RoutingDecision(strategy='direct')
                elif strategy == 'sql':
                    return RoutingDecision(strategy='sql')
                elif strategy == 'bm25':
                    return RoutingDecision(
                        strategy='bm25',
                        top_k=10,
                    )
                elif strategy == 'image':
                    return RoutingDecision(
                        strategy='semantic',
                        modalities=['image'],
                        top_k=10,
                        threshold=0.7,
                    )
                elif strategy == 'table':
                    return RoutingDecision(
                        strategy='semantic',
                        modalities=['table'],
                        top_k=10,
                        threshold=0.75,
                    )
                elif strategy == 'code':
                    return RoutingDecision(
                        strategy='semantic',
                        modalities=['code'],
                        top_k=10,
                        threshold=0.7,
                    )

        return None


class LLMIntentClassifier:
    """
    LLM 意图分类器

    使用 LLM 判断复杂查询的意图和所需模态。
    """

    CLASSIFICATION_PROMPT = """判断用户 query 需要检索哪种模态:
- text: 纯文本检索
- table: 表格数据检索
- image: 图像检索
- code: 代码检索
- multimodal: 多模态联合检索

query: {query}

请返回 JSON 格式: {{"modality": "text|table|image|code|multimodal"}}
"""

    def __init__(self, llm_client=None):
        self.llm_client = llm_client

    def classify(self, query: str) -> RoutingDecision:
        """
        使用 LLM 分类查询意图

        Args:
            query: 用户查询

        Returns:
            路由决策
        """
        if self.llm_client:
            try:
                prompt = self.CLASSIFICATION_PROMPT.format(query=query)
                response = self.llm_client.chat(prompt)
                import json
                result = json.loads(response)
                modality = result.get("modality", "multimodal")

                modality_map = {
                    "text": ["text"],
                    "table": ["table"],
                    "image": ["image"],
                    "code": ["code"],
                    "multimodal": ["text", "table", "image", "code"],
                }

                return RoutingDecision(
                    strategy='semantic',
                    modalities=modality_map.get(modality, ["text", "table", "image", "code"]),
                    top_k=10,
                )
            except Exception as e:
                logger.warning(f"LLM 分类失败，回退到多模态检索: {e}")

        # LLM 不可用，默认多模态检索
        return RoutingDecision(
            strategy='multimodal',
            modalities=["text", "table", "image", "code"],
            top_k=10,
        )


class Router:
    """
    分层路由入口

    决策流程:
    1. 规则引擎快速匹配（R0-R2）
    2. 未命中 → LLM 意图分类（R3-R7）
    3. 执行对应策略
    """

    def __init__(self, rule_engine: RuleEngine = None, llm_classifier: LLMIntentClassifier = None):
        self.rule_engine = rule_engine or RuleEngine()
        self.llm_classifier = llm_classifier or LLMIntentClassifier()

    def route(self, query: str) -> RoutingDecision:
        """
        路由决策

        Args:
            query: 用户查询

        Returns:
            路由决策
        """
        logger.info(f"路由查询: {query}")

        # 1. 规则引擎快速匹配
        decision = self.rule_engine.match(query)
        if decision:
            logger.info(f"规则引擎决策: {decision.strategy}")
            return decision

        # 2. LLM 意图分类
        decision = self.llm_classifier.classify(query)
        logger.info(f"LLM 意图分类决策: {decision.strategy}, modalities={decision.modalities}")
        return decision