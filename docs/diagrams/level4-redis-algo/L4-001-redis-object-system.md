%% Redis 对象系统图 - L4-001
%% 五种数据类型与底层数据结构对应

flowchart TB
    subgraph 数据类型["Redis 数据类型"]
        direction TB
        STRING[String]
        LIST[List]
        HASH[Hash]
        SET[Set]
        ZSET[Sorted Set]
    end

    subgraph 底层结构["底层数据结构"]
        direction TB
        SDS[SDS<br/>简单动态字符串]
        LINKED[LinkedList<br/>双向链表]
        ZIPLIST[ZipList<br/>压缩列表]
        HT[HashTable<br/>哈希表]
        INTSET[IntSet<br/>整数集合]
        SKIP[SkipList<br/>跳表 + 字典]
        QUICKLIST[QuickList<br/>链表 + 压缩列表]
    end

    subgraph 编码转换["编码转换规则"]
        direction TB
        ZSET_ENC[ZSet: 元素少→ZipList<br/>元素多→SkipList]
        LIST_ENC[List: 元素少→ZipList<br/>元素多→QuickList]
        HASH_ENC[Hash: 字段少→ZipList<br/>字段多→HashTable]
    end

    STRING --> SDS
    LIST --> QUICKLIST
    LIST --> LINKED
    HASH --> HT
    HASH --> ZIPLIST
    SET --> HT
    SET --> INTSET
    ZSET --> SKIP
    ZSET --> ZIPLIST

    QUICKLIST --> ZIPLIST
    QUICKLIST --> LINKED

    style 数据类型 fill:#d0bfff,stroke:#7048e8
    style 底层结构 fill:#e7f5ff,stroke:#1971c7
    style 编码转换 fill:#fff9db,stroke:#f59f00
