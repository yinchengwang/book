%% Redis 持久化流程图 - L4-004
%% RDB / AOF / 混合持久化

flowchart TB
    subgraph RDB["RDB 快照"]
        direction TB
        RDB_FORK[fork 子进程]
        RDB_TRAV[遍历内存数据]
        RDB_WRITE[写入 .rdb 文件]
        RDB_SAVE[保存完成]

        RDB_FORK --> RDB_TRAV --> RDB_WRITE --> RDB_SAVE
    end

    subgraph AOF["AOF 日志"]
        direction TB
        AOF_CMD[命令追加]
        AOF_BUF[AOF 缓冲区]
        AOF_SYNC[后台刷盘<br/>appendfsync]

        AOF_CMD --> AOF_BUF --> AOF_SYNC
    end

    subgraph AOF_REWRITE["AOF 重写"]
        direction TB
        REWRITE_FORK[fork 子进程]
        REWRITE_RDB[生成 RDB 格式]
        REWRITE_INCR[合并增量 AOF]
        REWRITE_DONE[替换旧 AOF]

        REWRITE_FORK --> REWRITE_RDB --> REWRITE_INCR --> REWRITE_DONE
    end

    subgraph 混合持久化["混合持久化 (4.0+)"]
        direction TB
        HYBRID[命令 → RDB 格式<br/>+ 增量 AOF]
    end

    style RDB fill:#e7f5ff,stroke:#1971c7
    style AOF fill:#fff9db,stroke:#f59f00
    style AOF_REWRITE fill:#d0bfff,stroke:#7048e8
    style 混合持久化 fill:#d3f9d8,stroke:#2f9e44
