%% K-Means 聚类流程图 - L4-009
%% 初始化 → 分配 → 更新 → 迭代

flowchart TD
    START([初始化]) --> INIT[随机选择 K 个<br/>中心点 centroid]

    INIT --> ASSIGN[分配阶段<br/>E-Step]

    ASSIGN --> DIST[计算距离<br/>每个点到 K 个中心]

    DIST --> CLOSEST[分配到最近中心<br/>形成 K 个簇]

    CLOSEST --> UPDATE[更新阶段<br/>M-Step]

    UPDATE --> RECALC[重新计算中心<br/>centroid = 均值]

    RECALC --> CONVERGE{收敛?<br/>中心点不变<br/>或达到最大迭代}

    CONVERGE -->|否| ASSIGN

    CONVERGE -->|是| DONE([聚类完成<br/>返回 K 个簇])

    style START fill:#e7f5ff,stroke:#1971c7
    style ASSIGN fill:#fff9db,stroke:#f59f00
    style UPDATE fill:#d0bfff,stroke:#7048e8
    style DONE fill:#d3f9d8,stroke:#2f9e44
