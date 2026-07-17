# 技术摘抄视图 — 设计文档

## Context

reading-radar 是一个纯静态 HTML/JS 学习站点，无构建工具。所有页面通过 `node server.js` 本地服务访问。现有 `index.html` 是读书雷达页面（雷达图 + 看板），需要在其中新增"摘抄"Tab。

摘抄数据存储在 `notes/excerpt/` 目录下，当前格式混杂（`.txt` 和 `.md`），无结构化元数据。Obsidian 可直接读取该目录下的文件。

**关键约束**：
- 零外部依赖（复用现有 marked.js CDN）
- 所有摘抄数据以 Markdown 文件存储在磁盘上，Git 可追踪
- Obsidian 双向链接：Obsidian 中编辑的内容自动在 HTML 中可见
- 支持有 server.js（http://）和无 server.js（file://）两种模式
- 风格必须与现有页面（index.html, interview.html）统一

## Goals / Non-Goals

**Goals:**
- 左右双栏布局：左侧书籍导航 + 右侧摘抄卡片流
- Markdown 文件存储，Obsidian 直接编辑，HTML 自动渲染
- server.js API 全文搜索 + 结构化筛选（书/标签/年份/批注）
- 手动编辑元数据界面（书名/作者/标签/收藏/批注）
- Obsidian 双向链接：点击 🔗 按钮在 Obsidian 中打开对应文件和段落
- 分页展示（默认 20 条/页）
- 响应式布局（桌面双栏 / 移动端单栏）
- 与读书雷达联动：看板卡片显示摘抄数，点击跳转

**Non-Goals:**
- 云端同步（无需注册，数据仅在本地）
- 团队协作（纯个人使用）
- OCR 文字识别（手动录入/复制粘贴）
- 间隔重复复习（Anki 集成）
- PDF 解析（仅支持已有 .md/.txt 文件）

## Decisions

### Decision 1: 数据存储 — YAML frontmatter + Markdown 正文

**选择**：每条摘抄以 `.md` 文件存储，YAML frontmatter 存放元数据，正文存放摘抄内容。

```yaml
---
book: "代码整洁之道"
author: "Robert C. Martin"
date: "2025-01-15"
tags: [专业主义, 责任, 担当]
favorite: true
---

专业主义有很深的含义...
```

**理由**：
- YAML frontmatter 是 Obsidian 标准格式，Obsidian 原生支持
- 结构化元数据支持筛选/搜索/统计
- 与现有 `interview-tracker` 的 Markdown 存储模式一致
- 纯文本可读，Git 可追踪

**替代方案**：
- 纯 JSON 存储 — 舍弃原因：Obsidian 不直接支持 JSON 编辑
- 继续用 .txt — 舍弃原因：无法结构化元数据

### Decision 2: 布局 — 左右双栏（非 Tab 切换）

**选择**：左侧书籍导航栏（~280px）+ 右侧摘抄内容区（flex:1）。

**理由**：
- 左侧可快速切换书籍/标签/年份，右侧即时刷新
- 与现有 `interview.html` 的题库浏览模式类似（左侧分类 + 右侧内容）
- 桌面端信息密度高，移动端自动切换为单栏

**替代方案**：Tab 切换（书籍列表页 + 摘抄详情页）
- 舍弃原因：切换成本高，双栏更流畅

### Decision 3: 搜索 — 服务端搜索（非前端）

**选择**：server.js 扫描所有 `.md` 文件，前端 fetch 结果。

**理由**：
- 摘抄文件可能较多（数百条），全量加载到前端影响性能
- 服务端搜索支持增量更新（仅扫描变更文件）
- 与现有 `interview-tracker` 的 API 模式一致

**替代方案**：前端一次性加载
- 舍弃原因：文件多时内存占用大，搜索慢

### Decision 4: 批注 — Obsidian 双向链接 + localStorage 双源

**选择**：批注优先写在 Obsidian 的 .md 文件中（引用块格式），同时支持 HTML 界面存到 localStorage。展示时合并两源。

**理由**：
- Obsidian 批注持久化到 .md 文件，Git 可追踪
- localStorage 批注方便快速记录，不破坏 .md 文件
- 双向链接格式 `[[笔记名]]` 在 Obsidian 中可跳转

**替代方案**：仅用 Obsidian 或仅用 localStorage
- 舍弃原因：单一来源不够灵活

### Decision 5: 元数据迁移 — 全量转换 .txt → .md

**选择**：写脚本将现有所有 `.txt` 批量转换为 `.md`（带 frontmatter）。

**理由**：
- 统一数据格式，消除混用
- 迁移后可享受结构化筛选/搜索
- 原始文件备份为 `.txt.bak`，可随时回退

**替代方案**：新旧共存
- 舍弃原因：维护两套解析逻辑增加复杂度

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 文件编码问题（GBK vs UTF-8） | 读取乱码 | 迁移脚本检测编码，统一转 UTF-8 |
| 大量摘抄文件导致 server.js 扫描慢 | 搜索延迟 | 启动时缓存索引，增量更新 |
| Obsidian 批注与 HTML 批注冲突 | 数据不一致 | 区分来源（obsidian/html），展示时合并 |
| 移动端双栏空间不足 | 体验差 | 响应式：移动端隐藏左侧栏，书籍选择用底部 sheet |
