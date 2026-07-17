# 聚类学习笔记

## 核心概念

- **K-Means**: 基于质心的划分聚类，迭代优化簇内距离平方和
- **层次聚类**: 凝聚式（自底向上合并）vs 分裂式（自顶向下分割），输出树状图
- **DBSCAN**: 基于密度的聚类，自动发现任意形状簇，识别噪声点
- **评估指标**: 轮廓系数、Calinski-Harabasz 指数、Davies-Bouldin 指数

## 工程对照

本仓库的向量索引模块大量使用了聚类思想。`engineering/include/db/index/vector_index/ivf/IndexIVF.h` 实现的 IVF（倒排文件）索引，核心就是 K-Means 聚类——将向量空间划分为 Voronoi 单元，每个质心对应一个倒排列表。搜索时先定位最近的 K 个质心（即聚类簇），只在对应的子空间内执行暴力搜索，大幅降低搜索范围。`engineering/include/db/index/vector_index/ivf_hnsw/IndexIVFHNSW.h` 进一步在 IVF 的每个聚类簇上叠加 HNSW 图索引，形成了"粗粒度聚类 + 细粒度图搜索"的两级索引架构。这与 DBSCAN 的密度连通性思路形成对比：IVF 是硬划分（每个点属于一簇），而 DBSCAN 允许噪声点和任意形状。仓库中的向量索引逻辑本质上就是在做稠密向量空间的聚类与搜索联合优化。
