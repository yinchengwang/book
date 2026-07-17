%% HNSW 搜索流程图 - L3-003
%% 从顶层到底层的搜索路径

flowchart TD
    START([查询向量]) --> TOP[从顶层 Layer Max 开始]

    TOP --> SEARCH[在当前层搜索<br/>贪婪遍历找最近邻]

    SEARCH --> LAYER_END{到达当前层末端?}

    LAYER_END -->|否| SEARCH

    LAYER_END -->|是| DOWN[跳转到下一层<br/>从当前最近邻开始]

    DOWN --> LAST{最后一层?}

    LAST -->|否| CONTINUE[继续搜索]

    CONTINUE --> SEARCH

    LAST -->|是| RESULT[返回最近邻结果]

    style START fill:#e7f5ff,stroke:#1971c7
    style SEARCH fill:#fff9db,stroke:#f59f00
    style DOWN fill:#d0bfff,stroke:#7048e8
    style RESULT fill:#d3f9d8,stroke:#2f9e44
