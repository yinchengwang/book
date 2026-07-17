%% 时序引擎聚合查询图 - L3-009
%% 时间分区 + 聚合函数 + 降采样

flowchart TB
    subgraph 存储设计["时间分区设计"]
        direction TB
        BUCKET1[Bucket-2024-01<br/>1月数据]
        BUCKET2[Bucket-2024-02<br/>2月数据]
        BUCKET3[Bucket-2024-03<br/>3月数据]
    end

    subgraph 聚合函数["聚合函数"]
        direction LR
        CNT[COUNT 计数]
        SUM[SUM 求和]
        AVG[AVG 平均]
        MAX[MAX 最大]
        MIN[MIN 最小]
    end

    subgraph 查询模式["查询模式"]
        direction TB

        subgraph 原始查询["原始粒度查询"]
            OQ[SELECT time, AVG(value)<br/>GROUP BY time<br/>WHERE ts BETWEEN ...]
        end

        subgraph 降采样["降采样查询 (Downsampling)"]
            DQ[SELECT date_trunc('day', ts), AVG(value)<br/>GROUP BY date_trunc('day', ts)]
        end
    end

    subgraph 保留策略["保留策略 (Retention)"]
        RETAIN[Hot: 1个月 精确保留<br/>Warm: 6个月 低精度<br/>Cold: 永久 聚合存储]
    end

    BUCKET1 --> CNT
    BUCKET1 --> SUM
    BUCKET1 --> AVG
    BUCKET1 --> MAX
    BUCKET1 --> MIN

    style 存储设计 fill:#e7f5ff,stroke:#1971c7
    style 聚合函数 fill:#d0bfff,stroke:#7048e8
    style 查询模式 fill:#fff9db,stroke:#f59f00
    style 保留策略 fill:#d3f9d8,stroke:#2f9e44
