#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
报告生成器
"""

from pathlib import Path
from typing import List
from datetime import datetime


class ReportGenerator:
    """报告生成器"""

    def __init__(self):
        pass

    def generate(self, results: List, output_path: str) -> None:
        """
        生成 Markdown 报告

        Args:
            results: BenchmarkResult 列表
            output_path: 输出文件路径
        """
        if not results:
            print("[ReportGenerator] No results to generate report.")
            return

        # 构建报告内容
        lines = []
        lines.append("# VectorBench 测试报告")
        lines.append("")
        lines.append(f"**生成时间**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append("")

        # 数据集概览
        lines.append("## 数据集概览")
        lines.append("")
        lines.append("| 数据集 | 维度 | 训练集 | 测试集 | 距离度量 |")
        lines.append("|--------|------|--------|--------|----------|")

        datasets = set(r.dataset for r in results)
        for ds_name in sorted(datasets):
            ds_results = [r for r in results if r.dataset == ds_name]
            if ds_results:
                r = ds_results[0]
                lines.append(
                    f"| {ds_name} | {r.dimension} | N/A | N/A | N/A |"
                )
        lines.append("")

        # 性能对比表格
        lines.append("## 性能对比")
        lines.append("")
        lines.append("### 召回率与吞吐量")
        lines.append("")
        lines.append(
            "| 算法 | Recall@1 | Recall@10 | Recall@100 | QPS | "
            "构建时间(s) | 内存(MB) |"
        )
        lines.append(
            "|------|----------|-----------|------------|-----|"
            "-------------|----------|"
        )

        for r in sorted(results, key=lambda x: -x.recall_at_10):
            params_str = ", ".join(f"{k}={v}" for k, v in r.params.items()) or "default"
            lines.append(
                f"| {r.algorithm} | {r.recall_at_1:.4f} | {r.recall_at_10:.4f} | "
                f"{r.recall_at_100:.4f} | {r.qps:.2f} | {r.build_time:.2f} | "
                f"{r.memory_mb:.2f} |"
            )
        lines.append("")

        # 参数详情
        lines.append("### 参数详情")
        lines.append("")
        lines.append("| 算法 | 参数 |")
        lines.append("|------|------|")
        for r in results:
            if r.params:
                param_str = ", ".join(f"{k}={v}" for k, v in r.params.items())
            else:
                param_str = "default"
            lines.append(f"| {r.algorithm} | {param_str} |")
        lines.append("")

        # QPS vs 召回率 散点图数据
        lines.append("### QPS vs 召回率数据点")
        lines.append("")
        lines.append("```")
        lines.append("# algorithm,recall@10,qps")
        for r in sorted(results, key=lambda x: -x.recall_at_10):
            lines.append(f"{r.algorithm},{r.recall_at_10:.4f},{r.qps:.2f}")
        lines.append("```")
        lines.append("")

        # 结论
        lines.append("## 结论")
        lines.append("")

        if results:
            best_recall = max(results, key=lambda x: x.recall_at_10)
            best_qps = max(results, key=lambda x: x.qps)

            lines.append(f"- **最高召回率**: {best_recall.algorithm} "
                        f"(Recall@10={best_recall.recall_at_10:.4f})")
            lines.append(f"- **最高 QPS**: {best_qps.algorithm} "
                        f"({best_qps.qps:.2f} QPS)")
            lines.append("")
            lines.append("**性价比分析**: 需要综合考虑召回率和 QPS 进行权衡。")
            lines.append("")

        # 写入文件
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        content = "\n".join(lines)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(content)

        print(f"[ReportGenerator] Report generated: {output_path}")
