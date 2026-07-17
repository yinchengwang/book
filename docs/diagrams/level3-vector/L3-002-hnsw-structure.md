%% HNSW 层结构图 - L3-002
%% 多层跳表的构造原理

flowchart TB
    subgraph HNSW["HNSW (Hierarchical Navigable Small World)"]
        direction TB

        subgraph Layer3["Layer 3 (顶层)"]
            L3_N1((L3-1))
        end

        subgraph Layer2["Layer 2"]
            L2_N1((L2-1))
            L2_N2((L2-2))
        end

        subgraph Layer1["Layer 1"]
            L1_N1((L1-1))
            L1_N2((L1-2))
            L1_N3((L1-3))
        end

        subgraph Layer0["Layer 0 (底层全连接)"]
            L0_N1((L0-1))
            L0_N2((L0-2))
            L0_N3((L0-3))
            L0_N4((L0-4))
            L0_N5((L0-5))
        end
    end

    L3_N1 --> L2_N1
    L2_N1 --> L1_N1
    L2_N1 --> L1_N2
    L2_N2 --> L1_N2
    L2_N2 --> L1_N3
    L1_N1 --> L0_N1
    L1_N1 --> L0_N2
    L1_N2 --> L0_N2
    L1_N2 --> L0_N3
    L1_N2 --> L0_N4
    L1_N3 --> L0_N4
    L1_N3 --> L0_N5

    Note right of Layer3: 稀疏搜索<br/>快速定位区域
    Note right of Layer0: 密集连接<br/>精确最近邻

    style Layer3 fill:#ffccc7,stroke:#c92a2a
    style Layer2 fill:#fff9db,stroke:#f59f00
    style Layer1 fill:#e7f5ff,stroke:#1971c7
    style Layer0 fill:#d3f9d8,stroke:#2f9e44
