# S27 — Knowledge_Hub First Sync Round（首轮实际数据同步）

## What Changes

S20+ 已实现 `learning/scripts/sync-learn-deep.sh` stub。S27 **第一次真正跑一次** 同步：

1. **创建占位 markdown**：在 `learning/notes/learn_deep/{c,cpp,db,ds,grok,illustrate}/` 写 1-2 个示例文件
2. **运行同步**：`bash learning/scripts/sync-learn-deep.sh --dry-run` 看效果
3. **真跑**：`bash learning/scripts/sync-learn-deep.sh` 把数据复制到 `apps/web/knowledge_hub/src/data/learn_deep/`
4. **验证双端**：两边文件都存在

## Why

**β 价值**：双源数据流从"声明" → "可观察"

## Scope

**包含**：
- 6 个示例 .md 文件
- 一次 dry-run 测试
- 一次真跑

**不包含**：
- ❌ 真实数据迁移
- ❌ 双向 sync 脚本
- ❌ watch / fs event
