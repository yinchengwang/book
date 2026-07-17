%% Buffer Pool 置换流程图 - L2-002
%% Hash 表查找 + Clock-Sweep 置换

flowchart TD
    START([页面请求]) --> LOOKUP[Hash 表查找<br/>page_id → buffer]

    LOOKUP --> HIT{命中?}

    HIT -->|是| PIN[Pin 页面<br/>refcount++]
    PIN --> RETURN[返回页面引用]

    HIT -->|否| FIND[Clock-Sweep<br/>寻找可置换页面]

    FIND --> CHECK{页面被占用?<br/>refcount > 0}

    CHECK -->|是| SKIP[跳过页面<br/>refcount--, 继续扫描]
    SKIP --> FIND

    CHECK -->|否| DIRTY{脏页?}

    DIRTY -->|是| FLUSH[刷盘<br/>写回磁盘]
    DIRTY -->|否| READ

    FLUSH --> READ[读取新页面<br/>到缓冲区]

    READ --> ALLOC[分配 Buffer<br/>加载页面]

    ALLOC --> INSERT[插入 Hash 表]

    INSERT --> PIN2[Pin 页面<br/>refcount++]

    PIN2 --> RETURN

    style START fill:#e7f5ff,stroke:#1971c7
    style RETURN fill:#d3f9d8,stroke:#2f9e44
    style FLUSH fill:#fff9db,stroke:#f59f00
