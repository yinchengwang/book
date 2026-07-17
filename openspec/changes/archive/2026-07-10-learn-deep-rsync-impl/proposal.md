# S20+ — Learn Deep 数据 rsync 实现

## What Changes

S19 已声明数据流（`learning/notes/learn_deep/` → `apps/web/knowledge_hub/src/data/learn_deep/`），但 sync 是 stub。

S20+ 实现真正的 rsync：

1. **`learning/scripts/sync-learn-deep.sh`** 实现：
   - 检测 `learning/notes/learn_deep/*.md` 与 `apps/web/.../learn_deep/*.md` 时间戳
   - 较新者覆盖较旧者
   - 删除消失的文件（在两边同步）

## Why

**β 价值（学习日志）**：
- 学习者在 Obsidian 改 md，自动同步到知识库小程序
- 让"学习即写、写即可用"

## Scope

**包含**：
- 替换 stub 实现为真实 rsync 逻辑
- V1-V3 验证（不实际跑，dry-run）

**不包含**：
- ❌ 实时双向 watch（仅 manual 跑）
- ❌ Git LFS
- ❌ 实际写两边文件（先 dry-run 模式）
