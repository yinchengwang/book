%% IVF+PQ 索引结构图 - L3-004
%% 倒排索引 + 乘积量化压缩

flowchart TB
    subgraph IVF["IVF (Inverted File Index)"]
        direction TB
        CENTROID[聚类中心点<br/>K 个 centroid]

        subgraph 倒排列表
            INV1[InvList-1<br/>向量: v1, v5, v9]
            INV2[InvList-2<br/>向量: v2, v6]
            INV3[InvList-K<br/>向量: v3, v7, v10]
        end

        CENTROID --> INV1
        CENTROID --> INV2
        CENTROID --> INV3
    end

    subgraph PQ["Product Quantization (乘积量化)"]
        direction TB

        subgraph 向量分块
            VEC[原始向量 D=128维]
            BLOCK1[Block-1<br/>32维]
            BLOCK2[Block-2<br/>32维]
            BLOCK3[Block-3<br/>32维]
            BLOCK4[Block-4<br/>32维]
        end

        subgraph 编码压缩
            CB1[码本1<br/>256个码字]
            CB2[码本2<br/>256个码字]
            CB3[码本3<br/>256个码字]
            CB4[码本4<br/>256个码字]
        end

        subgraph 压缩结果
            CODE[编码: q1, q2, q3, q4<br/>4 字节]
        end
    end

    VEC --> BLOCK1
    VEC --> BLOCK2
    VEC --> BLOCK3
    VEC --> BLOCK4

    BLOCK1 --> CB1
    BLOCK2 --> CB2
    BLOCK3 --> CB3
    BLOCK4 --> CB4

    CB1 --> CODE
    CB2 --> CODE
    CB3 --> CODE
    CB4 --> CODE

    style IVF fill:#e7f5ff,stroke:#1971c7
    style PQ fill:#fff9db,stroke:#f59f00
    style CODE fill:#d3f9d8,stroke:#2f9e44
