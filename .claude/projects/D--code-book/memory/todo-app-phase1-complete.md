---
name: todo-app-phase1-complete
description: todo-app Phase 1 (SQLite 迁移 + 字段系统) 完成，端到端验证通过，提交 c3b3bdc8
metadata:
  type: project
---

todo-app Phase 1 SQLite 迁移 + 字段系统完成。

**已完成任务 (1.1-1.6)：**
- Task 1.1: SQLite amalgamation 引入 (`third_part/sqlite3/`)
- Task 1.2: `todo_db` 持久化层 (建表、WAL 模式、内置字段预置)
- Task 1.3: `todo_model` CRUD 全部改造为 SQLite 操作
- Task 1.4: `todo_field` 字段系统 (field_def CRUD + field_value EAV)
- Task 1.5: 字段管理 API + 路由 (`GET/POST /api/fields`, `PATCH/DELETE /api/fields/:id`, `PATCH /api/fields/:id/sort`, `PATCH /api/todos/:id/fields`)
- Task 1.6: 端到端验证通过 (`curl` 测试扩展字段创建、设值、回显全部正常)

**路由修复**：修复 `PATCH /api/todos/:id/fields` 被 `PATCH /api/todos/:id` 拦截的路由匹配问题。

**关键提交：**
- `ca1c9107` - 引入 SQLite3 amalgamation
- `939481a8` - SQLite 持久化层 + 建表 + JSON 迁移
- `80ecfc87` - todo_model 改造为 SQLite
- `c3b3bdc8` - 字段系统 todo_field + 字段管理 API

**验证命令：**
```bash
./todo-app.exe -p 8090 -d /tmp/test.db
curl -X POST http://localhost:8090/api/todos -d '{"title":"test"}'
curl -X POST http://localhost:8090/api/fields -d '{"name":"客户","type":"text"}'
curl -X PATCH http://localhost:8090/api/todos/1/fields -d '{"fields":{"10":"张三"}}'
curl http://localhost:8090/api/todos/1  # 含 fields 对象
```
