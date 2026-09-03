%% SDS 数据结构图 - L4-002
%% 简单动态字符串结构与优势

flowchart LR
    subgraph SDS["SDS (Simple Dynamic String)"]
        direction TB

        subgraph 结构["sdshdr 结构"]
            LEN[len<br/>已用长度<br/>4字节]
            FREE[free<br/>剩余长度<br/>4字节]
            BUF[buf<br/>字符数组<br/>柔性数组]
        end

        subgraph 特性["核心特性"]
            BINARY[二进制安全<br/>不依赖 \0 结束]
            PREALLOC[空间预分配<br/>2x 增长策略]
            NO[无缓冲区溢出<br/>自动扩容]
        end
    end

    subgraph 对比["vs C 字符串"]
        C_STR[C char[]<br/>需手动管理长度<br/>不安全]
    end

    LEN --> FREE --> BUF
    BINARY -.-> SDS
    PREALLOC -.-> SDS
    NO -.-> SDS

    style SDS fill:#e7f5ff,stroke:#1971c7
    style 结构 fill:#d0bfff,stroke:#7048e8
    style 特性 fill:#fff9db,stroke:#f59f00
