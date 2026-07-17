%% 核心数据结构关系图 - L1-002
%% 各核心数据结构的组成和依赖关系

flowchart TB
    subgraph 节点管理["节点管理"]
        NR[NodeRegistry<br/>节点注册表]
        NM[NodeMeta<br/>节点元数据]
        NR --> NM
    end

    subgraph 分片拓扑["分片拓扑"]
        ST[ShardTopology<br/>分片拓扑]
        SN[ShardNode<br/>分片节点]
        SI[ShardInfo<br/>分片信息]
        VV[VirtualNode<br/>虚拟节点]
        ST --> SN
        ST --> SI
        ST --> VV
    end

    subgraph 路由器["分片路由"]
        SR[ShardRouter<br/>路由器]
        SR --> ST
    end

    subgraph Raft["Raft 共识"]
        RS[RaftServer]
        RS --> role["role<br/>Follower/Candidate/Leader"]
        RS --> term["currentTerm"]
        RS --> log["log[]<br/>日志条目"]
        RS --> nextIdx["nextIndex[]"]
        RS --> matchIdx["matchIndex[]"]
    end

    subgraph 分布式事务["分布式事务"]
        DT[DistTxn<br/>分布式事务]
        TP[TxnParticipant<br/>参与者]
        DT --> TP
        TP --> NR
    end

    style 节点管理 fill:#e7f5ff,stroke:#1971c7
    style 分片拓扑 fill:#d0bfff,stroke:#7048e8
    style 路由器 fill:#fff9db,stroke:#f59f00
    style Raft fill:#ffccc7,stroke:#c92a2a
    style 分布式事务 fill:#d3f9d8,stroke:#2f9e44
