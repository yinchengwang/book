# P4 — Obsidian Vault 完整迁移

## What Changes

学习层 Obsidian vault 现状：
- `learning/notes/` 已存在，是 132 个学习笔记的根
- `.obsidian/` 已配置

P4 让 vault 真正"完整"用于 Obsidian 体验：

1. **vault README 标准化**：`_index/` 中各轨道页面
2. **Obsidian 配置文件**：`learning/notes/.obsidian/` 校验/补全
3. **dataview 仪表盘**：`_dashboard/progress.md` 实时显示学习进度
4. **vault 关系图**：用 `[[wikilinks]]` 把 vault 与工程代码、OpenSpec 互联

## Why

**β 价值**：
- 让学习者用 Obsidian 打开 vault 时有完整体验
- 数据可视化让"学了什么"可量化

**前置依赖**：
- 已存在 vault 与 132 笔记

## Scope

**包含**：
- 创建/更新 `_index/{c,cpp,db,ds,grok,linux,obsidian}.md` 轨道索引
- 创建 `_dashboard/progress.md` 顶层 Dataview
- 校验 `.obsidian/app.json` 含 community plugins 配置

**不包含**：
- ❌ 修改笔记内容
- ❌ 替换现有 vault 结构
- ❌ 商业插件集成
