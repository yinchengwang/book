# 向量检索基准测试框架横向对比

## 分类概述

向量检索基准测试框架用于评估和比较不同向量索引/数据库的 ANN 搜索性能。通过标准化的数据集、查询集和评估指标，为选型和调优提供客观数据支撑，帮助开发者选择最适合特定场景的解决方案。

## 库一览

- **ANN-Benchmarks** - 向量检索标准基准测试框架，评估 HNSW/FAISS/Annoy 等

## 功能特性

| 维度 | ANN-Benchmarks |
|------|----------------|
| 编程语言 | Python |
| 开源协议 | Apache 2.0 |
| 首次发布 | 2018 |
| GitHub Stars | 8k+ |
| 支持算法 | HNSW/IVF/PQ/LSH/BruteForce 等 |
| 数据集 | SIFT/GIST/DEEP/FAST/MOVIELENS |
| 评估指标 | QPS/Recall/Latency |
| 支持引擎 | Faiss/Annoy/ScaNN/VEAGAS/DiskANN/NMSLIB |
| 可扩展性 | 支持自定义算法和数据集 |
| 一句话定位 | 向量检索性能标准测试框架 |

## 核心评估指标

**性能指标**：
- **QPS (Queries Per Second)** — 每秒查询数，越高越好
- **Recall@K** — Top-K 结果中相关结果的比例
- **Latency** — 单次查询延迟，P99/P95 更有意义
- **Memory Usage** — 内存占用，资源受限场景关键
- **Build Time** — 索引构建时间，快重载场景重要

**数据集说明**：
| 数据集 | 维度 | 数据量 | 类型 |
|--------|------|--------|------|
| SIFT-128-euclidean | 128 | 100万 | 图像描述子 |
| GIST-960-euclidean | 960 | 100万 | 图像特征 |
| DEEP-96-euclidean | 96 | 100万 | 深度学习特征 |
| MOVIELENS-20M | 1368 | 2000万 | 推荐系统 |
| Nb Billion-scale | 128 | 10亿+ | 超大规模 |

## 选型建议

**基于基准测试的选型**：
- **追求极致 QPS**：HNSW 类算法（ScaNN、HNSW）
- **内存受限**：PQ 量化（Faiss-PQ、Milvus）
- **超大规模数据**：DiskANN、IVF 索引
- **高精度需求**：Brute Force 或高参数 HNSW

## 学习路径

1. **先学向量索引原理** — 理解 HNSW/IVF/PQ 算法
2. **再学 ANN-Benchmarks 使用** — 运行标准测试
3. **理解评估指标** — Recall vs QPS 权衡
4. **针对性调优** — 根据基准结果优化参数

## 关联项目

- `index/vector/` — 项目向量索引实现
- `index/hnsw/` — HNSW 算法实现
- `docs/db_wiki/03_vector/` — 向量数据库对比
