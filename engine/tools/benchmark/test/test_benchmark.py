#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VectorBench 集成测试

测试所有核心组件的功能
"""

import sys
import os
from pathlib import Path

# 添加路径 - 从 test 目录向上找到 engineering，然后添加
current_file = Path(__file__).resolve()
# test/test_benchmark.py -> test -> benchmark -> tools -> engineering
sys.path.insert(0, str(current_file.parent.parent.parent))
os.chdir(sys.path[0])

import numpy as np


def test_adapter_registry():
    """测试适配器工厂"""
    from benchmark.adapters import create_adapter, BaseIndexAdapter

    # project_hnsw 使用纯 Python 实现，不需要 faiss
    adapter = create_adapter("project_hnsw", {"dimension": 128})
    assert isinstance(adapter, BaseIndexAdapter), "Should be BaseIndexAdapter"
    print("  PASS: create_adapter('project_hnsw')")

    # 测试 ivf_pq 适配器
    adapter = create_adapter("project_ivf_pq", {"dimension": 128})
    assert isinstance(adapter, BaseIndexAdapter)
    print("  PASS: create_adapter('project_ivf_pq')")

    # 测试 lsh 适配器
    adapter = create_adapter("project_lsh", {"dimension": 128})
    assert isinstance(adapter, BaseIndexAdapter)
    print("  PASS: create_adapter('project_lsh')")

    # 测试 diskann 适配器
    adapter = create_adapter("project_diskann", {"dimension": 128})
    assert isinstance(adapter, BaseIndexAdapter)
    print("  PASS: create_adapter('project_diskann')")


def test_hnsw_fallback():
    """测试 HNSW 后备实现"""
    np.random.seed(42)
    dimension = 64
    n_vectors = 500
    n_queries = 50

    vectors = np.random.random((n_vectors, dimension)).astype(np.float32)
    queries = np.random.random((n_queries, dimension)).astype(np.float32)

    from benchmark.adapters import create_adapter

    adapter = create_adapter("project_hnsw", {"dimension": dimension, "M": 8})
    adapter.fit(vectors)
    results = adapter.search(queries, k=10)
    assert results.shape == (n_queries, 10)

    print("  PASS: project_hnsw fallback search")


def test_recall_computation():
    """测试召回率计算"""
    from benchmark.core.benchmark_runner import BenchmarkRunner

    runner = BenchmarkRunner()

    predicted = np.array([
        [0, 1, 2, 3, 4],
        [0, 1, 2, 3, 4],
        [0, 1, 2, 3, 4],
    ])
    ground_truth = np.array([
        [0, 1, 6, 7, 8],
        [0, 1, 2, 9, 10],
        [5, 6, 7, 8, 9],
    ])

    recall = runner._compute_recall(predicted, ground_truth, k=2)
    expected = 4 / 6
    assert abs(recall - expected) < 0.01

    print("  PASS: recall computation")


def test_dataset_manager():
    """测试数据集管理器"""
    from benchmark.core.dataset_manager import DatasetManager

    manager = DatasetManager()
    datasets = manager.list_datasets()
    assert len(datasets) > 0

    print(f"  PASS: dataset manager ({len(datasets)} datasets)")


def test_result_save():
    """测试结果保存"""
    from benchmark.core.benchmark_runner import BenchmarkResult
    import tempfile

    result = BenchmarkResult(
        algorithm="test_algo",
        dataset="test_dataset",
        dimension=128,
        recall_at_1=0.95,
        recall_at_10=0.98,
        recall_at_100=0.99,
        qps=1000.0,
        build_time=1.5,
        memory_bytes=1024 * 1024,
        params={"M": 16}
    )

    with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
        temp_path = f.name

    try:
        result.save(temp_path)
        assert os.path.exists(temp_path)
        with open(temp_path, 'r') as f:
            content = f.read()
        assert "test_algo" in content
        print("  PASS: result save")
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)


def test_quantization_benchmark():
    """测试量化基准模块"""
    from benchmark.quantization import (
        QuantizationConfig, QuantizationBenchmark,
        PQQuantizer, LVQQuantizer, SQQuantizer
    )

    np.random.seed(42)
    dimension = 64
    n_vectors = 500

    # 测试 PQ 量化器
    train_vectors = np.random.random((n_vectors, dimension)).astype(np.float32)
    query_vectors = np.random.random((100, dimension)).astype(np.float32)

    # PQ 量化
    pq = PQQuantizer(dimension, m=8, bits=8)
    pq.fit(train_vectors)
    codes = pq.encode(query_vectors)
    assert codes.shape == (100, 8)
    print("  PASS: PQ quantizer")

    # LVQ 量化
    lvq = LVQQuantizer(dimension, bits=8)
    lvq.fit(train_vectors)
    codes = lvq.encode(query_vectors)
    assert len(codes) == 100
    print("  PASS: LVQ quantizer")

    # SQ 量化
    sq = SQQuantizer(dimension, bits=8)
    sq.fit(train_vectors)
    codes = sq.encode(query_vectors)
    assert codes.shape == query_vectors.shape
    print("  PASS: SQ quantizer")


def test_inference_latency():
    """测试端侧推理模块"""
    from benchmark.inference import (
        MockEmbeddingModel, InferenceLatencyAnalyzer
    )

    # 测试模型配置
    model = MockEmbeddingModel("mini-lm-6")
    assert model.dimension == 384
    config = model.get_latency_config()
    assert config.model_name == "mini-lm-6"
    print("  PASS: MockEmbeddingModel")

    # 测试向量编码
    vectors = model.encode(["test1", "test2", "test3"])
    assert vectors.shape == (3, 384)
    print("  PASS: model encode")

    # 测试分析器
    analyzer = InferenceLatencyAnalyzer()
    results = analyzer.run_model_comparison(
        index_search_qps=10000,
        n_queries=100
    )
    assert len(results) > 0
    assert all(isinstance(r, type(results[list(results.keys())[0]]))
               for r in results.values())
    print(f"  PASS: InferenceLatencyAnalyzer ({len(results)} models)")


def test_diskann_adapter():
    """测试 DiskANN 适配器"""
    np.random.seed(42)
    dimension = 64
    n_vectors = 300
    n_queries = 30

    vectors = np.random.random((n_vectors, dimension)).astype(np.float32)
    queries = np.random.random((n_queries, dimension)).astype(np.float32)

    from benchmark.adapters import create_adapter

    adapter = create_adapter("project_diskann", {"dimension": dimension})
    adapter.fit(vectors)
    results = adapter.search(queries, k=10)
    assert results.shape == (n_queries, 10)

    print("  PASS: project_diskann fallback search")


def test_real_embedding_model():
    """测试真实 Embedding 模型"""
    from benchmark.inference import (
        RealEmbeddingModel, create_embedding_model
    )

    # 测试模型列表
    models = RealEmbeddingModel.list_available_models()
    assert len(models) > 0
    print(f"  PASS: available models: {models}")

    # 测试 mock 模型
    mock = create_embedding_model("mock:mini-lm-6")
    vectors = mock.encode(["test1", "test2"])
    assert vectors.shape == (2, 384)
    print("  PASS: mock model encode")

    # 测试真实模型（如果可用）
    try:
        real = create_embedding_model("auto")
        vectors = real.encode(["hello world"])
        assert vectors.shape[1] > 0
        print("  PASS: real model encode")
    except Exception as e:
        print(f"  SKIP: real model not available ({e})")


def test_end_to_end_benchmark():
    """测试端到端基准测试"""
    from benchmark.inference import EndToEndBenchmark

    # 使用小规模测试数据
    corpus = [f"document {i}" for i in range(100)]
    queries = [f"query {i}" for i in range(10)]

    benchmark = EndToEndBenchmark()
    result = benchmark.run(
        corpus_texts=corpus,
        query_texts=queries,
        embedding_model="mock:mini-lm-6",
        algorithm="project_hnsw",
        k=10
    )

    assert result.total_queries == 10
    assert result.effective_qps > 0
    print(f"  PASS: end-to-end QPS={result.effective_qps:.0f}")


def main():
    """运行所有测试"""
    print("=" * 60)
    print("VectorBench Integration Tests")
    print("=" * 60)
    print(f"Working directory: {os.getcwd()}")
    print()

    tests = [
        ("Adapter Registry", test_adapter_registry),
        ("DiskANN Adapter", test_diskann_adapter),
        ("HNSW Fallback", test_hnsw_fallback),
        ("Recall Computation", test_recall_computation),
        ("Dataset Manager", test_dataset_manager),
        ("Result Save", test_result_save),
        ("Quantization Benchmark", test_quantization_benchmark),
        ("Inference Latency", test_inference_latency),
        ("Real Embedding Model", test_real_embedding_model),
        ("End-to-End Benchmark", test_end_to_end_benchmark),
    ]

    passed = 0
    failed = 0

    for name, test_func in tests:
        try:
            print(f"Test: {name}")
            test_func()
            passed += 1
        except Exception as e:
            print(f"  FAIL: {e}")
            failed += 1

    print()
    print("=" * 60)
    print(f"Results: {passed} passed, {failed} failed")
    print("=" * 60)

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
