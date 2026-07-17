# 看板渲染模块化

## Purpose

创建共享看板工具库 `data/kanban-render.js`（状态更新/进度计算），供 index.html 和 learning-kanban.html 加载使用。因两个页面数据模型不兼容（图书模型 vs ITEMS_REGISTRY 模型），各自保留内联渲染实现，共享辅助函数直接复用。`renderKanbanBoard()` 已定义标准 API，留待数据模型统一后切换。

## ADDED Requirements

### Requirement: 共享工具库 (renderKanbanBoard 已定义)

系统 SHALL 在 `data/kanban-render.js` 中提供 `renderKanbanBoard(containerId, items, options)` 函数，定义通用看板渲染 API 供后续使用。

#### Scenario: 通用看板渲染

- **WHEN** 调用 `renderKanbanBoard("kanban-embed", items, { compact: true })`
- **THEN** SHALL 在 `#kanban-embed` 容器中渲染紧凑模式看板（较小卡片、更少间距）

- **WHEN** 调用 `renderKanbanBoard("kanban-main", items, { showFilters: true })`
- **THEN** SHALL 在 `#kanban-main` 容器中渲染完整模式看板（含筛选器、搜索框、进度条）

### Requirement: 状态更新函数

系统 SHALL 提供 `updateKanbanStatus(itemId, newStatus)` 函数，更新指定知识点的看板状态，同时更新 DOM 显示和 localStorage 持久化。

#### Scenario: 状态切换持久化

- **WHEN** 调用 `updateKanbanStatus("c-syntax", "done")`
- **THEN** 对应 DOM 元素的样式 SHALL 更新为 "done" 状态
- **THEN** localStorage 中对应键值 SHALL 同步更新

### Requirement: 进度统计函数

系统 SHALL 提供 `getKanbanProgress(stackFilter)` 函数，返回指定技术栈或全部技术栈的学习进度统计。

#### Scenario: 进度计算

- **WHEN** 调用 `getKanbanProgress("c")`
- **THEN** SHALL 返回 `{ total, done, learning, todo, review, percent }` 统计对象

### Requirement: 两看板保留内联实现（数据模型不兼容）

因数据模型不兼容（index.html 图书模型 vs learning-kanban.html ITEMS_REGISTRY 模型），两个页面各自保留内联看板渲染实现。共享库提供 `updateKanbanStatus()` 和 `getKanbanProgress()` 等可复用辅助函数。`renderKanbanBoard()` 作为标准 API 定义，待数据模型统一后切换。
