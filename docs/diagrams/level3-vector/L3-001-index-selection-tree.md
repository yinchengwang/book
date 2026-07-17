%% 向量索引选择决策树 - L3-001
%% 根据数据规模、精度要求、内存限制选择索引

flowchart TD
    START([开始]) --> Q1{数据规模}

    Q1 -->|小规模<br/>&lt;100万| Q2{精度要求}
    Q1 -->|中规模<br/>100万-1亿| Q3{内存限制}
    Q1 -->|大规模<br/>&gt;1亿| Q4{内存限制}

    Q2 -->|高精度| HNSW[HNSW<br/>内存索引<br/>高精度]
    Q2 -->|中精度| IVF_PQ1[IVF+PQ<br/>内存索引<br/>中精度]
    Q2 -->|低精度| PQ[PQ-only<br/>纯量化<br/>低内存]

    Q3 -->|内存充足| HNSW2[HNSW<br/>内存索引]
    Q3 -->|内存受限| IVF_PQ2[IVF+PQ<br/>量化压缩]

    Q4 -->|内存充足| HNSW3[HNSW<br/>需大内存]
    Q4 -->|内存受限| DISKANN[DiskANN<br/>SSD 友好<br/>磁盘索引]

    HNSW --> END([推荐索引])
    IVF_PQ1 --> END
    PQ --> END
    HNSW2 --> END
    IVF_PQ2 --> END
    HNSW3 --> END
    DISKANN --> END

    style START fill:#e7f5ff,stroke:#1971c7
    style END fill:#d3f9d8,stroke:#2f9e44
    style HNSW fill:#d0bfff,stroke:#7048e8
    style HNSW2 fill:#d0bfff,stroke:#7048e8
    style HNSW3 fill:#d0bfff,stroke:#7048e8
    style IVF_PQ1 fill:#fff9db,stroke:#f59f00
    style IVF_PQ2 fill:#fff9db,stroke:#f59f00
    style PQ fill:#fff3bf,stroke:#e67700
    style DISKANN fill:#ffccc7,stroke:#c92a2a
