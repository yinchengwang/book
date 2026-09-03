%% Raft 状态机图 - L1-004
%% Follower / Candidate / Leader 三状态转换

stateDiagram-v2
    [*] --> Follower: 启动

    Follower --> Candidate: 选举超时
    Follower --> Follower: 收到 Leader 心跳

    Candidate --> Leader: 赢得多数票
    Candidate --> Follower: 收到更高任期 Leader
    Candidate --> Candidate: 选举超时重试

    Leader --> Follower: 收到更高任期请求

    Leader --> Leader: 发送心跳维持领导

    note right of Follower: 等待心跳<br/>维护 term
    note right of Candidate: 发起投票<br/>等待响应
    note right of Leader: 处理写请求<br/>复制日志
