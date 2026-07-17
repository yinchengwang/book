%% 分布式模块全景图 - L1-001
%% 从应用层到存储层的完整层次

flowchart TB
    subgraph 应用层["📱 应用层"]
        SQL_QUERY[SQL 查询]
        CROSS_SHARD[跨分片查询]
        DIST_TXN[分布式事务]
    end

    subgraph 协调层["🎯 协调层"]
        REGISTRY[节点注册发现]
        LOCK[全局锁服务]
        ELECTION[领导者选举]
        CONFIG[配置管理]
    end

    subgraph 高可用层["🛡️ 高可用层"]
        RAFT[Raft 共识]
        REPL[日志复制]
        MEMBERSHIP[成员变更]
        DETECT[故障检测]
    end

    subgraph 事务层["📋 事务层"]
        PC[2PC 两阶段提交]
        SAGA[SAGA 补偿事务]
        TSO[时间戳序]
        MVCC[多版本并发]
    end

    subgraph 分片层["🔢 分片层"]
        HASH_SHARD[Hash 分片]
        RANGE_SHARD[Range 分片]
        CONSISTENT[一致性 Hash]
        RESHARD[动态扩缩容]
    end

    subgraph 存储层["💾 存储层"]
        BUF[Buffer Pool]
        HEAP[Heap AM]
        BTREE[BTree AM]
        WAL[WAL]
    end

    应用层 --> 协调层
    协调层 --> 高可用层
    高可用层 --> 事务层
    事务层 --> 分片层
    分片层 --> 存储层

    style 应用层 fill:#d0bfff,stroke:#7048e8
    style 协调层 fill:#e7f5ff,stroke:#1971c7
    style 高可用层 fill:#fff9db,stroke:#f59f00
    style 事务层 fill:#ffccc7,stroke:#c92a2a
    style 分片层 fill:#d3f9d8,stroke:#2f9e44
    style 存储层 fill:#f8f0fc,stroke:#9c36b5
