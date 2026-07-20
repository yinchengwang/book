# ANN-Benchmarks 关键特性

## 学习目标
- 了解 ANN-Benchmarks 的关键特性
- 掌握这些特性如何支持向量检索算法评测

## 特性总览

```mermaid
graph TD
    A[ANN-Benchmarks 关键特性] --> B[多样化数据集]
    A --> C[多算法支持]
    A --> D[自动安装与容器化]
    A --> E[标准化评测流程]
    A --> F[结果可视化]
    A --> G[可扩展框架]
    A --> H[公开排行榜]
```

## 多样化数据集

| 数据集 | 维度 | 向量数 | 查询数 | 距离度量 | 说明 |
|--------|------|--------|--------|----------|------|
| SIFT-128 | 128 | 1,000,000 | 10,000 | L2 | 图像 SIFT 特征 |
| GIST-960 | 960 | 1,000,000 | 1,000 | L2 | 图像 GIST 特征 |
| GloVe-100 | 100 | 1,183,514 | 10,000 | 余弦 | 词向量 |
| GloVe-200 | 200 | 1,183,514 | 10,000 | 余弦 | 词向量 |
| Deep-96 | 96 | 1,000,000 | 10,000 | L2 | 深度学习特征 |
| Deep-256 | 256 | 1,000,000 | 10,000 | L2 | 深度学习特征 |
| Contriever-768 | 768 | 1,000,000 | 10,000 | 余弦 | 文本嵌入 |
| SPACE-v1 | 100 | 1,000,000 | 10,000 | 内积 | 最大内积搜索 |

## 多算法支持

```mermaid
graph LR
    A[支持算法] --> B[HNSWlib]
    A --> C[FAISS]
    A --> D[Annoy]
    A --> E[ScaNN]
    A --> F[NGT]
    A --> G[DiskANN]
    A --> H[LSH]
    A --> I[FLANN]
    A --> J[MRPT]
    A --> K[Vearch]
    B --> L["精度高<br/>速度极快"]
    C --> M["组合丰富<br/>IVF/PQ/Flat"]
    D --> N["简单可靠<br/>静态索引"]
    E --> O["Google 出品<br/>PQ 优化"]
    F --> Q["Yahoo 日本<br/>多种算法"]
    G --> R["磁盘索引<br/>超大容量"]
```

## 自动安装与容器化

```mermaid
flowchart TD
    A[用户运行评测] --> B[Docker 构建]
    B --> C{是否需要安装?}
    C -->|是| D[自动下载数据集]
    C -->|是| E[编译算法库]
    C -->|是| F[安装 Python 依赖]
    C -->|否| G[直接运行]
    D --> H[启动容器]
    E --> H
    F --> H
    H --> I[执行评测]
    I --> J[输出结果]
```

## 标准化评测流程

每个算法的评测流程一致：

1. **加载数据集**：读取 base 向量和 query 向量
2. **构建索引**：使用算法参数配置构建索引
3. **执行查询**：对每个 query 执行 k-NN 搜索
4. **计算召回**：与 ground truth 对比计算召回率
5. **记录性能**：记录 QPS、构建时间、内存占用
6. **存储结果**：JSON 格式保存

## 结果可视化

```mermaid
graph TD
    A[JSON 结果] --> B[plot.py]
    B --> C[Recall-vs-QPS 图]
    B --> D[Recall-vs-Build Time 图]
    B --> E[多算法对比图]
    C --> F[排行榜]
    C --> G[选型参考]
```

## 可扩展框架

添加新算法步骤：

```python
# 1. 在 ann_benchmarks/algorithms/ 下创建 Dockerfile
# 2. 在 algorithms/ 下添加算法包装器
# 3. 在配置中定义参数搜索空间

# 配置示例（YAML）
hnsw:
  docker-tag: ghcr.io/erikbern/ann-benchmarks-hnsw:latest
  module: ann_benchmarks.algorithms.hnsw
  constructor: HNSW
  base-args: ["@metric"]
  run-groups:
    M:
      args: [16]
      construction-args:
        efConstruction: [200, 400]
```

## 要点总结

- 提供从图像到文本的多样化标准数据集，覆盖不同维度和规模
- 支持 10+ 主流向量检索算法，评测框架统一
- Docker 容器化实现零配置自动安装，保证评测环境一致性
- 标准化评测流程确保各算法结果可比
- 可视化生成 Recall-vs-QPS 图，直观展示精度-性能权衡
- 开放架构易于扩展新算法和新数据集

## 思考题

1. 不同数据集上的评测结果为何不能直接类比？
2. 容器化评测相比裸机评测有哪些优缺点？
3. 如何为新算法添加 ANN-Benchmarks 支持？