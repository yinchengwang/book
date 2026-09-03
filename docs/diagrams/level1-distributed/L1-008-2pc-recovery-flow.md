%% 2PC 故障恢复流程图 - L1-008
%% 协调者宕机后的恢复机制

flowchart TD
    START([协调者故障]) --> CRASH[Coordinator 宕机<br/>事务处于 PREPARING]

    CRASH --> RESTART[重启恢复]

    RESTART --> QUERY[查询活跃 PREPARED 事务<br/>从持久化日志]

    QUERY --> ASK{询问参与者<br/>事务状态}

    ASK --> PA[Node-A: COMMITTED?]
    ASK --> PB[Node-B: COMMITTED?]
    ASK --> PC[Node-C: COMMITTED?]

    PA --> COUNT[统计结果]
    PB --> COUNT
    PC --> COUNT

    COUNT --> DECIDE{决定}

    DECIDE -->|全部 COMMITTED| RESUME_COMMIT[继续 Commit]
    DECIDE -->|全部 UNKNOWN| RETRY_COMMIT[重试 Commit]
    DECIDE -->|有 ABORTED| RETRY_ABORT[重试 Rollback]

    RESUME_COMMIT --> DONE([恢复完成])
    RETRY_COMMIT --> DONE
    RETRY_ABORT --> DONE

    style START fill:#ffccc7,stroke:#c92a2a
    style DONE fill:#d3f9d8,stroke:#2f9e44
