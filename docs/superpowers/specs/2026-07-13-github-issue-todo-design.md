# GitHub Issue 待办工具 — 设计文档

> 变更：`github-issue-todo-tool` | 日期：2026-07-13

## 概述

在项目内实现一个类似 GitHub Issues 的待办事项工具，使用项目自有的关系存储引擎作为数据后端，Vue 3 作为前端框架。一期目标：验证存储引擎正确性的同时，提供一个可用的 Web 版 Issue 管理工具。

---

## 1. 系统架构

```
┌──────────────────────────────────────────────────────────┐
│                  前端 (Vue 3 SPA)                         │
│          engineering/apps/web/github-issue-todo/         │
│                                                          │
│  IssueList.vue  IssueDetail.vue  IssueCreate.vue         │
│  FilterBar.vue   IssueCard.vue   Checklist.vue           │
│                        ↕ fetch() / JSON                  │
├──────────────────────────────────────────────────────────┤
│                  后端 (C + libmicrohttpd)                 │
│          engineering/apps/github-issue-todo/             │
│                                                          │
│  main.c          — HTTP 服务器入口                        │
│  issue_handler.c — REST 路由分发                          │
│  issue_model.c   — 数据模型 + 关系引擎操作                 │
│                                                          │
│  REST API (/api/issues/*) → JSON 响应                    │
│                        ↕                                 │
├──────────────────────────────────────────────────────────┤
│                  关系存储引擎                              │
│                                                          │
│  Catalog (系统表/元数据)                                  │
│  Heap AM (堆表存储)                                       │
│  Buffer Pool (页面缓存)                                   │
│  WAL (写前日志)                                          │
│  Disk (页面管理)                                         │
└──────────────────────────────────────────────────────────┘
```

---

## 2. 数据模型

### issues 表

| 列名 | 类型 | 说明 |
|------|------|------|
| id | INTEGER (PK, AUTO) | Issue 编号 |
| title | TEXT (NOT NULL) | 标题 |
| description | TEXT | 详细描述 |
| status | TEXT (DEFAULT 'open') | open / closed |
| labels | TEXT | JSON 数组，如 `["bug","urgent"]` |
| created_at | INTEGER | 创建时间戳 |
| updated_at | INTEGER | 更新时间戳 |

### checklist_items 表

| 列名 | 类型 | 说明 |
|------|------|------|
| id | INTEGER (PK, AUTO) | 待办项编号 |
| issue_id | INTEGER (NOT NULL, FK) | 关联 Issue ID |
| text | TEXT (NOT NULL) | 待办描述 |
| done | INTEGER (DEFAULT 0) | 0=未完成, 1=已完成 |
| sort_order | INTEGER (DEFAULT 0) | 排序序号 |

---

## 3. REST API 设计

所有请求/响应均为 JSON 格式。

### 统一响应结构

```json
{
    "code": 0,
    "data": {},
    "msg": "ok"
}
```

错误时 `code` 为非零值，`msg` 为错误描述。

### 接口列表

| 方法 | 路径 | 功能 |
|------|------|------|
| `GET` | `/api/issues` | 列表（支持筛选参数） |
| `POST` | `/api/issues` | 创建 Issue |
| `GET` | `/api/issues/:id` | 查看详情（含待办清单） |
| `PATCH` | `/api/issues/:id` | 更新（状态/标签/标题/描述） |
| `DELETE` | `/api/issues/:id` | 删除 |
| `POST` | `/api/issues/:id/checklist` | 添加待办项 |
| `PATCH` | `/api/issues/:id/checklist/:item_id` | 切换待办完成状态 |
| `DELETE` | `/api/issues/:id/checklist/:item_id` | 删除待办项 |

### 列表筛选参数

```
GET /api/issues?status=open&labels=bug,urgent&search=关键词&page=1&per_page=20
```

| 参数 | 说明 |
|------|------|
| status | open / closed / all（默认 all） |
| labels | 逗号分隔的标签名，多标签取交集 |
| search | 全文搜索标题和描述 |
| page | 页码（默认 1） |
| per_page | 每页数量（默认 20） |

### 创建 Issue 请求体

```json
{
    "title": "实现登录功能",
    "description": "用户需要支持邮箱+密码登录",
    "labels": ["bug", "urgent"]
}
```

### 创建 Checklist 请求体

```json
{
    "text": "设计数据库表"
}
```

---

## 4. 目录结构

```
engineering/apps/
├── github-issue-todo/              # C 后端
│   ├── main.c                      # 入口，启动 HTTP 服务器
│   ├── issue_handler.c             # REST API 路由处理
│   ├── issue_handler.h
│   ├── issue_model.c               # 关系引擎操作封装
│   ├── issue_model.h
│   ├── CMakeLists.txt              # 编译配置
│   └── web/                        # 前端静态文件（编译后输出到此）
│       └── ... (Vue 构建产物)
│
└── web/
    └── github-issue-todo/          # Vue 3 前端源码
        ├── index.html              # 入口
        ├── src/
        │   ├── main.js             # Vue 应用初始化
        │   ├── App.vue             # 根组件
        │   ├── router.js           # 路由配置
        │   ├── api.js              # API 请求封装
        │   ├── components/
        │   │   ├── IssueList.vue   # 列表页
        │   │   ├── IssueDetail.vue # 详情页
        │   │   ├── IssueCreate.vue # 新建页
        │   │   ├── FilterBar.vue   # 筛选面板
        │   │   ├── IssueCard.vue   # 单条 Issue 卡片
        │   │   └── Checklist.vue   # 待办清单组件
        │   └── styles/
        │       └── main.css        # 全局样式
        ├── package.json
        └── vite.config.js          # Vite 构建配置
```

---

## 5. 后端组件设计

### issue_model.h — 核心接口

```c
// Issue 数据结构
typedef struct {
    int64_t id;
    char title[256];
    char description[4096];
    char status[16];       // "open" / "closed"
    char labels[512];      // JSON 数组字符串
    int64_t created_at;
    int64_t updated_at;
} issue_t;

// Checklist 数据结构
typedef struct {
    int64_t id;
    int64_t issue_id;
    char text[256];
    int done;              // 0 / 1
    int sort_order;
} checklist_item_t;

// 数据库初始化（创建表）
int issue_db_init(const char *db_path);

// Issue CRUD
int issue_create(issue_t *issue);
int issue_get_by_id(int64_t id, issue_t *issue);
int issue_update(const issue_t *issue);
int issue_delete(int64_t id);

// 列表查询（支持筛选）
typedef struct {
    const char *status;     // NULL 表示全部
    const char *labels;     // NULL 表示全部
    const char *search;     // NULL 表示不搜索
    int page;
    int per_page;
} issue_query_t;

typedef struct {
    issue_t *items;
    int count;
    int total;
} issue_list_t;

int issue_list(const issue_query_t *query, issue_list_t *result);
void issue_list_free(issue_list_t *result);

// Checklist 操作
int checklist_add(int64_t issue_id, const char *text, checklist_item_t *item);
int checklist_toggle(int64_t item_id);
int checklist_delete(int64_t item_id);
int checklist_list(int64_t issue_id, checklist_item_t **items, int *count);
```

### issue_handler.c — REST 路由

```c
// libmicrohttpd 回调
enum MHD_Result handle_request(void *cls,
    struct MHD_Connection *connection,
    const char *url, const char *method,
    const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls);

// 路由分发
// GET  /api/issues          → handle_list()
// POST /api/issues          → handle_create()
// GET  /api/issues/:id      → handle_get()
// PATCH /api/issues/:id     → handle_update()
// DELETE /api/issues/:id    → handle_delete()
// POST /api/issues/:id/checklist     → handle_checklist_add()
// PATCH /api/issues/:id/checklist/:item_id → handle_checklist_toggle()
// DELETE /api/issues/:id/checklist/:item_id → handle_checklist_delete()
```

### main.c — 服务器入口

```c
// 解析命令行参数（端口、数据库路径）
// 初始化关系引擎
// 初始化 Issue 表
// 启动 libmicrohttpd 服务器
// 注册静态文件路由（前端文件）
```

---

## 6. 数据流

### 创建 Issue 流程

```
用户填写表单 → 前端 POST /api/issues
    → issue_handler.c 解析 JSON
    → issue_model.c issue_create()
        → 调用 Catalog 打开/创建 issues 表
        → Heap AM table_insert() 写入数据
        → WAL 记录
        → 返回自增 ID
    → 封装 JSON 响应
    → 前端收到后跳转详情页
```

### 查询列表流程

```
用户打开列表页 → 前端 GET /api/issues?status=open&labels=bug
    → issue_handler.c 解析查询参数
    → issue_model.c issue_list()
        → 调用 table_scan() 全表扫描
        → 逐行匹配筛选条件（状态/标签/搜索关键词）
        → 分页截取
        → 返回匹配结果
    → 封装 JSON 响应
    → 前端渲染列表
```

---

## 7. 错误处理

| 场景 | HTTP 状态码 | code | msg |
|------|------------|------|-----|
| 成功 | 200 | 0 | ok |
| 参数错误 | 400 | 1001 | 缺少必填参数 |
| Issue 不存在 | 404 | 2001 | issue not found |
| 数据库错误 | 500 | 9001 | 数据库操作失败 |
| JSON 解析失败 | 400 | 1002 | invalid JSON format |

---

## 8. 测试策略

### 后端测试（C 单元测试）
- `test_issue_model.c` — 测试 CRUD、筛选、标签、Checklist
- `test_issue_handler.c` — 测试 REST API 路由解析和响应
- 使用 gtest + 测试用内存数据库

### 前端测试（后续）
- 手动测试为主（开发阶段）
- 后续可加 vitest 单元测试

### 集成测试
- 启动后端 → curl 调用所有 API 端点 → 验证响应
- 验证数据持久化（重启后端后数据不丢失）

---

## 9. 开发阶段

| 阶段 | 内容 | 交付物 |
|------|------|--------|
| **P1** | 数据模型 + 关系引擎操作 | issue_model.h / issue_model.c，单元测试通过 |
| **P2** | HTTP 服务器 + REST API | main.c / issue_handler.c，curl 可调用所有端点 |
| **P3** | Vue 3 前端 SPA | 列表页 + 详情页 + 新建页，完整交互 |
| **P4** | 集成联调 + 测试 | 全链路可用，测试覆盖 |