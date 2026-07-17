# 面试追踪 — Proposal

## Why

当前 `interview.html` 仅用于浏览面试题库（问答式），缺少对个人面试进展的追踪能力。求职者在面试过程中需要记录公司状态、面试问题回顾、JD 信息和薪资等数据，但目前没有任何工具支持这一流程。新增"面试追踪"页面，作为个人面试管线管理器。

## What Changes

- **新增** `interview-tracker.html` 页面：面试追踪主界面
- **新增** 公司卡片视图：BOSS 直聘风格的矩形卡片，横向平铺，支持时间线分组
- **新增** 14 种面试状态：意向公司 → 沟通中 → 已投递 → 简历筛选 → 一面 → 二面 → 三面 → 主管面 → HR面 → 薪资谈判 → 背调 → Offer → 已接受 / 已拒绝
- **新增** 公司详情面板：JD 记录、多轮技术面试笔记、综合面/主管面/HR面记录、自由备注
- **新增** 时间线排序：按月份分组展示公司卡片，状态更新时自动跨月移动
- **新增** 多维过滤：月份选择、地域多选、关键词搜索、4 种排序方式
- **新增** 快捷筛选条：流程中 / 待回复 / Offer / 已拒绝
- **新增** 统计概览：总计、流程中、待回复、Offer、已拒绝
- **新增** Markdown 存储：公司数据以 Markdown 文件存储在 `data/interview-tracker/` 目录下，通过 server.js API 读写
- **新增** server.js API 路由：`/api/tracker/*` 用于 CRUD 操作
- **修改** `nav-component.js`：导航栏新增"面试追踪"入口
- **修改** `interview.html`：页面标题从"面试"改为"面试题库"

## Capabilities

### New Capabilities

- `interview-tracker-ui`: 面试追踪前端 UI（公司卡片、时间线、详情面板、新建弹窗、过滤排序）
- `interview-tracker-data`: 面试追踪数据层（Markdown 文件存储、server.js API、localStorage 降级）
- `interview-tracker-status`: 面试状态体系（14 种状态定义、进度条渲染、状态流转逻辑）

### Modified Capabilities

- 无（现有 spec 不涉及需求变更）

## Impact

- **新增文件**：`interview-tracker.html`、`data/app/interview-tracker.js`、`data/interview-tracker-api.js`、`data/interview-tracker/_index.json`
- **修改文件**：`data/app/nav-component.js`（新增导航项）、`server.js`（新增 API 路由）、`interview.html`（标题修改）
- **新增目录**：`data/interview-tracker/`（公司 Markdown 数据）
- **依赖**：复用现有 `marked.js`（Markdown 渲染）、`server.js`（API 服务）、全局导航组件
- **无外部依赖新增**：纯 HTML/JS，零构建工具
