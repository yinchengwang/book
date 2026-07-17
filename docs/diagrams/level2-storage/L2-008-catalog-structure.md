%% Catalog 系统结构图 - L2-008
%% 系统表的结构和关系

flowchart TB
    subgraph pg_class["pg_class 系统表"]
        direction TB
        PC_OID[oid: 表 OID]
        PC_NAME[relname: 表名]
        PC_KIND[relkind: 类型<br/>r=表, i=索引]
        PC_NODE[relfilenode: 物理文件]
        PC_TUPLE[reltuples: 行数估算]
        PC_SIZE[relpages: 页数估算]
    end

    subgraph pg_attribute["pg_attribute 系统表"]
        direction TB
        PA_OID[attrelid: 表 OID]
        PA_NAME[attname: 列名]
        PA_TYPE[atttypeid: 类型 OID]
        PA_NUM[attnum: 列序号]
        PA_DEF[attdefval: 默认值]
        PA_NULL[attnotnull: 非空约束]
    end

    subgraph pg_index["pg_index 系统表"]
        direction TB
        PI_OID[indrelid: 索引所属表 OID]
        PI_IDX[indindexid: 索引 OID]
        PI_COLS[indkey: 索引列]
        PI_EXPR[indpred: 条件表达式]
    end

    subgraph 缓存["系统表缓存"]
        SYS_CACHE[SysCache<br/>哈希表缓存]
        CATALOG_MEM[Catalog 内存结构<br/>表/列/索引元数据]
    end

    pg_class --> pg_attribute
    pg_class --> pg_index
    pg_attribute --> SYS_CACHE
    pg_class --> SYS_CACHE
    pg_index --> SYS_CACHE
    SYS_CACHE --> CATALOG_MEM

    style pg_class fill:#d0bfff,stroke:#7048e8
    style pg_attribute fill:#e7f5ff,stroke:#1971c7
    style pg_index fill:#fff9db,stroke:#f59f00
    style 缓存 fill:#d3f9d8,stroke:#2f9e44
