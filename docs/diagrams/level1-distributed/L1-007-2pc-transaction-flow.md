%% 2PC 事务流程图 - L1-007
%% Prepare 和 Commit 两个阶段的完整流程

flowchart TD
    START([开始事务]) --> ACTIVE[ACTIVE<br/>活跃状态]

    ACTIVE --> PREPARE{协调者发送<br/>Prepare}

    PREPARE --> NA[Node-A<br/>获取锁]
    PREPARE --> NB[Node-B<br/>获取锁]
    PREPARE --> NC[Node-C<br/>获取锁]

    NA --> VOTE_A{投票}
    NB --> VOTE_B{投票}
    NC --> VOTE_C{投票}

    VOTE_A -->|YES| WAIT_A[等待]
    VOTE_B -->|YES| WAIT_B[等待]
    VOTE_C -->|YES| WAIT_C[等待]

    VOTE_A -->|NO| ABORT[全局回滚]
    VOTE_B -->|NO| ABORT
    VOTE_C -->|NO| ABORT

    WAIT_A --> COLLECT{收集全部投票}
    WAIT_B --> COLLECT
    WAIT_C --> COLLECT

    COLLECT -->|全部 YES| COMMIT[发送 Commit]
    COLLECT -->|有 NO| ABORT2[发送 Rollback]

    COMMIT --> CA[Node-A 提交<br/>释放锁]
    COMMIT --> CB[Node-B 提交<br/>释放锁]
    COMMIT --> CC[Node-C 提交<br/>释放锁]

    CA --> END([事务完成])
    CB --> END
    CC --> END

    style START fill:#d3f9d8,stroke:#2f9e44
    style END fill:#d3f9d8,stroke:#2f9e44
    style COMMIT fill:#e7f5ff,stroke:#1971c7
    style ABORT fill:#ffccc7,stroke:#c92a2a
