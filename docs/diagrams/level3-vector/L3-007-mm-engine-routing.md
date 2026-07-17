%% 多模态引擎路由图 - L3-007
%% 根据数据类型选择对应引擎

flowchart TD
    START([查询请求]) --> PARSE[解析请求<br/>识别数据类型]

    PARSE --> ROUTE{数据类型?}

    ROUTE -->|Key-Value| KV[KV 引擎<br/>kv_engine]
    ROUTE -->|向量| VEC[Vector 引擎<br/>vector_engine]
    ROUTE -->|时序数据| TS[Timeseries 引擎<br/>ts_engine]
    ROUTE -->|文档/JSON| DOC[Document 引擎<br/>doc_engine]
    ROUTE -->|空间数据| SPATIAL[Spatial 引擎<br/>spatial_engine]
    ROUTE -->|图数据| GRAPH[Graph 引擎<br/>graph_engine]

    KV --> RESULT1[返回 KV 结果]
    VEC --> RESULT2[返回向量结果]
    TS --> RESULT3[返回时序结果]
    DOC --> RESULT4[返回文档结果]
    SPATIAL --> RESULT5[返回空间结果]
    GRAPH --> RESULT6[返回图结果]

    style START fill:#e7f5ff,stroke:#1971c7
    style KV fill:#ffccc7,stroke:#c92a2a
    style VEC fill:#d0bfff,stroke:#7048e8
    style TS fill:#fff9db,stroke:#f59f00
    style DOC fill:#d3f9d8,stroke:#2f9e44
    style SPATIAL fill:#e7f5ff,stroke:#1971c7
    style GRAPH fill:#f8f0fc,stroke:#9c36b5
