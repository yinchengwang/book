%% BTree 页面分裂流程图 - L2-005
%% 插入导致页面满时的分裂过程

flowchart TD
    START([插入键值]) --> FIND[定位目标叶子页<br/>从根向下搜索]

    FIND --> FULL{页面满?}

    FULL -->|否| INSERT[插入键值<br/>按序插入]

    FULL -->|是| SPLIT[分配新页面]

    SPLIT --> MIDDLE[找到中间键<br/>作为分裂点]

    MIDDLE --> MOVE[移动后半部分<br/>到新页面]

    MOVE --> UPDATE[更新父节点<br/>指向新页面]

    UPDATE --> PARENT_FULL{父节点满?}

    PARENT_FULL -->|是| CASCADE[级联分裂<br/>递归向上]
    PARENT_FULL -->|否| DONE

    CASCADE --> SPLIT2[父节点分裂]
    SPLIT2 --> MOVE2[移动键到新父页]
    MOVE2 --> UPDATE2[更新祖父节点]

    UPDATE2 --> DONE([插入完成])

    INSERT --> DONE
    UPDATE --> DONE

    style START fill:#e7f5ff,stroke:#1971c7
    style DONE fill:#d3f9d8,stroke:#2f9e44
    style SPLIT fill:#fff9db,stroke:#f59f00
    style CASCADE fill:#ffccc7,stroke:#c92a2a
