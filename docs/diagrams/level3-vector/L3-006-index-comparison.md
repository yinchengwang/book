%% 向量索引对比表 - L3-006
%% 各索引类型的特点对比

flowchart LR
    subgraph 对比维度["对比维度"]
        QPS[查询 QPS]
        MEM[内存占用]
        BUILD[构建速度]
        SCALE[数据规模]
        SCENARIO[适用场景]
    end

    subgraph HNSW["HNSW"]
        H_QPS[⭐⭐⭐⭐⭐ 高]
        H_MEM[⭐⭐⭐ 较高]
        H_BUILD[⭐⭐⭐⭐ 快]
        H_SCALE[百万级]
        H_SCENARIO[高精度召回<br/>实时查询]
    end

    subgraph IVF_PQ["IVF+PQ"]
        I_QPS[⭐⭐⭐⭐ 高]
        I_MEM[⭐⭐⭐⭐ 低]
        I_BUILD[⭐⭐⭐ 中]
        I_SCALE[千万级]
        I_SCENARIO[内存受限<br/>精度可调]
    end

    subgraph DiskANN["DiskANN"]
        D_QPS[⭐⭐⭐ 中]
        D_MEM[⭐⭐⭐⭐⭐ 极低]
        D_BUILD[⭐⭐⭐ 中]
        D_SCALE[亿级+]
        D_SCENARIO[超大规模<br/>SSD 存储]
    end

    subgraph BM25["BM25"]
        B_QPS[⭐⭐⭐⭐⭐ 高]
        B_MEM[⭐⭐⭐⭐ 低]
        B_BUILD[⭐⭐⭐⭐ 快]
        B_SCALE[无限制]
        B_SCENARIO[文本搜索<br/>稀疏向量]
    end

    QPS --> H_QPS
    QPS --> I_QPS
    QPS --> D_QPS
    QPS --> B_QPS

    style HNSW fill:#d0bfff,stroke:#7048e8
    style IVF_PQ fill:#e7f5ff,stroke:#1971c7
    style DiskANN fill:#fff9db,stroke:#f59f00
    style BM25 fill:#d3f9d8,stroke:#2f9e44
