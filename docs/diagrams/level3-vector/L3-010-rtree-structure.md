%% R-Tree 空间索引结构图 - L3-010
%% 最小边界矩形 + 节点分裂 + 空间查询

flowchart TB
    subgraph 结构["R-Tree 树形结构"]
        direction TB

        subgraph 根节点["Root"]
            R_MBR[MBR: (0,0)-(100,100)]
        end

        subgraph 中间节点["Level 1"]
            N1_MBR[MBR: (0,0)-(50,60)]
            N2_MBR[MBR: (50,0)-(100,100)]
        end

        subgraph 叶子节点["Level 2 (Leaf)"]
            L1_MBR[MBR: (0,0)-(25,30) R1]
            L2_MBR[MBR: (25,0)-(50,60) R2]
            L3_MBR[MBR: (50,0)-(75,50) R3]
            L4_MBR[MBR: (75,0)-(100,100) R4]
        end

        R_MBR --> N1_MBR
        R_MBR --> N2_MBR
        N1_MBR --> L1_MBR
        N1_MBR --> L2_MBR
        N2_MBR --> L3_MBR
        N2_MBR --> L4_MBR
    end

    subgraph 查询类型["空间查询类型"]
        direction LR
        POINT[Point Query<br/>点查询]
        RANGE[Range Query<br/>矩形查询]
        KNN[KNN Query<br/>K近邻查询]
    end

    style 结构 fill:#e7f5ff,stroke:#1971c7
    style 叶子节点 fill:#d3f9d8,stroke:#2f9e44
    style 查询类型 fill:#fff9db,stroke:#f59f00
