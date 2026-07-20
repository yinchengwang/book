# ANN-Benchmarks 架构设计

## 学习目标
- 理解 ANN-Benchmarks 的整体架构分层
- 掌握评测流水线的数据流

## 整体架构

```mermaid
graph TB
    subgraph "数据集层"
        DS_SIFT["SIFT-128<br/>1M base / 10K query"]
        DS_GLOVE["GloVe-100<br/>1.2M / 10K"]
        DS_DEEP["Deep-96<br/>1M / 10K"]
        DS_CONTRIEVER["Contriever-768<br/>1M / 10K"]
    end

    subgraph "算法层"
        ALG_HNSW["HNSWlib"]
        ALG_FAISS["FAISS<br/>IVF / PQ / Flat"]
        ALG_ANNOY["Annoy"]
        ALG_SCANN["ScaNN"]
        ALG_NGT["NGT"]
        ALG_DISKANN["DiskANN"]
    end

    subgraph "评测引擎"
        RUNNER["runner.py<br/>评测运行器"]
        CONFIG["算法参数配置<br/>YAML / JSON"]
        INSTALL["Docker 自动安装"]
    end

    subgraph "评价指标"
        RECALL["Recall<br/>召回率"]
        QPS["QPS<br/>每秒查询数"]
        BUILD["Build Time<br/>索引构建时间"]
        MEM["Memory<br/>内存占用"]
    end

    subgraph "结果输出"
        PLOT["plot.py<br/>Recall-vs-QPS 图"]
        JSON_RES["results/*.json<br/>原始结果"]
        HTML["排行榜 HTML"]
    end

    DS_SIFT --> RUNNER
    DS_GLOVE --> RUNNER
    DS_DEEP --> RUNNER
    DS_CONTRIEVER --> RUNNER
    RUNNER --> ALG_HNSW
    RUNNER --> ALG_FAISS
    RUNNER --> ALG_ANNOY
    RUNNER --> ALG_SCANN
    RUNNER --> ALG_NGT
    RUNNER --> ALG_DISKANN
    CONFIG --> RUNNER
    INSTALL --> RUNNER
    ALG_HNSW --> RECALL
    ALG_FAISS --> QPS
    ALG_ANNOY --> BUILD
    ALG_SCANN --> MEM
    RECALL --> JSON_RES
    QPS --> JSON_RES
    BUILD --> JSON_RES
    MEM --> JSON_RES
    JSON_RES --> PLOT
    JSON_RES --> HTML
```

## 评测流水线

```mermaid
sequenceDiagram
    participant U as 用户
    participant R as runner.py
    participant D as Docker
    participant A as 算法容器
    participant E as 评测存储

    U->>R: python run.py --algorithm=hnsw
    R->>D: 构建算法 Docker 镜像
    D->>A: 启动容器
    R->>A: 发送数据集 & 参数
    A->>A: 构建索引
    A->>A: 执行查询
    A-->>R: 返回结果 JSON
    R->>E: 存储结果
    U->>R: python plot.py
    R->>E: 读取结果
    R->>U: 生成 Recall-vs-QPS 图
```

## 数据格式

### fvecs / ivecs 格式

```
4字节 dim + dim * 4字节 float/int = 一个向量
```

```
# 读取示例（C 风格）
for each vector:
    dim = read_int32(file)        # 4 字节维度
    for i in range(dim):
        data[i] = read_float(file) # 4 字节浮点
```

### HDF5 格式

```
数据集文件结构：
├── train      # 训练集（base 向量）
├── test       # 测试集（query 向量）
├── neighbors  # 精确最近邻（ground truth）
├── distances  # 精确距离
```

## 目录结构

```
ann-benchmarks/
├── ann_benchmarks/        # 核心代码
│   ├── __init__.py
│   ├── runner.py          # 评测运行器
│   ├── plot.py            # 结果可视化
│   ├── measure.py         # 指标计算
│   ├── algorithms/        # 算法 Dockerfile
│   │   ├── hnsw.py
│   │   ├── faiss.py
│   │   └── annoy.py
│   └── datasets/          # 数据集定义
├── results/               # 评测结果 JSON
├── install/               # Docker 安装脚本
└── Dockerfile             # 基础镜像
```

## 要点总结

- 架构分为数据集层、算法层、评测引擎和结果输出四层
- 评测流水线：Docker 自动化构建 → 算法容器 → 索引构建 → 查询 → 结果收集 → 可视化
- 数据格式支持 fvecs/ivecs（标准向量格式）和 HDF5（含 ground truth）
- 核心指标：Recall、QPS、Build Time、Memory

## 思考题

1. 为什么 ANN-Benchmarks 选择 Docker 容器化算法，而不是直接在宿主机运行？
2. fvecs 格式中每个向量开头存储 dim 的设计有什么好处？
3. 评测结果中 Recall 和 QPS 通常存在 trade-off，为什么？