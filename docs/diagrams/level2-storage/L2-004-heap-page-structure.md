%% Heap Page 结构图 - L2-004
%% 页面内部布局和各区域用途

flowchart LR
    subgraph HeapPage["Heap Page 结构 (8KB)"]
        direction TB
        HEADER["PageHeader<br/>├─ pd_lsn: 日志序列号<br/>├─ pd_checksum: 校验和<br/>├─ pd_lower: 空闲空间起点<br/>└─ pd_upper: 空闲空间终点"]

        ITEMID["ItemIdData[]<br/>行指针数组<br/>├─ offset: 元组偏移<br/>├─ length: 元组长度<br/>└─ flags: 标志位<br/>← 从后向前增长"]

        FREE["Free Space<br/>空闲空间<br/>← 从两端向中间增长"]

        TUPLE["Tuple Data<br/>实际元组数据<br/>← 从前向后增长"]
    end

    HEADER --> ITEMID
    ITEMID --> FREE
    FREE --> TUPLE

    style HEADER fill:#d0bfff,stroke:#7048e8
    style ITEMID fill:#e7f5ff,stroke:#1971c7
    style FREE fill:#fff9db,stroke:#f59f00
    style TUPLE fill:#d3f9d8,stroke:#2f9e44
