%% 模块依赖关系图 - L0-002
%% 展示 src/ 与 include/ 的编译依赖关系

flowchart TB
    subgraph include["include/ 头文件"]
        direction TB
        INC_SM[include/self_made/]
        INC_ALGO[include/algo/]
        INC_INDEX[include/index/]
        INC_DB[include/db/]
    end

    subgraph src["src/ 源码"]
        direction TB
        SRC_SM[self_made/]
        SRC_ALGO[algo/]
        SRC_INDEX[index/]
        SRC_DB[db/storage/]
        SRC_SQL[db/sql/]
    end

    subgraph test["test/ 测试"]
        direction TB
        TEST_SM[测试 self_made]
        TEST_LC[测试 leetcode]
    end

    subgraph main["主二进制"]
        direction TB
        MAIN[AlgorithmPractice]
    end

    MAIN --> SRC_SM
    MAIN --> SRC_ALGO
    MAIN --> SRC_LC
    SRC_SM --> INC_SM
    SRC_ALGO --> INC_ALGO
    SRC_ALGO --> INC_SM
    SRC_INDEX --> INC_INDEX
    SRC_INDEX --> INC_ALGO
    SRC_DB --> INC_DB
    SRC_SQL --> INC_DB
    SRC_SQL --> SRC_DB
    TEST_SM --> SRC_SM
    TEST_SM --> INC_SM
    TEST_LC --> SRC_LC

    style include fill:#e7f5ff,stroke:#1971c7
    style src fill:#d0bfff,stroke:#7048e8
    style test fill:#fff9db,stroke:#f59f00
    style main fill:#d3f9d8,stroke:#2f9e44
