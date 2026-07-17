%% 分片路由流程图 - L1-003
%% MurmurHash3 分片键计算与路由定位

flowchart TD
    START([客户端请求]) --> PARSE[解析分片键<br/>user_id = 'u123']

    PARSE --> HASH[计算 MurmurHash3<br/>Hash(user_id)]

    HASH --> MOD[取模运算<br/>hash % vnode_count]

    MOD --> LOCATE[定位虚拟节点<br/>vnode = ring[slot]]

    LOCATE --> PRIMARY[获取主副本节点<br/>primary_node]

    PRIMARY --> WRITE[写入主副本<br/>同步等待 ACK]

    WRITE --> REPL[异步复制到从副本<br/>后台同步]

    REPL --> SUCCESS[返回成功<br/>客户端响应]

    style START fill:#d3f9d8,stroke:#2f9e44
    style SUCCESS fill:#d3f9d8,stroke:#2f9e44
    style HASH fill:#e7f5ff,stroke:#1971c7
    style PRIMARY fill:#fff9db,stroke:#f59f00
