# S22 Spec —— Phase10 Raft 学习层重写

## 1. 实施策略

学习层副本 4 个 .c 重写为**最小可编译版本**——函数体保持占位（return 0、NULL 等），签名匹配 .h。

## 2. 不做

- ❌ 实现真实 Raft 共识算法（仅参考代码）
- ❌ 改公共 .h
- ❌ 改工程层 src/db/core/raft.c（生产实现）
- ❌ 加 ctest
