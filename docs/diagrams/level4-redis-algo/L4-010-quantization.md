%% 量化压缩原理图 - L4-010
%% PQ / LVQ 量化 + SIMD 加速

flowchart TB
    subgraph PQ["Product Quantization (乘积量化)"]
        direction TB

        subgraph 分块["向量分块"]
            VEC[原始向量<br/>D=128维 float32<br/>512字节]
            DIV[分 D/d 块<br/>每块 d=8维]
            BLOCK1[Block-1: 8维]
            BLOCK2[Block-2: 8维]
            BLOCK3[... 14个块]
        end

        subgraph 聚类["每块独立聚类"]
            KMEANS[K-Means<br/>K=256 聚类]
            CODEBOOK[生成码本<br/>256 × 8 float32]
        end

        subgraph 编码["编码存储"]
            ENCODE[编码: q1,q2...q16<br/>每个 q 占 1 字节]
            COMPRESS[压缩率: 1/64]
        end
    end

    subgraph SIMD["SIMD 距离计算加速"]
        SIMD_ACC[并行计算<br/>距离查表]
        SUM[累加各块距离<br/>总距离排序]
    end

    VEC --> DIV --> BLOCK1
    DIV --> BLOCK2
    DIV --> BLOCK3
    BLOCK1 --> KMEANS
    KMEANS --> CODEBOOK
    CODEBOOK --> ENCODE
    ENCODE --> SIMD_ACC
    SIMD_ACC --> SUM

    style PQ fill:#e7f5ff,stroke:#1971c7
    style 分块 fill:#d0bfff,stroke:#7048e8
    style 聚类 fill:#fff9db,stroke:#f59f00
    style 编码 fill:#ffccc7,stroke:#c92a2a
    style SIMD fill:#d3f9d8,stroke:#2f9e44
