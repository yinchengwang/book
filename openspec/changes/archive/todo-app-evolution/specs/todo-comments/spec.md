## ADDED Requirements

### Requirement: 评论管理
系统 SHALL 支持为待办添加评论。
系统 SHALL 支持获取待办的评论列表。
系统 SHALL 支持删除评论。
每条评论包含 id、todo_id、text、created_at 字段。

#### Scenario: 添加评论
- **WHEN** 用户 POST `{"text": "这是一个评论"}` 到 `/api/todos/1/comments`
- **THEN** 系统创建评论并返回评论对象

#### Scenario: 获取评论列表
- **WHEN** 用户请求 `GET /api/todos/1/comments`
- **THEN** 系统返回该待办的全部评论列表（按 created_at 升序）

#### Scenario: 删除评论
- **WHEN** 用户 DELETE `/api/todos/1/comments/5`
- **THEN** 系统删除评论 ID 为 5 的评论

#### Scenario: 待办详情包含评论
- **WHEN** 用户请求 `GET /api/todos/1`
- **THEN** 系统返回的待办详情中包含 comments 字段