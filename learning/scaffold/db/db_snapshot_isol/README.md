# 快照隔离（Snapshot Isolation）学习卡

## 概述

本卡片演示快照隔离（Snapshot Isolation）与 Read Committed 的区别：
- SI: 事务全程使用同一快照
- RC: 每次语句重新计算快照
- First-Writer-Wins 冲突处理
- Write-Skew 异常场景

## 编译与运行

```bash
# 编译
gcc -std=c11 -Wall -o snapshot_isol main.c

# 运行
./snapshot_isol
```

## 学习要点

1. **SI vs RC**：SI 事务使用一致的快照，RC 每次语句刷新
2. **First-Writer-Wins**：检测并发写冲突，先提交者获胜
3. **Write-Skew**：两个事务基于快照读取后各自更新不同行，可能违反约束
4. **Serializable**：最高隔离级别，可避免 Write-Skew

## 工程对照

见 [NOTES.md](NOTES.md) 中工程轨 `mvcc_snapshot.c` 快照隔离实现对照。