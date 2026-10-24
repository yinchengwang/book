#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VectorBench 主入口 - 向量索引基准测试框架
"""

import argparse
import sys
from pathlib import Path

# 添加项目根目录到路径
project_root = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(project_root))

from benchmark.core.dataset_manager import DatasetManager
from benchmark.core.benchmark_runner import BenchmarkRunner
from benchmark.core.report_generator import ReportGenerator


def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(
        description="VectorBench - 向量索引基准测试框架",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python -m benchmark.main --dataset glove-100-angular
  python -m benchmark.main --algorithm project_hnsw --params M=16,ef_construction=200
  python -m benchmark.main --report --output results/benchmark.csv
        """
    )
    parser.add_argument(
        "--dataset",
        type=str,
        default="glove-100-angular",
        help="数据集名称 (default: glove-100-angular)"
    )
    parser.add_argument(
        "--algorithm",
        type=str,
        default="all",
        help="算法名称 (all/project_hnsw/faiss_hnsw/...)"
    )
    parser.add_argument(
        "--params",
        type=str,
        default="",
        help="参数，格式: key1=value1,key2=value2"
    )
    parser.add_argument(
        "--k",
        type=int,
        default=10,
        help="Top-K (default: 10)"
    )
    parser.add_argument(
        "--output",
        type=str,
        default="results/benchmark.csv",
        help="输出 CSV 文件路径 (default: results/benchmark.csv)"
    )
    parser.add_argument(
        "--report",
        action="store_true",
        help="生成 Markdown 报告"
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="列出所有可用的数据集"
    )
    parser.add_argument(
        "--embedding-model",
        type=str,
        default="auto",
        help="Embedding 模型 (auto/mock:mini-lm-6/all-MiniLM-L6-v2)"
    )
    parser.add_argument(
        "--mock-inference",
        action="store_true",
        help="强制使用 Mock Embedding 模型"
    )
    return parser.parse_args()


def parse_params(params_str: str) -> dict:
    """解析参数字符串"""
    if not params_str:
        return {}
    result = {}
    for pair in params_str.split(","):
        if "=" in pair:
            key, value = pair.split("=", 1)
            key = key.strip()
            value = value.strip()
            # 尝试转换为数字
            try:
                if "." in value:
                    value = float(value)
                else:
                    value = int(value)
            except ValueError:
                pass
            result[key] = value
    return result


def main():
    """主函数"""
    args = parse_args()

    # 列出数据集
    if args.list:
        manager = DatasetManager()
        datasets = manager.list_datasets()
        print("Available datasets:")
        for name in datasets:
            print(f"  - {name}")
        return 0

    # 解析参数
    params = parse_params(args.params)

    # 处理 Embedding 模型选择
    if hasattr(args, 'mock_inference') and args.mock_inference:
        embedding_model = "mock:mini-lm-6"
    elif hasattr(args, 'embedding_model'):
        embedding_model = args.embedding_model
    else:
        embedding_model = "auto"

    # 加载数据集
    print(f"[VectorBench] Loading dataset: {args.dataset}...")
    try:
        manager = DatasetManager()
        dataset = manager.load(args.dataset)
    except Exception as e:
        print(f"[Error] Failed to load dataset: {e}", file=sys.stderr)
        return 1

    # 运行测试
    runner = BenchmarkRunner()

    if args.algorithm == "all":
        print(f"\n[VectorBench] Running all enabled algorithms...")
        results = runner.run_all(dataset, k=args.k)
    else:
        print(f"\n[VectorBench] Running benchmark:")
        print(f"  Algorithm: {args.algorithm}")
        print(f"  Dataset: {dataset.name}")
        print(f"  K: {args.k}")
        if params:
            print(f"  Params: {params}")
        result = runner.run(args.algorithm, dataset, k=args.k, params=params)
        results = [result]

    # 输出结果
    print(f"\n{'='*60}")
    print("Results:")
    print(f"{'='*60}")
    for result in results:
        print(f"\n  Algorithm: {result.algorithm}")
        print(f"  Dataset: {result.dataset}")
        print(f"  Recall@1:  {result.recall_at_1:.4f}")
        print(f"  Recall@10: {result.recall_at_10:.4f}")
        print(f"  Recall@100: {result.recall_at_100:.4f}")
        print(f"  QPS: {result.qps:.2f}")
        print(f"  Build Time: {result.build_time:.2f}s")
        print(f"  Memory: {result.memory_mb:.2f} MB")

    # 保存结果
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    for result in results:
        result.save(str(output_path))

    print(f"\n[VectorBench] Results saved to: {output_path}")

    # 生成报告
    if args.report:
        reporter = ReportGenerator()
        report_path = str(output_path).replace(".csv", "_report.md")
        reporter.generate(results, report_path)
        print(f"[VectorBench] Report saved to: {report_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
