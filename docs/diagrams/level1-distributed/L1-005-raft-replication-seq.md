%% Raft 日志复制时序图 - L1-005
%% Leader 处理写请求的完整流程

sequenceDiagram
    participant C as 客户端
    participant L as Leader
    participant F1 as Follower-1
    participant F2 as Follower-2

    C->>L: 写请求 write(key, value)
    L->>L: 创建日志条目<br/>本地写入

    par 并行复制
        L->>F1: AppendEntries<br/>新日志条目
        L->>F2: AppendEntries<br/>新日志条目
    end

    F1-->>L: ACK
    F2-->>L: ACK

    alt 多数派确认
        L->>L: 提交日志<br/>应用到状态机
        L-->>C: 写成功响应
    else 未获多数
        L-->>C: 写失败
    end
