%% SQL 执行流程图 - L2-010
%% 从 SQL 到结果的完整处理流程

flowchart LR
    subgraph Parser["📝 Parser 解析器"]
        P1["SQL 文本"]
        P2["词法分析<br/>Token 序列"]
        P3["语法分析<br/>AST 语法树"]
    end

    subgraph Analyzer["🔍 Analyzer 分析器"]
        A1["语义分析<br/>类型检查"]
        A2["查询树<br/>Query 结构"]
        A3["表/列解析<br/>元数据查找"]
    end

    subgraph Optimizer["⚡ Optimizer 优化器"]
        O1["规则优化<br/>谓词下推"]
        O2["代价估算<br/>选择索引"]
        O3["执行计划<br/>Plan 树"]
    end

    subgraph Executor["🚀 Executor 执行器"]
        E1["算子执行<br/>SeqScan/IndexScan"]
        E2["数据处理<br/>Filter/Project"]
        E3["结果返回"]
    end

    P1 --> P2 --> P3
    P3 --> A1 --> A2 --> A3
    A3 --> O1 --> O2 --> O3
    O3 --> E1 --> E2 --> E3

    style Parser fill:#d0bfff,stroke:#7048e8
    style Analyzer fill:#e7f5ff,stroke:#1971c7
    style Optimizer fill:#fff9db,stroke:#f59f00
    style Executor fill:#d3f9d8,stroke:#2f9e44
