%% 学习路线图 - L0-003
%% Phase 1-5 → Phase 6-8 → Phase 9 渐进式学习路径

flowchart LR
    subgraph Phase1["Phase 1-5: 基础阶段"]
        direction TB
        P1_1[基础数据结构<br/>链表/队列/栈/树]
        P1_2[基础算法<br/>排序/二分/字符串]
        P1_3[LeetCode 刷题<br/>90+ 题解]
        P1_4[面试题准备<br/>C/C++/数据库]
    end

    subgraph Phase2["Phase 6-8: 高级特性"]
        direction TB
        P2_1[向量索引<br/>HNSW/IVF/DiskANN]
        P2_2[多模态存储<br/>KV/时序/空间/图]
        P2_3[存储引擎<br/>BufferPool/WAL/BTree]
        P2_4[高级算法<br/>K-Means/PQ量化]
    end

    subgraph Phase3["Phase 9: 分布式演进"]
        direction TB
        P3_1[分片与路由<br/>Hash/一致性Hash]
        P3_2[Raft 共识<br/>领导者选举/日志复制]
        P3_3[分布式事务<br/>2PC/故障恢复]
        P3_4[节点协调<br/>注册/健康检查/全局锁]
    end

    Phase1 --> Phase2 --> Phase3

    style Phase1 fill:#e7f5ff,stroke:#1971c7
    style Phase2 fill:#fff9db,stroke:#f59f00
    style Phase3 fill:#ffccc7,stroke:#c92a2a
