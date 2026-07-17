## Why

现有的 `github-issue-todo` 项目是一个 GitHub Issue 风格的待办工具，功能局限于 Issue 管理（CRUD + Checklist + 标签）。需要将其演变为一个通用的全功能待办事项系统，支持优先级、截止日期、分组、评论、看板视图、统计看板，并与 OPSX 变更系统打通，以便从待办事项直接创建变更。

## What Changes

1. **目录改名**：`engineering/apps/github-issue-todo/` → `engineering/apps/todo-app/`
2. **数据模型扩展**：增加 `priority`、`due_date`、`group_id`、`sort_order`、`status(archived)` 字段
3. **新增分组管理**：groups 表，支持分类、颜色标识
4. **新增评论功能**：comments 表，支持待办评论
5. **新增统计看板 API**：`GET /api/todos/stats`
6. **新增拖拽排序 API**：`PATCH /api/todos/:id/sort`
7. **REST API 路径变更**：`/api/issues/*` → `/api/todos/*`
8. **新增 OPSX 变更桥接**：`POST /api/todos/:id/create-change`
9. **前端工程化**：从内嵌 `web/` 目录迁移为独立 Vue 3 + Vite 项目 `engineering/apps/web/todo-app/`
10. **数据自动迁移**：旧 `issue_tool.json` → 新 `todo_app.json` schema

## Capabilities

### New Capabilities
- `todo-priority`: 待办优先级管理（P0-P4，支持筛选和排序）
- `todo-due-date`: 截止日期管理（设置、过期高亮、筛选）
- `todo-groups`: 待办分组/分类管理（CRUD + 颜色标识）
- `todo-comments`: 待办评论（添加/删除/列表）
- `todo-sort`: 拖拽排序（手动调整待办顺序）
- `todo-stats`: 统计看板（完成率、优先级分布、分组分布、过期统计）
- `todo-create-change`: OPSX 变更创建桥接（从待办一键创建变更）
- `todo-board-view`: 看板视图（按分组/优先级分组展示）
- `todo-migration`: 旧数据自动迁移（issue_tool.json → todo_app.json）

### Modified Capabilities
- 无（全新项目，非现有 spec 变更）

## Impact

- **目录**：`engineering/apps/github-issue-todo/` 整体迁移到 `engineering/apps/todo-app/`
- **API 前缀**：`/api/issues/*` → `/api/todos/*`（**BREAKING**，旧客户端不兼容）
- **数据文件**：`issue_tool.json` → `todo_app.json`（自动迁移，旧文件备份为 `.bak`）
- **前端目录**：新增 `engineering/apps/web/todo-app/`（独立 Vue 3 + Vite 项目）
- **CMake**：`CMakeLists.txt` 中目标名和路径需更新
- **新依赖**：前端项目引入 Vue 3、Vue Router、Vite 构建工具