# 降维学习笔记

## 核心概念

- **PCA**: 主成分分析，通过协方差矩阵特征分解找到最大方差方向
- **LDA**: 线性判别分析，最大化类间散度与类内散度之比
- **t-SNE**: 基于概率分布的流形学习，保留局部邻域结构
- **UMAP**: 基于黎曼几何和拓扑数据分析的统一流形逼近

## 工程对照

本仓库的向量量化模块与降维思想一脉相承。`engineering/include/algo-prod/quantization/pq.h` 实现的乘积量化（Product Quantization）本质上是一种有损降维压缩技术：它将高维向量空间分解为多个低维子空间的笛卡尔积，对每个子空间独立聚类编码。这与 PCA 的线性降维思路不同——PCA 寻找全局主成分方向，而 PQ 在子空间级别做局部量化。`engineering/include/algo-prod/quantization/lvq.h` 的 LVQ（潜在变量量化）和 `rq.h` 的残差量化进一步在逐级残差上压缩。在 HNSW 索引 (`engineering/include/db/index/vector_index/hnsw/faiss_hnsw.h`) 中，降维直接体现在图构建的层级结构上——高层图是底层图的低维"摘要"，通过指数衰减的概率分布控制层级分配。这种从原始空间到低维空间的信息压缩范式，正是降维在工程落地中的体现。
