%% 排序算法对比表 - L4-006
%% 各排序算法的复杂度、稳定性对比

flowchart LR
    subgraph 算法["排序算法"]
        BUBBLE[冒泡排序]
        INSERT[插入排序]
        SELECT[选择排序]
        MERGE[归并排序]
        QUICK[快速排序]
        HEAP[堆排序]
        COUNT[计数排序]
        BUCKET[桶排序]
        RADIX[基数排序]
    end

    subgraph 复杂度["时间/空间复杂度"]
        COMPLEX[最好: O(n)<br/>平均: O(n²)<br/>最坏: O(n²)<br/>空间: O(1)]
    end

    subgraph 稳定性["稳定性"]
        STABLE[稳定?]
    end

    subgraph 特点["特点"]
        FEATURE[适用场景]
    end

    算法 --> 复杂度
    算法 --> 稳定性
    算法 --> 特点

    BUBBLE --> COMPLEX
    INSERT --> COMPLEX
    SELECT --> COMPLEX
    MERGE --> COMPLEX
    QUICK --> COMPLEX
    HEAP --> COMPLEX
    COUNT --> COMPLEX
    BUCKET --> COMPLEX
    RADIX --> COMPLEX

    style 算法 fill:#d0bfff,stroke:#7048e8
    style 复杂度 fill:#e7f5ff,stroke:#1971c7
    style 稳定性 fill:#fff9db,stroke:#f59f00
    style 特点 fill:#d3f9d8,stroke:#2f9e44
