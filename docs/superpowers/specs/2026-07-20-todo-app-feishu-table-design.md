# todo-app 多维表格化设计文档

> **目标**：将 todo-app 从固定字段的 JSON 持久化应用，改造为飞书多维表格风格的可扩展字段系统，支持用户自定义字段类型和动态视图。

**架构**：后端 C + SQLite 嵌入式数据库 + 混合字段系统（EAV），前端 Vue 3 渐进适配。Phase 1 聚焦字段系统 + SQLite 迁移，Phase 2 做视图系统。

**Tech Stack**：C11、SQLite3 amalgamation、cJSON、Winsock2、Vue 3

---

## 1. 总体架构

```
┌─────────────────────────────────────────────────────────────┐
│                     前端（Vue 3）                           │
│  字段配置面板 / 动态表格视图 / 看板 / 日历 / 甘特图        │
└───────────────────────┬─────────────────────────────────────┘
                        │ REST API
┌───────────────────────▼─────────────────────────────────────┐
│                    HTTP 路由层（不变）                       │
│            todo_handler.c — 路由分发 + 参数解析              │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│                  业务逻辑层（调整）                          │
│       todo_model.c — 字段系统 + 动态字段 CRUD               │
│       todo_calendar.c — 日历（适配扩展字段）                │
│       todo_plan.c — 学习计划（适配扩展字段）                │
│       todo_stats.c — 统计（适配扩展字段）                   │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│                  持久化层（JSON → SQLite）                   │
│       todo_db.c — sqlite3 封装层（新建）                    │
│       ┌─────────────────────────────────────────────────┐   │
│       │ SQLite3（third_part/sqlite3/）                   │   │
│       │ amalgamation 单文件（sqlite3.c + sqlite3.h）     │   │
│       └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 设计原则

1. **混合字段模式**：内置字段（title、status、priority 等）保留为 `todos` 表的列，扩展字段通过 `fields_def` + `field_values` 管理
2. **向后兼容**：现有 API 端点不变，新增字段管理端点，前端可渐进升级
3. **数据迁移**：启动时自动从 JSON 迁移到 SQLite

---

## 2. SQLite Schema 设计

### 2.1 `todos` 表（内置字段）

```sql
CREATE TABLE IF NOT EXISTS todos (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    title           TEXT NOT NULL DEFAULT '',
    description     TEXT NOT NULL DEFAULT '',
    status          TEXT NOT NULL DEFAULT 'open',       -- open / closed / archived
    labels          TEXT NOT NULL DEFAULT '[]',         -- JSON 数组字符串
    priority        INTEGER NOT NULL DEFAULT 2,         -- 0=紧急 1=高 2=中 3=低 4=无
    due_date        INTEGER NOT NULL DEFAULT 0,         -- Unix 时间戳
    group_id        INTEGER NOT NULL DEFAULT 0,
    sort_order      INTEGER NOT NULL DEFAULT 0,
    todo_type       INTEGER NOT NULL DEFAULT 0,         -- 0=普通 1=每日 2=每周 3=每月
    original_date   INTEGER NOT NULL DEFAULT 0,
    carryover_count INTEGER NOT NULL DEFAULT 0,
    plan_id         INTEGER NOT NULL DEFAULT 0,
    plan_item_id    INTEGER NOT NULL DEFAULT 0,
    completed_at    INTEGER NOT NULL DEFAULT 0,
    postpone_until  INTEGER NOT NULL DEFAULT 0,
    task_system_id  INTEGER NOT NULL DEFAULT 0,
    created_at      INTEGER NOT NULL DEFAULT (unixepoch()),
    updated_at      INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_todos_status    ON todos(status);
CREATE INDEX IF NOT EXISTS idx_todos_group     ON todos(group_id);
CREATE INDEX IF NOT EXISTS idx_todos_due_date  ON todos(due_date);
CREATE INDEX IF NOT EXISTS idx_todos_plan      ON todos(plan_id);
CREATE INDEX IF NOT EXISTS idx_todos_ts        ON todos(task_system_id);
```

### 2.2 `fields_def` 表（字段定义）

```sql
CREATE TABLE IF NOT EXISTS fields_def (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL,                -- 字段名
    type        TEXT NOT NULL,                -- text / number / single_select / multi_select / date / datetime / user / attachment / formula / link
    options     TEXT NOT NULL DEFAULT '{}',   -- JSON 配置
    built_in    INTEGER NOT NULL DEFAULT 0,   -- 1=内置字段（不可删除，不可改类型）
    ref_table   TEXT,                         -- link 类型：关联表名
    ref_field   TEXT,                         -- link 类型：关联字段名
    formula     TEXT,                         -- formula 类型：表达式
    sort_order  INTEGER NOT NULL DEFAULT 0,
    created_at  INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_fields_order ON fields_def(sort_order);
```

**内置字段预置**（id 固定 1-9，built_in=1，不可删除）：

| id | name | type | options |
|----|------|------|---------|
| 1 | 标题 | text | `{}` |
| 2 | 描述 | text | `{}` |
| 3 | 状态 | single_select | `{"choices":["open","closed","archived"]}` |
| 4 | 优先级 | single_select | `{"choices":["紧急","高","中","低","无"]}` |
| 5 | 截止日期 | date | `{}` |
| 6 | 标签 | multi_select | `{}` |
| 7 | 分组 | single_select | `{}` |
| 8 | 创建时间 | datetime | `{}` |
| 9 | 更新时间 | datetime | `{}` |

### 2.3 `field_values` 表（字段值 - EAV）

```sql
CREATE TABLE IF NOT EXISTS field_values (
    todo_id  INTEGER NOT NULL,
    field_id INTEGER NOT NULL,
    value    TEXT,                              -- 统一 TEXT 存储
    PRIMARY KEY (todo_id, field_id),
    FOREIGN KEY (todo_id)  REFERENCES todos(id)       ON DELETE CASCADE,
    FOREIGN KEY (field_id) REFERENCES fields_def(id)  ON DELETE CASCADE
) WITHOUT ROWID;

CREATE INDEX IF NOT EXISTS idx_fv_field ON field_values(field_id);
```

**字段类型 → 存储方式**：

| 类型 | value 存储格式 | 示例 |
|------|---------------|------|
| text | 原始字符串 | `"读 Database Internals"` |
| number | 数字字符串 | `"42"` |
| single_select | 选中值 | `"open"` |
| multi_select | JSON 数组 | `"[\"urgent\",\"backend\"]"` |
| date | Unix 时间戳字符串 | `"1784547746"` |
| datetime | Unix 时间戳字符串 | `"1784547746"` |
| user | 用户 ID 字符串 | `"1"` |
| attachment | JSON 对象 | `"{\"name\":\"file.pdf\",\"size\":1024}"` |
| formula | 动态计算，不存值 | — |
| link | 关联记录 ID 字符串 | `"5"` |

### 2.4 其他表（直接迁移现有结构）

```sql
CREATE TABLE IF NOT EXISTS checklist (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    todo_id    INTEGER NOT NULL,
    text       TEXT NOT NULL,
    done       INTEGER NOT NULL DEFAULT 0,
    sort_order INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (todo_id) REFERENCES todos(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_check_todo ON checklist(todo_id);

CREATE TABLE IF NOT EXISTS groups (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT NOT NULL,
    color      TEXT NOT NULL DEFAULT '#4A90D9',
    sort_order INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE TABLE IF NOT EXISTS comments (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    todo_id    INTEGER NOT NULL,
    text       TEXT NOT NULL,
    created_at INTEGER NOT NULL DEFAULT (unixepoch()),
    FOREIGN KEY (todo_id) REFERENCES todos(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_comment_todo ON comments(todo_id);

CREATE TABLE IF NOT EXISTS task_systems (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    color       TEXT NOT NULL DEFAULT '#4A90D9',
    sort_order  INTEGER NOT NULL DEFAULT 0,
    created_at  INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE TABLE IF NOT EXISTS plans (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    start_date  INTEGER NOT NULL DEFAULT 0,
    end_date    INTEGER NOT NULL DEFAULT 0,
    color       TEXT NOT NULL DEFAULT '#4A90D9',
    status      INTEGER NOT NULL DEFAULT 0,
    created_at  INTEGER NOT NULL DEFAULT (unixepoch()),
    updated_at  INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE TABLE IF NOT EXISTS plan_items (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    plan_id          INTEGER NOT NULL,
    parent_id        INTEGER NOT NULL DEFAULT 0,
    title            TEXT NOT NULL,
    item_type        INTEGER NOT NULL DEFAULT 3,
    planned_date     INTEGER NOT NULL DEFAULT 0,
    estimated_minutes INTEGER NOT NULL DEFAULT 0,
    order_index      INTEGER NOT NULL DEFAULT 0,
    completion_rule  INTEGER NOT NULL DEFAULT 1,
    todo_id          INTEGER NOT NULL DEFAULT 0,
    actual_minutes   INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (plan_id) REFERENCES plans(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_pi_plan ON plan_items(plan_id);
```

---

## 3. Metatable 元数据管理

系统需要记录自身元数据（版本号、自增 ID 状态等），新增 `meta` 表：

```sql
CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- 预置元数据
INSERT OR IGNORE INTO meta (key, value) VALUES
    ('schema_version', '1'),
    ('next_todo_id', '1'),
    ('next_check_id', '1'),
    ('next_group_id', '1'),
    ('next_comment_id', '1'),
    ('next_ts_id', '1'),
    ('next_plan_id', '1'),
    ('next_pi_id', '1'),
    ('next_field_id', '10');
```

---

## 4. 文件结构变更

### 新增文件

| 文件 | 职责 |
|------|------|
| `third_part/sqlite3/sqlite3.c` | SQLite amalgamation 源码 |
| `third_part/sqlite3/sqlite3.h` | SQLite 头文件 |
| `third_part/sqlite3/CMakeLists.txt` | SQLite 静态库构建 |
| `engineering/apps/todo-app/todo_db.h` | SQLite 持久化层接口 |
| `engineering/apps/todo-app/todo_db.c` | SQLite 持久化层实现 |
| `engineering/apps/todo-app/todo_field.h` | 字段系统接口 |
| `engineering/apps/todo-app/todo_field.c` | 字段系统实现 |

### 修改文件

| 文件 | 变更内容 |
|------|---------|
| `engineering/apps/todo-app/CMakeLists.txt` | 添加 sqlite3 依赖 |
| `engineering/apps/todo-app/todo_model.c` | 替换 JSON 持久化为 SQLite；添加字段值读写 |
| `engineering/apps/todo-app/todo_model.h` | 添加字段相关 API 声明 |
| `engineering/apps/todo-app/todo_handler.c` | 新增字段管理路由和 handler |
| `engineering/apps/todo-app/main.c` | 启动时调用 SQLite 初始化 |

---

## 5. API 设计

### 5.1 字段管理（新增端点）

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/fields` | 获取所有字段定义（含内置字段） |
| `POST` | `/api/fields` | 创建自定义字段 |
| `PATCH` | `/api/fields/:id` | 更新字段定义（名称、选项等） |
| `DELETE` | `/api/fields/:id` | 删除自定义字段（内置字段返回 400） |
| `PATCH` | `/api/fields/:id/sort` | 调整字段排序 |

**POST /api/fields 请求体**：
```json
{
  "name": "负责人",
  "type": "single_select",
  "options": {
    "choices": ["张三", "李四", "王五"]
  }
}
```

**响应**：
```json
{
  "code": 0,
  "data": {
    "id": 10,
    "name": "负责人",
    "type": "single_select",
    "options": {"choices": ["张三", "李四", "王五"]},
    "built_in": 0,
    "sort_order": 10
  }
}
```

### 5.2 字段值操作（新增端点）

**PATCH /api/todos/:id/fields** — 更新扩展字段值：

```json
// 请求体
{
  "fields": {
    "10": "张三",
    "11": "42",
    "12": ["选项A", "选项B"]
  }
}

// 响应
{
  "code": 0,
  "data": {
    "id": 1,
    "fields": {
      "10": "张三",
      "11": "42",
      "12": ["选项A", "选项B"]
    }
  }
}
```

### 5.3 列表查询扩展

`GET /api/todos` 返回中，每条 todo 增加 `fields` 对象：

```json
{
  "code": 0,
  "data": {
    "items": [
      {
        "id": 1,
        "title": "读 Database Internals",
        "status": "open",
        "fields": {
          "10": "张三",
          "11": "42"
        }
      }
    ],
    "total": 45,
    "page": 1,
    "per_page": 20
  }
}
```

---

## 6. 数据迁移策略

### 迁移流程

应用启动时自动检测，无需手动操作：

1. 检查 SQLite 数据库文件是否存在（`todo_app.db`）
2. 如果不存在：
   a. 创建所有表
   b. 预置内置字段（1-9）
   c. 检查 `todo_app.json` 是否存在
   d. 如果 JSON 文件存在，读取并逐条插入 SQLite
   e. 重命名 `todo_app.json` → `todo_app.json.bak`
3. 如果存在，正常打开

### 迁移函数

```c
// todo_db.h
int todo_db_open(const char *path);
void todo_db_close(void);
int todo_db_migrate_from_json(const char *json_path);
```

### 回滚策略

如果 SQLite 迁移失败，保留原始 JSON 文件不动，应用退回 JSON 模式。`todo_app.json.bak` 可手动恢复。

---

## 7. 字段类型 options 规范

每种字段类型在 `options` 列中存储不同的配置：

```json
// single_select
{"choices": ["open", "closed", "archived"]}

// multi_select
{"choices": ["urgent", "backend", "frontend"]}

// number
{"min": 0, "max": 100, "unit": "小时"}

// date / datetime
{"format": "YYYY-MM-DD"}

// user
{"multi": false}

// attachment
{"max_size": 10485760, "allowed_types": ["pdf", "doc"]}

// formula
{"expression": "{优先级} + 1", "result_type": "number"}

// link
{"ref_table": "todos", "ref_field": "title", "multi": false}
```

---

## 8. Phase 2 预览：视图系统

Phase 1 只做字段系统 + SQLite 迁移，视图系统作为 Phase 2。

### 视图定义表（Phase 2）

```sql
CREATE TABLE views (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT NOT NULL,
    type        TEXT NOT NULL,   -- table / board / calendar / gantt
    config      TEXT NOT NULL DEFAULT '{}',
    is_default  INTEGER NOT NULL DEFAULT 0,
    sort_order  INTEGER NOT NULL DEFAULT 0,
    created_at  INTEGER NOT NULL DEFAULT (unixepoch())
);
```

`config` 字段存储视图配置 JSON：

```json
// 表格视图
{
  "visible_fields": [1, 3, 4, 5, 10, 11],
  "frozen_fields": [1],
  "group_by": "3",
  "sort_by": {"field": "5", "desc": false}
}

// 看板视图
{
  "group_field": "3",
  "card_fields": [1, 5, 10],
  "card_cover_field": null
}

// 日历视图
{
  "date_field": "5",
  "show_fields": [1, 10]
}

// 甘特图
{
  "start_field": "5",
  "end_field": null,
  "duration_field": "11",
  "bar_label_field": "1"
}
```

### 视图切换

前端顶部增加视图切换栏，用户可切换表格/看板/日历/甘特图，每个视图可自定义显示的字段和布局。

---

## 9. 实施计划（分阶段）

### Phase 1.1：基础设施引入
- 下载 SQLite amalgamation 到 `third_part/sqlite3/`
- 编写 `third_part/sqlite3/CMakeLists.txt`
- 修改 `engineering/apps/todo-app/CMakeLists.txt` 添加 sqlite3 依赖
- 编译验证

### Phase 1.2：SQLite 持久化层
- 新建 `todo_db.h` / `todo_db.c`
- 实现所有表的 CREATE TABLE
- 实现 `todo_db_open()` / `todo_db_close()`
- 实现 JSON 到 SQLite 的迁移函数
- 实现 `todo_db_save()` 替代方案（SQLite 实时写入）

### Phase 1.3：模型层适配
- 修改 `todo_model.c` 中的所有 CRUD 函数，从 JSON 操作改为 SQLite 操作
- 添加字段值读写 API
- 保持 `todo_t` 结构体和现有函数签名不变

### Phase 1.4：字段系统
- 新建 `todo_field.h` / `todo_field.c`
- 实现字段定义 CRUD
- 实现字段值 EAV 读写
- 预置 9 个内置字段

### Phase 1.5：API 扩展
- 在 `todo_handler.c` 中新增字段管理路由
- 新增 `PATCH /api/todos/:id/fields` 端点
- 修改 `handle_list` 返回中携带 `fields` 数据
- 新增 `handle_fields_list` / `handle_fields_create` / `handle_fields_update` / `handle_fields_delete` / `handle_fields_sort`

### Phase 1.6：前端适配
- 前端新增字段配置面板（创建/编辑/删除字段）
- 表格视图适配动态列
- 新建/编辑 TODO 时支持扩展字段
- 详情面板展示扩展字段

### Phase 2：视图系统（后续）
- 视图定义 CRUD
- 表格/看板/日历/甘特图视图实现
- 视图切换和配置 UI

---

## 10. 边界情况与错误处理

| 场景 | 处理方式 |
|------|---------|
| JSON 迁移中途失败 | 回滚事务，JSON 不动，日志记录错误 |
| 删除字段时已有数据 | 级联删除 `field_values` 中对应数据 |
| 修改字段类型 | 已有数据保留原值，新值按新类型校验 |
| 内置字段操作 | 拒绝删除/改类型，返回 400 |
| 字段名重复 | 创建时校验，返回 400 |
| SQLite 文件损坏 | 返回错误码，日志提示运行 `sqlite3 todo_app.db ".recover"` |
| 高并发写入 | SQLite 默认串行化，WAL 模式可提升性能 |