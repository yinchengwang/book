%% WAL 日志格式图 - L2-006
%% WAL Record 内部结构和写入顺序

flowchart LR
    subgraph WALRecord["WAL Record 结构"]
        direction TB
        LSN["LSN<br/>日志序列号<br/>8字节"]

        TXN["XID<br/>事务 ID<br/>4字节"]

        PREV["Prev LSN<br/>上一条日志指针<br/>8字节"]

        LEN["Length<br/>记录长度<br/>4字节"]

        TYPE["Type<br/>操作类型<br/>HeapInsert/Update/Delete<br/>4字节"]

        PAGEID["Page ID<br/>页面 ID<br/>8字节"]

        DATA["Data<br/>页面变更数据<br/>变长"]
    end

    subgraph 写入顺序
        direction LR
        W1["1. 写入 LSN"]
        W2["2. 写入 prev_lsn"]
        W3["3. 写入 txn_id"]
        W4["4. 写入 type"]
        W5["5. 写入 page_id + data"]
    end

    LSN --> PREV --> TXN --> TYPE --> PAGEID --> DATA

    style LSN fill:#e7f5ff,stroke:#1971c7
    style PREV fill:#e7f5ff,stroke:#1971c7
    style TXN fill:#d0bfff,stroke:#7048e8
    style TYPE fill:#fff9db,stroke:#f59f00
    style PAGEID fill:#d3f9d8,stroke:#2f9e44
    style DATA fill:#ffccc7,stroke:#c92a2a
