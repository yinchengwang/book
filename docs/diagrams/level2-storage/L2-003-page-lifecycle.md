%% 页面生命周期图 - L2-003
%% 页面从磁盘到内存再到磁盘的完整流转

flowchart LR
    subgraph 状态流转
        DISK[(磁盘)]
        BUF[(Buffer)]
        DIRTY[(脏页)]
        FREE[(空闲)]
    end

    DISK -->|读取| BUF
    BUF -->|修改| DIRTY
    DIRTY -->|刷盘| DISK
    BUF -->|Unpin refcount=0| FREE
    FREE -->|重用| BUF

    subgraph 操作
        PIN[Pin 加锁]
        UNPIN[Unpin 解锁]
        MODIFY[修改数据]
        FLUSH[刷盘]
    end

    BUF --> PIN
    PIN --> MODIFY
    MODIFY --> DIRTY
    DIRTY --> UNPIN
    BUF --> UNPIN
    UNPIN --> FLUSH

    style DISK fill:#f8f0fc,stroke:#9c36b5
    style BUF fill:#e7f5ff,stroke:#1971c7
    style DIRTY fill:#fff9db,stroke:#f59f00
    style FREE fill:#d3f9d8,stroke:#2f9e44
