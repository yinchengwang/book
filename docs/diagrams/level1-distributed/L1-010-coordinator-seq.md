%% 节点协调流程时序图 - L1-010
%% 节点注册、健康检查、全局锁流程

sequenceDiagram
    participant NA as Node-A
    participant NR as Registry
    participant HC as HealthChecker
    participant GL as GlobalLock
    participant PC as TwoPCoordinator

    rect rgb(200, 230, 255)
        Note over NA,NR: 节点注册流程
        NA->>NR: 注册请求(node_id, addr, capacity)
        NR-->>NA: 注册成功(regist_id)
    end

    rect rgb(255, 240, 200)
        Note over HC,NA: 健康检查流程
        loop 定期心跳
            HC->>NA: 心跳探测
            NA-->>HC: ACK + 状态信息
        end
    end

    rect rgb(220, 255, 220)
        Note over NA,GL: 全局锁流程
        NA->>GL: 请求锁(lock_id, mode=EXCLUSIVE)
        GL-->>NA: 锁授予(lock_token)
        NA->>GL: 释放锁(lock_token)
        GL-->>NA: 锁已释放
    end
