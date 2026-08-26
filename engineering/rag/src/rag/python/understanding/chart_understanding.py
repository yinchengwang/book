"""
@file chart_understanding.py
@brief 图表理解服务 - 解析和理解图表内容

支持:
- 解析图表结构 (标题、轴标签、数据趋势)
- VQA 提取详细信息
- 生成检索用 QA 对
"""

import asyncio
import re
from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional, Union, Tuple
from enum import Enum
import json

try:
    from PIL import Image
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False


class ChartType(Enum):
    """图表类型"""
    LINE = "line"
    BAR = "bar"
    PIE = "pie"
    SCATTER = "scatter"
    AREA = "area"
    TABLE = "table"
    MIXED = "mixed"
    UNKNOWN = "unknown"


@dataclass
class ChartStructure:
    """图表结构"""
    chart_type: ChartType = ChartType.UNKNOWN
    title: str = ""
    x_axis_label: str = ""
    y_axis_label: str = ""
    legend: List[str] = field(default_factory=list)

    # 数据点
    data_points: List[Dict[str, Any]] = field(default_factory=list)

    # 趋势分析
    trend: str = ""  # "increasing", "decreasing", "stable", "fluctuating"
    max_value: float = 0.0
    min_value: float = 0.0
    max_point: Tuple[str, float] = ("", 0.0)
    min_point: Tuple[str, float] = ("", 0.0)

    # 异常值
    outliers: List[Tuple[str, float]] = field(default_factory=list)

    # 元数据
    source: str = ""
    time_range: str = ""


@dataclass
class QAPair:
    """问答对"""
    question: str
    answer: str
    confidence: float = 1.0
    topic: str = ""


@dataclass
class ChartAnalysis:
    """图表分析结果"""
    structure: ChartStructure
    description: str
    qa_pairs: List[QAPair] = field(default_factory=list)
    vqa_answers: Dict[str, str] = field(default_factory=dict)
    raw_response: str = ""

    def to_dict(self) -> Dict[str, Any]:
        return {
            "structure": {
                "chart_type": self.structure.chart_type.value,
                "title": self.structure.title,
                "x_axis": self.structure.x_axis_label,
                "y_axis": self.structure.y_axis_label,
                "legend": self.structure.legend,
                "trend": self.structure.trend,
                "max_value": self.structure.max_value,
                "min_value": self.structure.min_value,
            },
            "description": self.description,
            "qa_pairs": [
                {"question": qa.question, "answer": qa.answer, "confidence": qa.confidence}
                for qa in self.qa_pairs
            ]
        }


class ChartParser:
    """图表解析器"""

    def __init__(self):
        self.chart_patterns = {
            ChartType.LINE: [r"折线图", r"趋势图", r"line.*chart"],
            ChartType.BAR: [r"柱状图", r"条形图", r"bar.*chart"],
            ChartType.PIE: [r"饼图", r"比例图", r"pie.*chart"],
            ChartType.SCATTER: [r"散点图", r"scatter.*plot"],
            ChartType.AREA: [r"面积图", r"area.*chart"],
        }

    def parse(self, source: Union[str, bytes]) -> ChartStructure:
        """
        解析图表

        @param source: 图表源 (可以是 HTML、SVG、文件路径或图片字节)
        @return 图表结构
        """
        if isinstance(source, bytes):
            return self._parse_image(source)
        elif source.endswith(('.png', '.jpg', '.jpeg', '.svg')):
            return self._parse_file(source)
        else:
            return self._parse_markup(source)

    def _parse_file(self, file_path: str) -> ChartStructure:
        """解析文件"""
        structure = ChartStructure()

        if file_path.endswith('.svg'):
            return self._parse_svg(file_path)
        elif PIL_AVAILABLE:
            # 图片文件需要 OCR 或 VLM
            return structure

        return structure

    def _parse_markup(self, markup: str) -> ChartStructure:
        """解析 HTML/SVG 标记"""
        structure = ChartStructure()

        # 检测图表类型
        for chart_type, patterns in self.chart_patterns.items():
            for pattern in patterns:
                if re.search(pattern, markup, re.IGNORECASE):
                    structure.chart_type = chart_type
                    break

        # 提取标题
        title_patterns = [
            r'<title>([^<]+)</title>',
            r'class="title">([^<]+)',
            r'data-title="([^"]+)"',
        ]
        for pattern in title_patterns:
            match = re.search(pattern, markup, re.IGNORECASE)
            if match:
                structure.title = match.group(1).strip()
                break

        # 提取轴标签
        x_patterns = [r'x-axis[^>]*>([^<]+)', r'xLabel[^:]*:\s*["\']([^"\']+)']
        for pattern in x_patterns:
            match = re.search(pattern, markup, re.IGNORECASE)
            if match:
                structure.x_axis_label = match.group(1).strip()
                break

        y_patterns = [r'y-axis[^>]*>([^<]+)', r'yLabel[^:]*:\s*["\']([^"\']+)']
        for pattern in y_patterns:
            match = re.search(pattern, markup, re.IGNORECASE)
            if match:
                structure.y_axis_label = match.group(1).strip()
                break

        # 提取数据点 (JSON 格式)
        data_patterns = [
            r'data\s*=\s*(\[[^\]]+\])',
            r'data\s*:\s*(\[[^\]]+\])',
        ]
        for pattern in data_patterns:
            match = re.search(pattern, markup)
            if match:
                try:
                    structure.data_points = json.loads(match.group(1))
                except json.JSONDecodeError:
                    pass
                break

        # 提取图例
        legend_pattern = r'legend\s*[:=]\s*\[([^\]]+)\]'
        match = re.search(legend_pattern, markup, re.IGNORECASE)
        if match:
            structure.legend = [l.strip().strip('"\'') for l in match.group(1).split(',')]

        return structure

    def _parse_svg(self, svg_path: str) -> ChartStructure:
        """解析 SVG 文件"""
        with open(svg_path, 'r', encoding='utf-8') as f:
            svg_content = f.read()
        return self._parse_markup(svg_content)

    def _parse_image(self, image_bytes: bytes) -> ChartStructure:
        """解析图片 - 需要 OCR/VLM"""
        structure = ChartStructure()
        # TODO: 集成 OCR 或 VLM
        return structure


class ChartUnderstandingService:
    """
    图表理解服务

    输入: 图表图片/HTML/SVG
    输出: 结构化描述 + QA对
    """

    def __init__(
        self,
        vqa_model: str = "llava-v1.6-34b",
        vqa_endpoint: Optional[str] = None,
        use_local_vlm: bool = False
    ):
        """
        @param vqa_model: VQA 模型名称
        @param vqa_endpoint: VQA API 端点 (用于云端)
        @param use_local_vlm: 是否使用本地 VLM
        """
        self.vqa_model = vqa_model
        self.vqa_endpoint = vqa_endpoint
        self.use_local_vlm = use_local_vlm
        self.chart_parser = ChartParser()

        # VQA 问题模板
        self.vqa_questions = [
            "What is the main title of this chart?",
            "What are the x-axis and y-axis labels?",
            "What is the overall trend shown in the data?",
            "What is the maximum value and when does it occur?",
            "What is the minimum value and when does it occur?",
            "Are there any notable patterns or outliers?",
            "What is the time period covered by this chart?",
        ]

        # 中文问题
        self.vqa_questions_cn = [
            "这个图表的标题是什么？",
            "X轴和Y轴的标签是什么？",
            "数据展示的总体趋势是什么？",
            "最大值是多少，什么时候出现的？",
            "最小值是多少，什么时候出现的？",
            "有什么显著的规律或异常值？",
            "这个图表覆盖的时间范围是什么？",
        ]

    async def understand(
        self,
        chart_source: Union[str, bytes],
        language: str = "auto"
    ) -> ChartAnalysis:
        """
        理解图表

        @param chart_source: 图表源 (图片路径/URL/HTML/SVG/字节)
        @param language: 语言 "en" | "zh" | "auto"
        @return 图表分析结果
        """
        # 1. 解析图表结构
        structure = self.chart_parser.parse(chart_source)

        # 2. 选择问题集
        questions = self.vqa_questions_cn if language == "zh" else self.vqa_questions

        # 3. VQA 提取详细信息
        vqa_answers = await self._vqa_extract(chart_source, questions)

        # 4. 合并结构信息
        self._merge_structure(structure, vqa_answers)

        # 5. 生成描述
        description = self._generate_description(structure, vqa_answers)

        # 6. 生成 QA 对
        qa_pairs = self._generate_qa_pairs(structure, vqa_answers)

        return ChartAnalysis(
            structure=structure,
            description=description,
            qa_pairs=qa_pairs,
            vqa_answers=vqa_answers,
        )

    async def _vqa_extract(
        self,
        chart_source: Union[str, bytes],
        questions: List[str]
    ) -> Dict[str, str]:
        """使用 VQA 模型提取信息"""
        answers = {}

        if self.use_local_vlm:
            answers = await self._local_vqa(chart_source, questions)
        elif self.vqa_endpoint:
            answers = await self._api_vqa(chart_source, questions)
        else:
            # 无 VQA，使用规则解析
            answers = self._rule_based_extract(chart_source, questions)

        return answers

    async def _local_vqa(
        self,
        chart_source: Union[str, bytes],
        questions: List[str]
    ) -> Dict[str, str]:
        """本地 VLM 推理"""
        # TODO: 集成本地 VLM (如 LLaVA, Qwen-VL)
        raise NotImplementedError("Local VLM not yet implemented")

    async def _api_vqa(
        self,
        chart_source: Union[str, bytes],
        questions: List[str]
    ) -> Dict[str, str]:
        """API VQA 调用"""
        # TODO: 集成 API (如 GPT-4V, Claude Vision)
        raise NotImplementedError("API VQA not yet implemented")

    def _rule_based_extract(
        self,
        chart_source: Union[str, bytes],
        questions: List[str]
    ) -> Dict[str, str]:
        """基于规则的提取"""
        answers = {}

        if isinstance(chart_source, str):
            # 从文本中提取
            for q in questions:
                if "标题" in q or "title" in q.lower():
                    # 查找标题
                    match = re.search(r'<title>([^<]+)</title>', chart_source)
                    if match:
                        answers["title"] = match.group(1)
                    else:
                        answers["title"] = "Unknown"

                elif "轴" in q or "axis" in q.lower():
                    answers["axis"] = "X/Y axis detected in chart"

                elif "趋势" in q or "trend" in q.lower():
                    answers["trend"] = "Trend analysis pending VQA"

        return answers

    def _merge_structure(self, structure: ChartStructure, vqa_answers: Dict[str, str]):
        """合并 VQA 答案到结构"""
        if "title" in vqa_answers and not structure.title:
            structure.title = vqa_answers["title"]

        if "axis" in vqa_answers:
            # 解析轴标签
            axis_parts = vqa_answers["axis"].split(",")
            if len(axis_parts) >= 2:
                structure.x_axis_label = axis_parts[0].strip()
                structure.y_axis_label = axis_parts[1].strip()

        if "trend" in vqa_answers:
            structure.trend = self._classify_trend(vqa_answers["trend"])

        if "time_range" in vqa_answers:
            structure.time_range = vqa_answers["time_range"]

    def _classify_trend(self, trend_text: str) -> str:
        """分类趋势"""
        trend_lower = trend_text.lower()

        if any(w in trend_lower for w in ["上升", "增长", "increase", "rise", "grow"]):
            return "increasing"
        elif any(w in trend_lower for w in ["下降", "减少", "decrease", "decline", "fall"]):
            return "decreasing"
        elif any(w in trend_lower for w in ["稳定", "持平", "stable", "flat"]):
            return "stable"
        elif any(w in trend_lower for w in ["波动", "起伏", "fluctuate", "wave"]):
            return "fluctuating"

        return "unknown"

    def _generate_description(
        self,
        structure: ChartStructure,
        vqa_answers: Dict[str, str]
    ) -> str:
        """生成结构化描述"""
        parts = []

        # 标题
        if structure.title:
            parts.append(f"图表标题: {structure.title}")

        # 类型
        if structure.chart_type != ChartType.UNKNOWN:
            type_map = {
                ChartType.LINE: "折线图",
                ChartType.BAR: "柱状图",
                ChartType.PIE: "饼图",
                ChartType.SCATTER: "散点图",
                ChartType.AREA: "面积图",
                ChartType.TABLE: "表格",
            }
            parts.append(f"类型: {type_map.get(structure.chart_type, '未知')}")

        # 轴
        if structure.x_axis_label or structure.y_axis_label:
            parts.append(f"X轴: {structure.x_axis_label}, Y轴: {structure.y_axis_label}")

        # 趋势
        if structure.trend:
            trend_map = {
                "increasing": "呈上升趋势",
                "decreasing": "呈下降趋势",
                "stable": "保持稳定",
                "fluctuating": "呈波动状态",
            }
            parts.append(f"趋势: {trend_map.get(structure.trend, structure.trend)}")

        # 数值范围
        if structure.max_value > 0 or structure.min_value > 0:
            parts.append(f"数值范围: {structure.min_value:.2f} - {structure.max_value:.2f}")

        # 时间范围
        if structure.time_range:
            parts.append(f"时间范围: {structure.time_range}")

        return "; ".join(parts) if parts else "图表信息待解析"

    def _generate_qa_pairs(
        self,
        structure: ChartStructure,
        vqa_answers: Dict[str, str]
    ) -> List[QAPair]:
        """生成问答对"""
        qa_pairs = []

        # 基于图表元素生成问题
        if structure.title:
            qa_pairs.append(QAPair(
                question=f"这个图表的标题是什么？",
                answer=structure.title,
                topic="标题"
            ))

        if structure.chart_type != ChartType.UNKNOWN:
            type_name = structure.chart_type.value
            qa_pairs.append(QAPair(
                question="这是什么类型的图表？",
                answer=f"这是{type_name}图",
                topic="类型"
            ))

        if structure.trend:
            qa_pairs.append(QAPair(
                question="数据呈现什么趋势？",
                answer=structure.trend,
                topic="趋势"
            ))

        if structure.max_value > 0:
            qa_pairs.append(QAPair(
                question="最大值是多少？",
                answer=f"{structure.max_value}",
                topic="数值"
            ))

        if structure.min_value > 0:
            qa_pairs.append(QAPair(
                question="最小值是多少？",
                answer=f"{structure.min_value}",
                topic="数值"
            ))

        # 从 VQA 答案生成
        for key, answer in vqa_answers.items():
            if answer and answer != "Unknown":
                question_templates = {
                    "title": "图表标题是什么？",
                    "trend": "数据趋势是什么？",
                    "time_range": "数据的时间范围是什么？",
                }
                if key in question_templates:
                    qa_pairs.append(QAPair(
                        question=question_templates[key],
                        answer=answer,
                        topic=key
                    ))

        return qa_pairs

    async def batch_understand(
        self,
        chart_sources: List[Union[str, bytes]],
        language: str = "auto"
    ) -> List[ChartAnalysis]:
        """批量理解图表"""
        tasks = [self.understand(source, language) for source in chart_sources]
        return await asyncio.gather(*tasks)


# ========== Factory ==========

def create_chart_understanding_service(
    vqa_model: str = "llava-v1.6-34b",
    vqa_endpoint: Optional[str] = None,
    use_local_vlm: bool = False
) -> ChartUnderstandingService:
    """创建图表理解服务"""
    return ChartUnderstandingService(
        vqa_model=vqa_model,
        vqa_endpoint=vqa_endpoint,
        use_local_vlm=use_local_vlm
    )


# ========== 示例使用 ==========

if __name__ == "__main__":
    async def main():
        service = create_chart_understanding_service()

        # 示例: 解析 HTML 图表
        html_chart = """
        <div class="chart">
            <title>Monthly Sales Report</title>
            <x-axis>Month</x-axis>
            <y-axis>Revenue (USD)</y-axis>
            <data>
                [{"month": "Jan", "value": 1000},
                 {"month": "Feb", "value": 1500},
                 {"month": "Mar", "value": 1200}]
            </data>
        </div>
        """

        analysis = await service.understand(html_chart, language="en")
        print(json.dumps(analysis.to_dict(), indent=2, ensure_ascii=False))

    asyncio.run(main())
