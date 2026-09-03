%% 跳表结构图 - L4-003
%% 多层链表 + O(log n) 查找

flowchart TB
    subgraph SkipList["跳表结构"]
        direction TB

        subgraph Level3["Level 3 (顶层)"]
            L3_H[HEAD]
            L3_N1((N1))
        end

        subgraph Level2["Level 2"]
            L2_H[HEAD]
            L2_N1((N1))
            L2_N2((N2))
            L2_N3((N3))
        end

        subgraph Level1["Level 1"]
            L1_H[HEAD]
            L1_N1((N1))
            L1_N2((N2))
            L1_N3((N3))
            L1_N4((N4))
            L1_N5((N5))
        end

        subgraph Level0["Level 0 (底层)"]
            L0_H[HEAD]
            L0_N1((N1))
            L0_N2((N2))
            L0_N3((N3))
            L0_N4((N4))
            L0_N5((N5))
            L0_NIL[NIL]
        end
    end

    subgraph 随机层高["随机层高生成"]
        RANDOM[随机函数<br/>50% Level+1<br/>25% Level+2<br/>12.5% Level+3]
    end

    L3_H --> L3_N1
    L3_N1 --> L2_H
    L2_H --> L2_N1
    L2_N1 --> L2_N2
    L2_N2 --> L2_N3
    L2_N3 --> L1_H
    L1_H --> L1_N1
    L1_N1 --> L1_N2
    L1_N2 --> L1_N3
    L1_N3 --> L1_N4
    L1_N4 --> L1_N5
    L1_N5 --> L0_H
    L0_H --> L0_N1 --> L0_N2 --> L0_N3 --> L0_N4 --> L0_N5 --> L0_NIL

    style Level3 fill:#ffccc7,stroke:#c92a2a
    style Level2 fill:#fff9db,stroke:#f59f00
    style Level1 fill:#e7f5ff,stroke:#1971c7
    style Level0 fill:#d3f9d8,stroke:#2f9e44
