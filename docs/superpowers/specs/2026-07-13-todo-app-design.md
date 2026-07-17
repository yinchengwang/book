# Todo App — 待办事项系统设计文档

> 变更：从 GitHub Issue 待办工具演变为通用待办事项系统
> 日期：2026-07-13

## 概述

将现有的 `github-issue-todo` 项目演变为一个全功能待办事项系统，保留现有功能基础，增加优先级、截止日期、分组、评论、看板视图、统计看板等能力，并与 OPSX 变更系统打通。

---

## 1. 系统架构

```
┌──────────────────────────────────────────────────────────────┐
│              前端 (Vue 3 + Vite + Vue Router)                │
│           engineering/apps/web/todo-app/                     │
│                                                              │
│  ListView      DetailView     BoardView     StatsView        │
│  FilterBar     TodoCard       GroupManager  CreateChange     │
│                        ↕ fetch() / JSON                      │
├──────────────────────────────────────────────────────────────┤
│              后端 (C + Winsock HTTP Server)                   │
│           engineering/apps/todo-app/                          │
│                                                              │
│  main.c          — HTTP 服务器入口                            │
│  todo_handler.c  — REST API 路由分发                         │
│  todo_model.c    — 数据模型 + JSON 文件持久化                 │
│  todo_change.c   — OPSX 变更创建桥接                         │
│                                                              │
│  REST API (/api/todos/*) → JSON 响应                         │
└──────────────────────────────────────────────────────────────┘
```

### 目录结构变更

```
旧: engineering/apps/github-issue-todo/      → 新: engineering/apps/todo-app/
旧: engineering/apps/github-issue-todo/web/  → 新: engineering/apps/web/todo-app/
```

---

## 2. 数据模型

### todos（原 issues）

| 列名 | 类型 | 说明 | 来源 |
|------|------|------|------|
| id | INTEGER (PK, AUTO) | 编号 | 继承 |
| title | TEXT (NOT NULL) | 标题 | 继承 |
| description | TEXT | 描述 | 继承 |
| status | TEXT (DEFAULT 'open') | open / closed / archived | 扩展 |
| priority | INTEGER (DEFAULT 1) | 0=无, 1=低, 2=中, 3=高, 4=紧急 | 新增 |
| due_date | INTEGER (DEFAULT 0) | 截止日期时间戳, 0=无 | 新增 |
| group_id | INTEGER (DEFAULT 0) | 所属分组 ID, 0=未分组 | 新增 |
| sort_order | INTEGER (DEFAULT 0) | 手动排序序号 | 新增 |
| labels | TEXT | JSON 数组字符串 | 继承 |
| created_at | INTEGER | 创建时间 | 继承 |
| updated_at | INTEGER | 更新时间 | 继承 |

### checklist_items（不变）

| 列名 | 类型 | 说明 |
|------|------|------|
| id | INTEGER (PK, AUTO) | 编号 |
| todo_id | INTEGER (NOT NULL, FK) | 关联待办 ID（原 issue_id） |
| text | TEXT (NOT NULL) | 待办描述 |
| done | INTEGER (DEFAULT 0) | 0=未完成, 1=已完成 |
| sort_order | INTEGER (DEFAULT 0) | 排序序号 |

### groups（新增）

| 列名 | 类型 | 说明 |
|------|------|------|
| id | INTEGER (PK, AUTO) | 分组编号 |
| name | TEXT (NOT NULL) | 分组名称 |
| color | TEXT (DEFAULT '#4A90D9') | 颜色标识 |
| sort_order | INTEGER (DEFAULT 0) | 排序序号 |
| created_at | INTEGER | 创建时间 |

### comments（新增）

| 列名 | 类型 | 说明 |
|------|------|------|
| id | INTEGER (PK, AUTO) | 评论编号 |
| todo_id | INTEGER (NOT NULL, FK) | 关联待办 ID |
| text | TEXT (NOT NULL) | 评论内容 |
| created_at | INTEGER | 创建时间 |

---

## 3. REST API 设计

统一响应结构（与现有一致）：

```json
{ "code": 0, "data": {}, "msg": "ok" }
```

### 待办 API

| 方法 | 路径 | 功能 | 说明 |
|------|------|------|------|
| `GET` | `/api/todos` | 列表 | 继承，新增筛选参数 |
| `POST` | `/api/todos` | 创建 | 继承，新增字段 |
| `GET` | `/api/todos/:id` | 详情（含 Checklist + 评论） | 继承 |
| `PATCH` | `/api/todos/:id` | 更新 | 继承，新增字段 |
| `DELETE` | `/api/todos/:id` | 删除 | 继承 |
| `PATCH` | `/api/todos/:id/sort` | 更新拖拽排序 | 新增 |
| `GET` | `/api/todos/stats` | 统计看板数据 | 新增 |
| `POST` | `/api/todos/:id/create-change` | 创建 OPSX 变更 | 新增 |

### 列表筛选参数（扩展）

```
GET /api/todos?status=open&priority=3&group_id=1&due_before=1700000000&due_after=1600000000&sort=due_date&page=1&per_page=20
```

| 参数 | 说明 |
|------|------|
| status | open / closed / archived / all（默认 all） |
| priority | 0-4，按优先级筛选 |
| group_id | 按分组筛选 |
| due_before | 截止日期在之前 |
| due_after | 截止日期在之后 |
| sort | 排序：due_date / priority / sort_order / created_at（默认 sort_order） |
| labels | 逗号分隔标签 |
| search | 全文搜索标题和描述 |
| page | 页码（默认 1） |
| per_page | 每页数量（默认 20） |

### Checklist API（继承，路径变更）

| 方法 | 路径 | 功能 |
|------|------|------|
| `POST` | `/api/todos/:id/checklist` | 添加待办项 |
| `PATCH` | `/api/todos/:id/checklist/:item_id` | 切换完成状态 |
| `DELETE` | `/api/todos/:id/checklist/:item_id` | 删除待办项 |

### 评论 API（新增）

| 方法 | 路径 | 功能 |
|------|------|------|
| `GET` | `/api/todos/:id/comments` | 获取评论列表 |
| `POST` | `/api/todos/:id/comments` | 添加评论 |
| `DELETE` | `/api/todos/:id/comments/:cid` | 删除评论 |

### 分组 API（新增）

| 方法 | 路径 | 功能 |
|------|------|------|
| `GET` | `/api/groups` | 分组列表 |
| `POST` | `/api/groups` | 创建分组 |
| `PATCH` | `/api/groups/:id` | 更新分组（名称/颜色/排序） |
| `DELETE` | `/api/groups/:id` | 删除分组（级联解除待办关联） |

### 统计看板响应

```json
{
  "total": 42,
  "open": 15,
  "closed": 25,
  "archived": 2,
  "overdue": 3,
  "due_today": 5,
  "by_priority": [
    { "priority": 4, "count": 2, "label": "紧急" },
    { "priority": 3, "count": 5, "label": "高" },
    { "priority": 2, "count": 8, "label": "中" },
    { "priority": 1, "count": 3, "label": "低" }
  ],
  "by_group": [
    { "group_id": 1, "name": "工作", "color": "#4A90D9", "count": 10 }
  ],
  "completion_rate": 0.6
}
```

### OPSX 变更桥接

`POST /api/todos/:id/create-change`

请求体无需参数（使用待办自身数据），响应：

```json
{
  "change_id": "opsx-change-xxx",
  "url": "https://..."
}
```

---

## 4. 前端路由与组件设计

### 路由

| 路径 | 组件 | 说明 |
|------|------|------|
| `/` | `ListView.vue` | 列表视图（默认） |
| `/board` | `BoardView.vue` | 看板视图（按分组/优先级分组） |
| `/stats` | `StatsView.vue` | 统计看板 |
| `/groups` | `GroupManager.vue` | 分组管理 |

### 组件树

```
App.vue
├── AppHeader.vue              — 导航栏 + 搜索 + 新建按钮
├── router-view
│   ├── ListView.vue           — 列表视图
│   │   ├── FilterBar.vue      — 筛选面板
│   │   ├── TodoCard.vue       — 待办卡片
│   │   └── Pagination.vue     — 分页
│   ├── BoardView.vue          — 看板视图
│   │   └── BoardColumn.vue    — 分组列
│   ├── StatsView.vue          — 统计看板
│   └── GroupManager.vue       — 分组管理
├── DetailPanel.vue            — 侧边栏详情
│   ├── Checklist.vue          — 待办清单
│   ├── Comments.vue           — 评论
│   └── CreateChangeBtn.vue    — OPSX 变更按钮
└── CreateDialog.vue           — 新建弹窗
```

### 状态管理

使用 Vue 3 Composition API（reactive/ref）管理状态，不引入 Vuex/Pinia，保持轻量：

- `useTodoList()` — 列表状态、筛选、分页
- `useTodoDetail()` — 详情、编辑、Checklist、评论
- `useGroups()` — 分组状态
- `useStats()` — 统计看板

---

## 5. OPSX 变更桥接设计

模块：`todo_change.c` / `todo_change.h`

核心逻辑：

```c
// 根据 todo_id 创建 OPSX 变更
// 调用 opsx-propose 脚本通过子进程执行
// 返回变更 ID 和 URL
int todo_create_change(int64_t todo_id, char *out_change_id, size_t out_size);
```

实现方式：
- 使用 `popen()` 或 `system()` 调用 OPSX CLI
- 命令格式：`opsx propose --title "待办标题" --description "待办描述"`
- 解析 CLI 输出获取变更 ID

---

## 6. 数据迁移方案

1. 启动时检测旧文件 `issue_tool.json` 是否存在
2. 存在则读取旧数据，给每条记录添加默认值：
   - `priority: 1`（低）
   - `due_date: 0`（无截止日期）
   - `group_id: 0`（未分组）
   - `sort_order: 0`
3. 写入新 JSON 文件 `todo_app.json`
4. 重命名旧文件为 `issue_tool.json.bak`
5. 后续所有操作使用新文件

---

## 7. 错误处理

| 场景 | HTTP 状态码 | code | msg |
|------|------------|------|-----|
| 成功 | 200 | 0 | ok |
| 参数错误 | 400 | 1001 | 缺少必填参数 |
| JSON 解析失败 | 400 | 1002 | invalid JSON format |
| 资源不存在 | 404 | 2001 | not found |
| 操作失败 | 500 | 9001 | 数据库操作失败 |
| OPSX 变更失败 | 500 | 9002 | 创建变更失败 |

---

## 8. 测试策略

### 后端测试

| 测试文件 | 测试内容 |
|----------|----------|
| `test_todo_model.cpp` | CRUD、筛选、优先级、截止日期、分组、评论、排序 |
| `test_todo_handler.cpp` | REST API 路由解析和响应 |
| `test_todo_migration.cpp` | 旧数据迁移兼容性 |

### 集成测试
- 启动后端 → curl 调用所有 API 端点 → 验证响应
- 验证数据持久化（重启后数据不丢失）
- 验证旧数据迁移

### 前端测试
- 开发阶段手动测试为主
- 后续可加 vitest 单元测试

---

## 9. 开发阶段规划

| 阶段 | 内容 | 交付物 |
|------|------|--------|
| **P1** | 基础改造：目录改名 + 数据模型扩展 + 迁移 | `engineering/apps/todo-app/`，旧数据自动迁移 |
| **P2** | 新增功能：优先级、截止日期、分组、评论、排序 | 完整后端 API，curl 可调用所有端点 |
| **P3** | 前端工程化：Vue 3 + Vite 项目初始化 | 独立 `engineering/apps/web/todo-app/`，基础页面可访问 |
| **P4** | 前端功能开发：列表/看板/统计/分组管理/详情侧边栏 | 完整前端体验 |
| **P5** | OPSX 变更桥接：`POST /api/todos/:id/create-change` | 从待办一键创建 OPSX 变更 |
| **P6** | 集成联调 + 测试 | 全链路可用，测试覆盖 |
