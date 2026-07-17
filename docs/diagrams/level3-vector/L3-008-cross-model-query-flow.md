%% 跨模型联合查询流程图 - L3-008
%% 向量相似 + 条件过滤 + 空间查询

flowchart TB
    subgraph 规划器["查询规划器"]
        PARSE[解析 SQL<br/>SELECT * FROM docs<br/>WHERE embed &lt;-&gt; ? &lt; 0.5<br/>AND spatial ST_Within]
        DECOMPOSE[分解查询<br/>子查询拆分]
    end

    subgraph 子查询["并行子查询"]
        VQ[向量子查询<br/>embbeding &lt;-&gt; ? &lt; 0.5<br/>→ Top-k 向量 ID]
        FQ[过滤子查询<br/>category = 'tech'<br/>→ 过滤后 ID 列表]
        SQ[空间子查询<br/>ST_Within(point, bbox)<br/>→ 空间 ID 列表]
    end

    subgraph 合并["结果合并"]
        INTERSECT[交集合并<br/>ID ∩ ID ∩ ID]
        RERANK[Rerank 重排<br/>综合评分]
        RETURN[返回结果]
    end

    PARSE --> DECOMPOSE
    DECOMPOSE --> VQ
    DECOMPOSE --> FQ
    DECOMPOSE --> SQ

    VQ --> INTERSECT
    FQ --> INTERSECT
    SQ --> INTERSECT

    INTERSECT --> RERANK
    RERANK --> RETURN

    style 规划器 fill:#d0bfff,stroke:#7048e8
    style 子查询 fill:#e7f5ff,stroke:#1971c7
    style 合并 fill:#fff9db,stroke:#f59f00
