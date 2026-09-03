%% 节点协调器架构图 - L1-009
%% 协调器的整体架构和各组件职责

flowchart TB
    subgraph Coordinator["🧭 节点协调器"]
        direction TB

        subgraph 核心组件
            NR[NodeRegistry<br/>节点注册表]
            GLS[GlobalLockService<br/>全局锁服务]
            PC[TwoPCoordinator<br/>两阶段协调器]
            HC[HealthChecker<br/>健康检查器]
        end

        subgraph 数据存储
            ND[NodeDirectory<br/>节点目录]
            LT[LockTable<br/>锁表]
            AT[ActiveTxnTable<br/>活跃事务表]
        end

        subgraph 通信层
            RPC[RPC 通信]
            HB[心跳机制]
        end
    end

    NR --> ND
    GLS --> LT
    PC --> AT
    HC --> HB
    HB --> RPC

    subgraph 外部节点
        N1[Node-A]
        N2[Node-B]
        N3[Node-C]
    end

    NR <--> N1
    NR <--> N2
    NR <--> N3
    GLS <--> N1
    GLS <--> N2
    GLS <--> N3
    PC <--> N1
    PC <--> N2
    PC <--> N3

    style Coordinator fill:#e7f5ff,stroke:#1971c7
    style 核心组件 fill:#d0bfff,stroke:#7048e8
    style 数据存储 fill:#fff9db,stroke:#f59f00
    style 通信层 fill:#d3f9d8,stroke:#2f9e44
