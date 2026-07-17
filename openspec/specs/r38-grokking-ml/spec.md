# R38 Grokking 机器学习系统能力规格

## 概述

R38 新增 Grokking 机器学习系统能力，涵盖 14 张学习卡片，包括传统机器学习算法、深度学习基础和应用场景。

## 能力列表

### 传统机器学习

| 卡片 ID | 标题 | 难度 | 描述 |
|---------|------|------|------|
| ml_fundamentals | 机器学习基础 | basic | 监督/无监督学习、训练测试划分、过拟合欠拟合 |
| linear_regression | 线性回归 | basic | 一元/多元回归、梯度下降、评估指标 |
| logistic_regression | 逻辑回归 | basic | 二分类、Sigmoid、交叉熵损失 |
| decision_tree | 决策树 | basic | ID3/CART、信息增益、剪枝 |
| random_forest | 随机森林 | intermediate | Bagging、特征抽样、集成学习 |
| gradient_boosting | 梯度提升 | intermediate | GBDT、残差拟合、正则化 |
| svm | 支持向量机 | intermediate | 最大间隔、核函数、软间隔 |
| clustering | 聚类 | basic | K-Means、层次聚类、DBSCAN |
| dimensionality_reduction | 降维 | intermediate | PCA、LDA、t-SNE、UMAP |

### 深度学习

| 卡片 ID | 标题 | 难度 | 描述 |
|---------|------|------|------|
| neural_network | 神经网络 | intermediate | MLP、反向传播、激活函数 |
| cnn | 卷积神经网络 | intermediate | 卷积、池化、图像分类 |
| rnn | 循环神经网络 | intermediate | LSTM、GRU、序列建模 |

### 应用

| 卡片 ID | 标题 | 难度 | 描述 |
|---------|------|------|------|
| nlp | 自然语言处理 | intermediate | 词嵌入、TextCNN、情感分析 |
| recommendation | 推荐系统 | intermediate | 协同过滤、矩阵分解、评估指标 |

## 技术栈

- **语言**: Python 3.x
- **依赖**: 纯 numpy 实现，无外部 ML 框架依赖
- **结构**: 每张卡包含 main.py、Makefile、README.md、NOTES.md

## 与工程轨的对照

每张卡的 NOTES.md 都引用了本仓库的实际代码模块作为工程对照，建立了 ML 概念与数据库内核、索引结构、存储引擎之间的概念桥梁。

## 变更记录

- 2026-07-12: 初始版本，14 张 ML 学习卡片完成