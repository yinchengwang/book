%% Raft 领导者选举时序图 - L1-006
%% 从选举超时到新 Leader 产生的完整过程

sequenceDiagram
    participant F1 as Follower-1
    participant F2 as Follower-2
    participant F3 as Follower-3
    participant C as Candidate

    Note over F1,F3: 选举超时触发

    F1->>F1: 选举超时
    F1->>F1: currentTerm++<br/>转为 Candidate
    F1->>F1: 投票给自己

    F1->>F2: RequestVote(term=2, lastLogIndex=10)
    F1->>F3: RequestVote(term=2, lastLogIndex=10)

    F2-->>F1: VoteGranted(term=2)
    F3-->>F1: VoteGranted(term=2)

    alt 获得多数票
        F1->>F1: 成为 Leader
        loop 维持领导
            F1->>F2: 心跳 AppendEntries
            F1->>F3: 心跳 AppendEntries
        end
    end
