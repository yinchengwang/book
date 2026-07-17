%% 数据结构选择决策树 - L4-007
%% 根据需求快速定位合适数据结构

flowchart TD
    START([需求]) --> Q1{核心操作?}

    Q1 -->|快速查找| HASH[哈希表<br/>O(1) 查找]
    Q1 -->|有序遍历| ORDER[需要有序?]
    Q1 -->|快速增删| LINK[链表<br/>O(1) 增删]
    Q1 -->|范围统计| RANGE[范围查询?]

    ORDER -->|是| BALANCED[平衡树<br/>AVL/红黑树/跳表]
    ORDER -->|否| SET1[Set/HashSet<br/>无序去重]

    LINK --> LINKED[单向/双向链表]
    LINK --> DEQUE[Deque<br/>两端操作]

    RANGE -->|是| SEG[线段树<br/>树状数组<br/>O(log n) 范围]
    RANGE -->|否| HASH2[哈希表<br/>O(1) 单点]

    HASH --> END([数据结构])
    BALANCED --> END
    SET1 --> END
    LINKED --> END
    DEQUE --> END
    SEG --> END
    HASH2 --> END

    style START fill:#e7f5ff,stroke:#1971c7
    style END fill:#d3f9d8,stroke:#2f9e44
    style HASH fill:#d0bfff,stroke:#7048e8
    style BALANCED fill:#fff9db,stroke:#f59f00
