# P4 — Design（Obsidian Vault 完整化）

## 1. 现状分析

`learning/notes/`:
- 132 md 笔记
- `_index/` 已有 home + 部分轨道
- `_templates/` 已有
- `.obsidian/` 已配置
- `_dashboard/` 缺

P4 增量：
- `_dashboard/progress.md`（顶层 Dataview 显示 6 轨道笔记数）
- `_index/` 各轨道完整索引
- `.obsidian/app.json` 校验 plugin 配置

## 2. progress.md Dataview 设计

```markdown
# 学习进度（动态仪表盘）

## 总览

\`\`\`dataview
TABLE length(rows) as "笔记数"
FROM #c OR #cpp OR #db OR #ds OR #grok OR #linux
GROUP BY (file.tags[0]) as "轨道"
SORT length(rows) DESC
\`\`\`

## 最近 7 天笔记

\`\`\`dataview
LIST FROM ""
WHERE file.cday >= date(today) - dur(7 days)
SORT file.cday DESC
\`\`\`
```

## 3. 轨道索引（_index/{c,cpp,...}.md）

每个轨道含：
- 简介
- 该轨道笔记列表（动态 dataview）
- 关键代码链接（`[[ds/linked_list_demo]]` 等）

## 4. .obsidian 配置校验

`app.json` 必须含：
- `dailyNote.enabled`
- `communityPlugins`
- dataview 启用

## 5. 不做

- ❌ 笔记内容修改
- ❌ 替换 vault
- ❌ 商业 plugins
