# P3 — Knowledge_Hub 数据双向 Sync

## What Changes

S20+ 实现了 sync 单向 + S27 真跑 6 README.md。P3 升级为**双向 + conflict-detection**：

1. **双向 sync**：新增 `--reverse` 选项（已存在但未实现），mirror flow
2. **冲突检测**：若主存储和副本均修改（mtime 都 > sync_state），产生 conflict report
3. **sync state 文件**：`.sync-state.json` 记住上次成功的 sync mtime
4. **CI gate**：可选——CI 失败时发出 warning

## Why

**β 价值**：真正双向数据流——学习者在 Obsidian 改写，知识库小程序同步更新

**前置依赖**：
- S20+ sync-learn-deep.sh 已实现 rsync 版本
- S27 第一轮真同步成功

## Scope

**包含**：
- `--reverse` 实际实现
- conflict report 输出
- sync state 持久化
- 验证 V1-V3

**不包含**：
- ❌ 真双向冲突解决（manual）
- ❌ fs watch 实时
- ❌ 跨设备 sync
