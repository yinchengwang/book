# S19 — Knowledge_Hub 数据 → Obsidian Vault 迁移

## What Changes

`apps/web/knowledge_hub/src/data/learn_deep/` 包含大量 markdown 数据（c/cpp/db/ds/grok/illustrate 6 大技术栈），用于知识库小程序的"学习内容"页面。

按 CLAUDE.md 知识库子项目规范"所有内容数据必须以 Markdown 文件存储"，这些已经在 markdown 中。但 **学习层 obsidian vault 标准化** 需要：

1. **从 `apps/web/knowledge_hub/src/data/learn_deep/` 迁出** 到 `learning/notes/learn_deep/`：
   - 知识库小程序仍是 markdown 数据消费者（避免破坏小程序运行）
   - 但**主存储**迁到 Obsidian vault
   
2. **新机制**：建立 `learning/notes/learn_deep/` ↔ `apps/web/knowledge_hub/src/data/learn_deep/` 软链接或同步脚本

**简化**：实际上保持现状（数据已在 md 格式），仅做**文档化说明**：
- `learning/notes/README.md` 新增 "Learn Deep 来源于 `apps/web/knowledge_hub/src/data/learn_deep/`" 说明
- `apps/web/knowledge_hub/CLAUDE.md` 注明"数据源主存储是 `learning/notes/learn_deep/`，本应用仅消费"

## Why

**β 价值（学习日志）**：
- Obsidian 是知识管理首选工具
- 学习层数据**已经在 markdown 中**，但分散在 `apps/web/knowledge_hub/src/data/learn_deep/` 与潜在 `learning/notes/`
- 让"学习者写一份 Markdown，知识库自动同步"是终极体验

**前置依赖**：
- S2 / S3 已声明学习层 obsidian vault 标准
- knowledge_hub 实际数据**已经是 markdown**

## Scope

**包含**：
- 在 `learning/notes/learn_deep/` 创建符号链接或 mock 目录（先建空目录）
- 在 `learning/notes/README.md` 增加 Learn Deep 来源说明
- 在 `knowledge_hub/CLAUDE.md` 增加"主存储来源"声明
- 提供 `learning/scripts/sync-learn-deep.sh`（先 stub）

**不包含**：
- ❌ 实际迁移 markdown 内容
- ❌ 双向同步脚本（先 stub）
- ❌ 跑小型 build 验证（仅文档化）

## 风险

| 风险 | 概率 | 缓解 |
|---|---|---|
| Windows symlink 兼容性 | 中 | 用目录代替 |
| 数据冗余（同一文件两份） | 中 | stub 同步脚本 |
