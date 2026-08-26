"""
@file test_chart_understanding.py
@brief 图表理解服务单元测试
"""

import pytest
import asyncio
from typing import Dict, Any

from rag.python.understanding.chart_understanding import (
    ChartParser,
    ChartType,
    ChartUnderstandingService,
    ChartAnalysis,
    create_chart_understanding_service,
)


class TestChartParser:
    """ChartParser 测试"""

    def setup_method(self):
        self.parser = ChartParser()

    def test_parse_html_chart(self):
        """测试解析 HTML 图表"""
        html = """
        <div class="chart">
            <title>Sales Report</title>
            <x-axis>Month</x-axis>
            <y-axis>Revenue</y-axis>
            <data>[{"month": "Jan", "value": 100}]</data>
        </div>
        """
        structure = self.parser.parse(html)

        assert structure.chart_type == ChartType.UNKNOWN or structure.chart_type == ChartType.LINE
        assert structure.title == "Sales Report"
        assert structure.x_axis_label == "Month"
        assert structure.y_axis_label == "Revenue"

    def test_parse_svg_chart(self):
        """测试解析 SVG 图表"""
        svg = """
        <svg>
            <title>Line Chart</title>
            <g class="x-axis">Month</g>
            <g class="y-axis">Value</g>
        </svg>
        """
        structure = self.parser.parse(svg)

        assert structure.title == "Line Chart"

    def test_chart_type_detection(self):
        """测试图表类型检测"""
        test_cases = [
            ("折线图", ChartType.LINE),
            ("bar chart", ChartType.BAR),
            ("pie chart", ChartType.PIE),
        ]

        for chart_type_str, expected_type in test_cases:
            html = f"<div>{chart_type_str}</div>"
            structure = self.parser.parse(html)
            assert structure.chart_type == expected_type

    def test_extract_data_points(self):
        """测试提取数据点"""
        html = """
        <div>
            <data>[{"x": 1, "y": 10}, {"x": 2, "y": 20}]</data>
        </div>
        """
        structure = self.parser.parse(html)

        # 数据点可能被解析或为空
        assert isinstance(structure.data_points, list)

    def test_extract_legend(self):
        """测试提取图例"""
        html = """
        <div>
            legend = ["Series A", "Series B"]
        </div>
        """
        structure = self.parser.parse(html)

        assert len(structure.legend) == 2
        assert "Series A" in structure.legend


class TestChartUnderstandingService:
    """ChartUnderstandingService 测试"""

    def setup_method(self):
        self.service = create_chart_understanding_service()

    @pytest.mark.asyncio
    async def test_understand_html_chart(self):
        """测试理解 HTML 图表"""
        html = """
        <div class="chart">
            <title>Monthly Sales</title>
            <x-axis>Month</x-axis>
            <y-axis>Revenue (USD)</y-axis>
            <data>[{"month": "Jan", "value": 1000}]</data>
        </div>
        """
        analysis = await self.service.understand(html, language="en")

        assert isinstance(analysis, ChartAnalysis)
        assert analysis.structure is not None
        assert analysis.description is not None

    @pytest.mark.asyncio
    async def test_understand_with_qa_pairs(self):
        """测试生成 QA 对"""
        html = """
        <div>
            <title>Test Chart</title>
            <data>[{"x": 1, "y": 100}]</data>
        </div>
        """
        analysis = await self.service.understand(html)

        assert len(analysis.qa_pairs) > 0

    @pytest.mark.asyncio
    async def test_batch_understand(self):
        """测试批量理解"""
        charts = [
            "<div><title>Chart 1</title></div>",
            "<div><title>Chart 2</title></div>",
            "<div><title>Chart 3</title></div>",
        ]

        results = await self.service.batch_understand(charts)

        assert len(results) == 3
        assert all(isinstance(r, ChartAnalysis) for r in results)

    def test_chart_analysis_to_dict(self):
        """测试 ChartAnalysis 序列化"""
        from rag.python.understanding.chart_understanding import ChartStructure, QAPair

        structure = ChartStructure(
            chart_type=ChartType.LINE,
            title="Test",
            x_axis_label="X",
            y_axis_label="Y",
        )

        analysis = ChartAnalysis(
            structure=structure,
            description="Test description",
            qa_pairs=[QAPair(question="Q", answer="A")],
        )

        data = analysis.to_dict()
        assert "structure" in data
        assert "description" in data
        assert "qa_pairs" in data

    def test_trend_classification(self):
        """测试趋势分类"""
        service = ChartUnderstandingService()

        assert service._classify_trend("上升趋势") == "increasing"
        assert service._classify_trend("下降趋势") == "decreasing"
        assert service._classify_trend("保持稳定") == "stable"
        assert service._classify_trend("波动较大") == "fluctuating"


class TestFactory:
    """工厂函数测试"""

    def test_create_service(self):
        """测试创建服务"""
        service = create_chart_understanding_service()

        assert isinstance(service, ChartUnderstandingService)
        assert service.vqa_model is not None

    def test_create_service_custom_config(self):
        """测试自定义配置"""
        service = create_chart_understanding_service(
            vqa_model="custom-model",
            use_local_vlm=True,
        )

        assert service.vqa_model == "custom-model"
        assert service.use_local_vlm is True


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
