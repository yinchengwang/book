# Todo App 架构设计

## 1. 架构概览

```mermaid
flowchart TB
    subgraph 客户端层
        A[Web 浏览器]
        B[HTTP 客户端]
    end

    subgraph HTTP 服务器层
        C[libmicrohttpd<br/>HTTP 服务器]
        D[路由分发器]
    end

    subgraph 请求处理层
        E[todo_handler.h<br/>请求处理器]
        F[JSON 解析/序列化]
    end

    subgraph 数据模型层
        G[todo_model.h<br/>数据模型 + CRUD]
        H[todo_t / checklist_item_t<br/>group_t / comment_t]
    end

    subgraph 持久化层
        I[JSON 文件存储<br/>todo_app.json]
        J[todo_migration.h<br/>旧数据迁移]
    end

    subgraph 变更管理
        K[todo_change.h<br/>OPSX 变更支持]
    end

    A --> C
    B --> C
    C --> D
    D --> E
    E --> F
    F --> G
    G --> H
    G --> I
    J --> I
    E --> K
```

## 2. 数据模型

```mermaid
classDiagram
    class todo_t {
        +int64_t id
        +char title[256]
        +char description[4096]
        +char status[16]
        +char labels[1024]
        +int priority
        +int64_t due_date
        +int64_t group_id
        +int sort_order
        +int64_t created_at
        +int64_t updated_at
    }

    class checklist_item_t {
        +int64_t id
        +int64_t todo_id
        +char text[512]
        +int done
        +int sort_order
    }

    class group_t {
        +int64_t id
        +char name[64]
        +char color[16]
        +int sort_order
        +int64_t created_at
    }

    class comment_t {
        +int64_t id
        +int64_t todo_id
        +char text[2048]
        +int64_t created_at
    }

    class todo_query_t {
        +const char status
        +const char labels
        +const char search
        +int priority
        +int64_t group_id
        +int64_t due_before
        +int64_t due_after
        +const char sort
        +int sort_desc
        +int page
        +int per_page
    }

    class todo_stats_t {
        +int total
        +int open
        +int closed
        +int archived
        +int overdue
        +int due_today
        +double completion_rate
    }

    todo_t "1" --> "*" checklist_item_t : 包含
    todo_t "1" --> "*" comment_t : 包含
    todo_t "*" --> "1" group_t : 属于
    todo_query_t --> todo_t : 查询
    todo_stats_t --> todo_t : 统计
```

**优先级常量定义：**

| 常量 | 值 | 含义 |
|------|---|------|
| `PRIORITY_URGENT` | 0 | 紧急 |
| `PRIORITY_HIGH` | 1 | 高 |
| `PRIORITY_MEDIUM` | 2 | 中 |
| `PRIORITY_LOW` | 3 | 低 |
| `PRIORITY_NONE` | 4 | 无优先级 |

## 3. HTTP 路由

```mermaid
flowchart LR
    subgraph API 端点
        A["/api/todos"]
        B["/api/todos/:id"]
        C["/api/todos/stats"]
        D["/api/todos/:id/checklist"]
        E["/api/todos/:id/comments"]
        F["/api/groups"]
        G["/api/groups/:id"]
    end

    subgraph 操作
        A1["GET  列表查询"]
        A2["POST 创建待办"]
        B1["GET    获取详情"]
        B2["PUT    更新"]
        B3["DELETE 删除"]
        C1["GET 统计数据"]
        D1["GET  列出清单"]
        D2["POST 添加清单项"]
        E1["GET  列出评论"]
        E2["POST 添加评论"]
        F1["GET  列出分组"]
        F2["POST 创建分组"]
        G1["PUT    更新分组"]
        G2["DELETE 删除分组"]
    end

    A --> A1
    A --> A2
    B --> B1
    B --> B2
    B --> B3
    C --> C1
    D --> D1
    D --> D2
    E --> E1
    E --> E2
    F --> F1
    F --> F2
    G --> G1
    G --> G2
```

**RESTful API 映射表：**

| 端点 | 方法 | 功能 | 对应函数 |
|------|------|------|----------|
| `/api/todos` | GET | 列表查询 | `todo_list()` |
| `/api/todos` | POST | 创建待办 | `todo_create()` |
| `/api/todos/:id` | GET | 获取详情 | `todo_get_by_id()` |
| `/api/todos/:id` | PUT | 更新待办 | `todo_update()` |
| `/api/todos/:id` | DELETE | 删除待办 | `todo_delete()` |
| `/api/todos/stats` | GET | 统计数据 | `todo_get_stats()` |
| `/api/todos/:id/checklist` | GET | 列出清单 | `checklist_list()` |
| `/api/todos/:id/checklist` | POST | 添加清单项 | `checklist_add()` |
| `/api/todos/:id/comments` | GET | 列出评论 | `comment_list()` |
| `/api/todos/:id/comments` | POST | 添加评论 | `comment_add()` |
| `/api/groups` | GET | 列出分组 | `group_list()` |
| `/api/groups` | POST | 创建分组 | `group_create()` |
| `/api/groups/:id` | PUT | 更新分组 | `group_update()` |
| `/api/groups/:id` | DELETE | 删除分组 | `group_delete()` |

## 4. 请求处理流程

### 4.1 创建待办

```mermaid
sequenceDiagram
    participant C as 客户端
    participant H as HTTP 服务器
    participant R as 路由器
    participant T as todo_handler
    participant M as todo_model
    participant S as JSON 存储

    C->>H: POST /api/todos<br/>{title, description, ...}
    H->>R: 解析请求路径
    R->>T: 路由到创建处理器
    T->>T: 解析 JSON 请求体
    T->>M: todo_create(todo, &id)
    M->>M: 分配 ID (自增)
    M->>M: 设置 created_at/updated_at
    M->>S: 追加写入 JSON
    S-->>M: 写入成功
    M-->>T: 返回新 ID
    T-->>H: 构造 JSON 响应<br/>{id, title, ...}
    H-->>C: HTTP 201 Created
```

### 4.2 查询列表

```mermaid
sequenceDiagram
    participant C as 客户端
    participant H as HTTP 服务器
    participant R as 路由器
    participant T as todo_handler
    participant M as todo_model
    participant S as JSON 存储

    C->>H: GET /api/todos?status=open&sort=priority
    H->>R: 解析请求路径和查询参数
    R->>T: 路由到列表处理器
    T->>T: 构建 todo_query_t
    T->>M: todo_list(&query, &result)
    M->>S: 加载全部数据
    S-->>M: 返回 JSON 数组
    M->>M: 过滤 + 排序 + 分页
    M-->>T: 返回 todo_list_t
    T->>T: 序列化为 JSON
    T-->>H: 构造响应
    H-->>C: HTTP 200 OK<br/>{items: [...], total: N}
```

### 4.3 更新待办

```mermaid
sequenceDiagram
    participant C as 客户端
    participant H as HTTP 服务器
    participant R as 路由器
    participant T as todo_handler
    participant M as todo_model
    participant S as JSON 存储

    C->>H: PUT /api/todos/:id<br/>{title, status, ...}
    H->>R: 解析请求路径
    R->>T: 路由到更新处理器
    T->>T: 解析 URL 中的 ID
    T->>T: 解析 JSON 请求体
    T->>M: todo_get_by_id(id, &todo)
    M->>S: 查找记录
    S-->>M: 返回旧数据
    M-->>T: 返回旧待办
    T->>M: todo_update(&todo)
    M->>M: 更新 updated_at
    M->>S: 重写 JSON 文件
    S-->>M: 写入成功
    M-->>T: 更新成功
    T-->>H: 构造响应
    H-->>C: HTTP 200 OK
```

## 5. 持久化

```mermaid
flowchart TB
    subgraph 启动流程
        A[main.c 启动] --> B{检查旧数据?}
        B -->|是| C[todo_migrate_from_legacy]
        B -->|否| D[todo_db_load]
        C --> D
        D --> E[加载 JSON 到内存]
        E --> F[构建内存索引]
    end

    subgraph 运行时
        G[CRUD 操作] --> H[修改内存数据]
        H --> I[立即持久化<br/>todo_db_save]
        I --> J[覆盖写 JSON 文件]
    end

    subgraph JSON 结构
        K["{"<br/>  \"todos\": [...],<br/>  \"checklist_items\": [...],<br/>  \"groups\": [...],<br/>  \"comments\": [...],<br/>  \"next_ids\": {...}<br/>}"]
    end

    F --> G
    J --> K
```

**JSON 文件结构：**

```json
{
  "todos": [
    {
      "id": 1,
      "title": "完成架构文档",
      "description": "编写 Todo App 和 vdb_cli 架构设计",
      "status": "open",
      "labels": "[\"文档\", \"架构\"]",
      "priority": 1,
      "due_date": 1723996800,
      "group_id": 2,
      "sort_order": 0,
      "created_at": 1723824000,
      "updated_at": 1723824000
    }
  ],
  "checklist_items": [
    {
      "id": 1,
      "todo_id": 1,
      "text": "读取源码",
      "done": 1,
      "sort_order": 0
    }
  ],
  "groups": [
    {
      "id": 1,
      "name": "工作",
      "color": "#f85149",
      "sort_order": 0,
      "created_at": 1723824000
    }
  ],
  "comments": [
    {
      "id": 1,
      "todo_id": 1,
      "text": "已完成初稿",
      "created_at": 1723824000
    }
  ],
  "next_ids": {
    "todo": 2,
    "checklist_item": 2,
    "group": 2,
    "comment": 2
  }
}
```

## 6. OPSX 变更集成

```mermaid
flowchart LR
    subgraph OPSX 平台
        A[变更请求<br/>Change Request]
    end

    subgraph Todo App
        B[todo_change.h]
        C[todo_create_change]
    end

    subgraph 数据模型
        D[todo_t]
    end

    A -->|触发| B
    B --> C
    C --> D
    C -->|生成| E[变更 ID]
    C -->|生成| F[变更 URL]
    E --> G[返回给调用方]
    F --> G
```

**OPSX 变更 API：**

```c
/**
 * @brief 创建 OPSX 变更
 * @param todo_id 待办 ID
 * @param out_change_id 输出变更 ID
 * @param change_id_size out_change_id 缓冲区大小
 * @param out_url 输出变更 URL
 * @param url_size out_url 缓冲区大小
 * @return 0 成功，-1 失败
 */
int todo_create_change(int64_t todo_id, 
                       char *out_change_id, size_t change_id_size,
                       char *out_url, size_t url_size);
```

## 7. 关键代码位置

| 模块 | 文件路径 | 核心功能 |
|------|----------|----------|
| 数据模型 | `engineering/apps/todo-app/todo_model.h` | 4 个实体结构体定义 + CRUD API |
| 数据模型实现 | `engineering/apps/todo-app/todo_model.c` | JSON 序列化/反序列化 + 内存管理 |
| HTTP 处理 | `engineering/apps/todo-app/todo_handler.h` | HTTP 服务器初始化/启动/停止 |
| HTTP 处理实现 | `engineering/apps/todo-app/todo_handler.c` | 路由分发 + 请求处理 + 响应构造 |
| 数据迁移 | `engineering/apps/todo-app/todo_migration.h` | 旧版数据迁移接口 |
| 数据迁移实现 | `engineering/apps/todo-app/todo_migration.c` | 检测旧数据 + 格式转换 |
| OPSX 变更 | `engineering/apps/todo-app/todo_change.h` | 变更创建接口 |
| OPSX 变更实现 | `engineering/apps/todo-app/todo_change.c` | 变更 ID/URL 生成 |
| 服务器入口 | `engineering/apps/todo-app/main.c` | 命令行参数解析 + 启动流程 |