%% 存储引擎全景图 - L2-001
%% 从 SQL 执行到磁盘文件的完整层次

flowchart TB
    subgraph SQL层["📝 SQL 执行器层"]
        PARSER[Parser<br/>SQL → AST]
        ANALYZER[Analyzer<br/>AST → 查询树]
        OPTIMIZER[Optimizer<br/>查询树 → 执行计划]
        EXECUTOR[Executor<br/>执行计划 → 结果]
    end

    subgraph AM层["🔍 Access Method 层"]
        HEAP[Heap AM<br/>堆表扫描]
        BTREE_AM[BTree AM<br/>索引扫描]
        GIST[GiST<br/>空间索引]
        GIN[GIN<br/>全文索引]
    end

    subgraph Buffer层["💾 Buffer Pool 层"]
        HASH[Hash 表<br/>页面查找]
        CLOCK[Clock-Sweep<br/>页面置换]
        BUF_DESC[Buffer 描述符]
    end

    subgraph Catalog层["📋 Catalog 层"]
        PG_CLASS[pg_class<br/>表/索引元数据]
        PG_ATTR[pg_attribute<br/>列定义]
        PG_INDEX[pg_index<br/>索引信息]
    end

    subgraph WAL层["📝 WAL 层"]
        REDO[Redo Log<br/>日志记录]
        CHECKPOINT[Checkpoint<br/>检查点]
        BUF_COORD[Buffer 协调]
    end

    subgraph Disk层["💿 磁盘文件层"]
        PAGE[page.c<br/>页面管理]
        DISK[disk.c<br/>磁盘读写]
        TABLESPACE[tablespace<br/>表空间]
    end

    SQL层 --> AM层
    AM层 --> Buffer层
    Buffer层 --> Catalog层
    Buffer层 --> WAL层
    WAL层 --> Disk层
    Buffer层 --> Disk层

    style SQL层 fill:#d0bfff,stroke:#7048e8
    style AM层 fill:#e7f5ff,stroke:#1971c7
    style Buffer层 fill:#fff9db,stroke:#f59f00
    style Catalog层 fill:#d3f9d8,stroke:#2f9e44
    style WAL层 fill:#ffccc7,stroke:#c92a2a
    style Disk层 fill:#f8f0fc,stroke:#9c36b5
