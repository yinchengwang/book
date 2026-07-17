# 提案：项目重命名为 knowledge-hub 并完成下划线命名规范化

## Why

当前项目存在以下问题：

1. **项目名不贴切**：`reading-radar-2` 与实际功能（知识管理、学习追踪）关联度低
2. **命名规范不一致**：部分文件名/路径使用中划线（`learn-deep`），不符合项目其他部分的命名习惯
3. **图解系列渲染失效**：396 篇图解文章依赖手写的简化 Markdown 解析器，无法正确渲染图片、表格等复杂元素
4. **内容存储不规范**：图解内容全部打包在单个 TS 文件中，而非 CLAUDE.md 规定的 `.md` 文件格式

本次重构将统一命名规范、修复渲染问题，并为后续功能迭代打下基础。

## What Changes

### 项目级变更

- 重命名项目目录：`reading-radar-2/` → `knowledge_hub/`
- 更新根目录 CLAUDE.md 中的项目引用
- 更新 `project.config.json`（如有）

### 命名规范化（下划线化）

所有文件名、目录名、路由路径统一使用下划线命名：

| 类型 | 原名 | 新名 |
|------|------|------|
| 页面目录 | `src/pages/learn-deep/` | `src/pages/learn_deep/` |
| 页面组件 | `LearnDeep.tsx` | `learn_deep.tsx` |
| 路由路径 | `/learn-deep` | `/learn_deep` |
| 数据文件 | `learn-deep-index.ts` | `learn_deep_index.ts` |
| 数据文件 | `learn-deep-content.ts` | `learn_deep_content.ts` |
| 样式文件 | `LearnDeep.scss` | `learn_deep.scss` |
| 路由配置 | `routes.ts` 中的路径 | 下划线路径 |
| 懒加载配置 | `App.tsx` 中的路径 | 下划线路径 |
| E2E 测试 | `e2e/learn-deep.spec.ts` | `e2e/learn_deep.spec.ts` |
| 迁移脚本 | `web/scripts/migrate-learn-deep.cjs` | `web/scripts/migrate_learn_deep.cjs` |

**BREAKING**：路由路径变更会导致用户收藏的旧链接失效。

### 图解系列重构

- 将 `learn-deep-content.ts` 中的 396 篇文章拆分到独立的 `.md` 文件
- 目录结构：`src/data/learn-deep/{category}/{slug}.md`
- 创建 Markdown 渲染组件，支持：
  - 标题（h1-h6）
  - 代码块（带语法高亮）
  - 图片（`![alt](url)`）
  - 表格（GFM 风格）
  - 引用、列表、粗体、斜体
  - 链接（外部链接新窗口打开）
- Web 端使用 `react-markdown` + `remark-gfm`
- 小程序端实现兼容方案

## Capabilities

### New Capabilities

- `markdown-renderer`: 跨平台 Markdown 渲染组件，支持图片、表格、代码高亮
- `learn-deep-md-files`: 将 396 篇文章从 TS 文件迁移到独立 MD 文件

### Modified Capabilities

- `illustrate-series`: 更新图解系列存储方式（TS → MD），同步渲染组件要求
- `markdown-content-rendering`: 扩展支持小程序端渲染需求

## Impact

### 受影响代码

| 文件/目录 | 变更类型 |
|-----------|----------|
| `reading-radar-2/` 目录 | 重命名 |
| `src/pages/learn-deep/` | 目录+文件重命名 |
| `src/data/learn-deep-*.ts` | 文件重命名 |
| `web/config/routes.ts` | 路由路径更新 |
| `web/App.tsx` | import 路径更新 |
| `src/pages/*/页面.tsx` | import 路径更新 |
| `e2e/*.spec.ts` | import 路径更新 |
| `web/scripts/migrate-*.cjs` | 文件重命名 |

### 依赖关系

- Web 端：已安装 `react-markdown` + `remark-gfm`，无需新增依赖
- 小程序端：需要实现兼容的 Markdown 渲染方案

### 需同步更新的文档

- `CLAUDE.md`
- `docs/README.md`
- `docs/ARCHITECTURE.md`
- `docs/BUTTONS.md`（按钮选择器路径变更）
- `docs/TEST_CASES.md`
- 根目录 `CLAUDE.md` 中的子项目引用
