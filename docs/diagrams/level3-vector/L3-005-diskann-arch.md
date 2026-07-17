%% DiskANN 架构图 - L3-005
%% Vamana 图 + SSD 友好设计

flowchart TB
    subgraph DiskANN["DiskANN 架构"]
        direction TB

        subgraph 图索引["Vamana 图索引"]
            VAMANA[构建 Vamana 图<br/>优化磁盘 I/O]
            BEAM[Beam Search<br/>并行探测]
            PQ_ENCODE[PQ 编码<br/>向量压缩]
        end

        subgraph 存储层["存储优化"]
            SSD[SSD 存储<br/>顺序读优化]
            SORTED[顶点排序<br/>按度数排序]
            PAGES[固定页大小<br/>内存页映射]
        end

        subgraph 查询流程["查询流程"]
            ROUTE[路由定位<br/>起始点选择]
            BEAM_SEARCH[Beam Search<br/>k 近邻搜索]
            PQ_DIST[PQ 距离计算<br/>加速比较]
            RANK[结果排序<br/>返回 Top-k]
        end
    end

    PQ_ENCODE --> SSD
    VAMANA --> SSD
    SSD --> BEAM
    BEAM --> BEAM_SEARCH
    BEAM_SEARCH --> PQ_DIST
    PQ_DIST --> RANK

    Note right of DiskANN: vs HNSW:<br/>HNSW 全内存<br/>DiskANN 磁盘友好

    style 图索引 fill:#d0bfff,stroke:#7048e8
    style 存储层 fill:#e7f5ff,stroke:#1971c7
    style 查询流程 fill:#fff9db,stroke:#f59f00
