%% KMP 匹配流程图 - L4-008
%% 部分匹配表 + 匹配跳转

flowchart TB
    subgraph PMT["PMT (Partial Match Table)"]
        direction TB

        subgraph 构建["PMT 构建过程"]
            B1[初始化<br/>pi[0] = 0]
            B2[递推计算<br/>pi[i] = pi[k] + 1]
            B3[回溯检查<br/>while s[i] != s[pi[k]]
        end
    end

    subgraph 匹配["KMP 匹配过程"]
        direction TB

        subgraph 主串匹配
            M1[遍历主串 S]
            M2[比较 S[i] vs P[j]]
            M3[匹配成功<br/>j++ 继续]
            M4[匹配失败<br/>j = pi[j-1]]
        end
    end

    subgraph 对比["vs 暴力匹配"]
        BRUTE[暴力: 回退 i<br/>O(mn)]
        KMP[KMP: 仅回退 j<br/>O(m+n)]
    end

    构建 --> 匹配
    BRUTE -.-> 对比
    KMP -.-> 对比

    style PMT fill:#e7f5ff,stroke:#1971c7
    style 构建 fill:#d0bfff,stroke:#7048e8
    style 匹配 fill:#fff9db,stroke:#f59f00
    style 对比 fill:#d3f9d8,stroke:#2f9e44
