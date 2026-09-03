%% 算法分类全景图 - L4-005
%% 本项目算法覆盖范围

flowchart TB
    subgraph 基础算法["基础算法"]
        direction TB
        SORT[排序<br/>冒泡/插入/选择<br/>归并/快速/堆]
        SEARCH[二分查找<br/>标准/左手边/右手边]
        TWO_PTR[双指针<br/>快慢/左右/滑动窗口]
        MONO[单调栈<br/>Next Greater Element]
    end

    subgraph 高级算法["高级算法"]
        direction TB
        DP[动态规划<br/>背包/股票/打家劫舍]
        BACKTRACE[回溯<br/>全排列/组合/子集]
        DIVIDE[分治<br/>归并排序/快速幂]
        GREEDY[贪心<br/>区间调度/哈夫曼]
        KMP[KMP<br/>字符串匹配]
    end

    subgraph 专用算法["专用算法"]
        direction TB
        KMEANS[K-Means<br/>聚类分析]
        QUANTIZE[量化<br/>PQ/LVQ 压缩]
        DIST[距离计算<br/>欧氏/余弦/汉明<br/>SIMD 加速]
        SEG[分词词典<br/>Trie/AC自动机]
    end

    基础算法 --> 高级算法
    高级算法 --> 专用算法

    style 基础算法 fill:#e7f5ff,stroke:#1971c7
    style 高级算法 fill:#fff9db,stroke:#f59f00
    style 专用算法 fill:#d3f9d8,stroke:#2f9e44
