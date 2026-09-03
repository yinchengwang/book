%% 项目全景地图 - L0-001
%% 项目三层架构：基础层、索引层、存储引擎层

flowchart TB
    subgraph 项目["📚 C/C++ 算法与数据结构项目"]
        subgraph 基础层["🔧 基础层"]
            SM[self_made<br/>链表/队列/栈/树/堆/排序/字符串]
            ALGO[algo<br/>通用算法库]
            LC[leetcode<br/>面试题解]
            INT[interview<br/>面试题]
        end

        subgraph 索引层["🔍 索引层"]
            VEC[index/vector_index<br/>HNSW/IVF/DiskANN/BM25]
            HASH[index/hash<br/>CCEH/Bloom/Cuckoo]
            TREE[index/tree<br/>BTree/B+Tree]
            BLOCK[index/block<br/>BRIN]
        end

        subgraph 存储层["💾 存储引擎层"]
            CATALOG[db/catalog<br/>系统表管理]
            BUF[db/bufmgr<br/>Buffer Pool]
            HEAP[db/heapam<br/>Heap AM]
            BTREE[db/btreeam<br/>BTree AM]
            WAL[db/wal<br/>WAL 日志]
        end

        subgraph 分布式["🌐 Phase 9 分布式"]
            SHARD[分片路由]
            RAFT[Raft 共识]
            TXN[2PC 事务]
            COORD[节点协调]
        end
    end

    ALGO --> SM
    VEC --> ALGO
    HASH --> ALGO
    TREE --> ALGO
    BUF --> CATALOG
    HEAP --> BUF
    BTREE --> BUF
    WAL --> BUF
    SHARD --> RAFT
    TXN --> COORD
    RAFT --> COORD

    style 基础层 fill:#e7f5ff,stroke:#1971c7
    style 索引层 fill:#d0bfff,stroke:#7048e8
    style 存储层 fill:#fff9db,stroke:#f59f00
    style 分布式 fill:#ffccc7,stroke:#c92a2a
