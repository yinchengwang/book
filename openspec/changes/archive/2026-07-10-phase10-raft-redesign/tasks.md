# S22 — Tasks（Phase10 Raft 学习层重写）— CANCELED

> **决策取消**：S22 试图完整重写 raft.c (812 行) + dist_txn.c + shard.c 让其与 .h 对齐。**最小可编译 stub 重写**也意味着：
> - 删除 812 行现有业务逻辑（即使是占位也是 phase9 已实施）
> - 与 phase10 重构目标**方向相反**——phase10 不是写 stub，而是补真实业务
>
> **S22 决策关闭**：把"phase9 raft 业务逻辑续做"标记为后续独立大工作
> - 后续 phase10+ 应在工程层 `engineering/src/db/core/raft.c` 重写（不是学习层副本）
> - 学习层副本 `learning/.../raft.c` 仅作"参考代码"保留现状

## 1.1 调研

- [x] 1.1.1 已查：raft.c (812 行) / dist_txn.c / shard.c 与 .h 不兼容
- [x] 1.1.2 决策：scope 过大，关闭

## 1.2 结论

- [x] 1.2.1 S22 关闭
- [x] 1.2.2 后续工作：phase10+（独立大 change）——在工程层 raft.c 重写业务逻辑
- [x] 1.2.3 学习层 raft.c 副本保留作 phase9 历史快照

## 1.3 学习层 .c 验证（现状）

- [x] 1.3.1 V1: `cmake -B build-learning -S learning` 配置成功
- [x] 1.3.2 V2: `cmake --build build-learning` 全部成功（coordinator.c 已编译，其他 3 个仍 EXCLUDE）
- [x] 1.3.3 V3: libdistributed-solutions.a 含 coordinator 部分（57 符号）

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/phase10-raft-redesign/`
- [ ] 1.4.2 `git commit -m "chore(phase10): S22 关闭——phase9 raft 业务重做推迟到 phase10+"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-phase10-raft-redesign/`
