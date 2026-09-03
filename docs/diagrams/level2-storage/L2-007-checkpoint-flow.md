%% 检查点流程图 - L2-007
%% 检查点的完整执行过程

flowchart TD
    START([检查点触发]) --> COND{触发条件}

    COND -->|时间间隔| CHECK_TIME[检查时间间隔]
    COND -->|事务数| CHECK_TXN[检查事务数]
    COND -->|WAL 段数| CHECK_WAL[检查 WAL 段数]

    CHECK_TIME --> TRAVERSE[遍历所有脏页]
    CHECK_TXN --> TRAVERSE
    CHECK_WAL --> TRAVERSE

    TRAVERSE --> LOOP{还有脏页?}

    LOOP -->|是| FLUSH[刷盘脏页<br/>data = true]
    FLUSH --> NEXT[下一脏页]
    NEXT --> LOOP

    LOOP -->|否| CKPT_RECORD[写入检查点记录<br/>checkpoint lsn]

    CKPT_RECORD --> CTRL[更新控制文件<br/>记录检查点位置]

    CTRL --> DONE([检查点完成])

    style START fill:#e7f5ff,stroke:#1971c7
    style DONE fill:#d3f9d8,stroke:#2f9e44
    style FLUSH fill:#fff9db,stroke:#f59f00
