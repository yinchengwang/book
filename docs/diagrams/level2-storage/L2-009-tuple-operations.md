%% 元组操作流程图 - L2-009
%% Insert / Update / Delete 的完整流程

flowchart TD
    subgraph Insert["Insert 操作"]
        I1[获取目标页面]
        I2[检查空闲空间]
        I3[写入元组数据]
        I4[分配 ItemId]
        I5[更新 pd_lower]
        I6[标记脏页]

        I1 --> I2 --> I3 --> I4 --> I5 --> I6
    end

    subgraph Update["Update 操作"]
        U1[定位元组页面]
        U2[定位 ItemId]
        U3[写入新版本元组]
        U4[标记旧版本无效<br/>t_xmax = txn_id]
        U5[标记脏页]

        U1 --> U2 --> U3 --> U4 --> U5
    end

    subgraph Delete["Delete 操作"]
        D1[定位元组页面]
        D2[定位 ItemId]
        D3[标记删除标记<br/>t_xmax = txn_id<br/>t_cid = command_id]
        D4[标记脏页]

        D1 --> D2 --> D3 --> D4
    end

    style Insert fill:#d3f9d8,stroke:#2f9e44
    style Update fill:#fff9db,stroke:#f59f00
    style Delete fill:#ffccc7,stroke:#c92a2a
