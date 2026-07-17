# 技术摘抄视图 — Proposal

## Why

当前 `notes/excerpt/` 目录下的摘抄数据以纯文本 `.txt` 和少量 `.md` 文件散落存储，缺乏结构化和可视化浏览能力。用户在 Obsidian 中编辑摘抄后，无法在 reading-radar 页面中以卡片流形式浏览、搜索、筛选。目标网站（learn.lianglianglee.com）展示了结构化技术摘抄的最佳实践，需要在读书雷达中新增"摘抄"Tab，提供专业级的摘抄浏览与管理体验。

## What Changes

- **新增** `excerpt-view.js`：摘抄视图核心 JS 模块（双栏布局、卡片渲染、分页、搜索）
- **新增** `excerpt-api.js`：API 封装层（fetch 调用、参数构建、错误处理）
- **新增** server.js API 路由：`/api/excerpts/*` 用于摘抄 CRUD 和搜索
- **修改** `index.html`：新增"📝 摘抄"Tab 按钮 + 双栏布局 HTML + CSS + URL hash 路由（`#excerpt`）
- **新增** `scripts/migrate-excerpts.js`：.txt → .md 批量迁移脚本（YAML frontmatter）
- **修改** `notes/excerpt/` 下所有 `.txt` 文件 → `.md` 文件（带 frontmatter 结构化）
- **修改** `BOOK_DATA`：每本书新增 `excerptCount` 和 `excerptLink` 字段，看板卡片显示摘抄数

> 注意：摘抄是 index.html 的内部 Tab，**不是独立 HTML 页面**，因此**不需要修改** `nav-component.js`。URL hash 路由 `#excerpt` 由 index.html 内部处理，不经过全局导航栏。

## Capabilities

### Modified Capabilities

- `data-item-registry`: BOOK_DATA 新增 excerptCount/excerptLink 字段

### New Capabilities

- `excerpt-ui`: 摘抄前端 UI（双栏布局、卡片渲染、编辑弹窗、批注面板、标签筛选）
- `excerpt-data`: 摘抄数据层（.md frontmatter 解析、Obsidian 双向链接、localStorage 降级）
- `excerpt-api`: server.js RESTful API（书籍列表、分页查询、全文搜索、元数据更新）
