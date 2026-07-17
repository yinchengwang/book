## ADDED Requirements

### Requirement: 分组管理
系统 SHALL 支持创建、读取、更新、删除分组。
每个分组包含 id、name、color、sort_order 字段。
系统 SHALL 支持将待办关联到分组（group_id）。
系统 SHALL 支持按分组筛选待办列表。
删除分组时 SHALL 级联解除关联待办的 group_id 为 0。

#### Scenario: 创建分组
- **WHEN** 用户 POST `{"name": "工作", "color": "#4A90D9"}` 到 `/api/groups`
- **THEN** 系统创建分组并返回分组对象

#### Scenario: 获取分组列表
- **WHEN** 用户请求 `GET /api/groups`
- **THEN** 系统返回全部分组列表

#### Scenario: 更新分组
- **WHEN** 用户 PATCH 分组 ID 1 时更新 name 为 "个人"
- **THEN** 系统更新分组名称并返回

#### Scenario: 删除分组
- **WHEN** 用户 DELETE 分组 ID 1
- **THEN** 系统删除该分组，并将关联待办的 group_id 置为 0

#### Scenario: 按分组筛选
- **WHEN** 用户请求 `GET /api/todos?group_id=1`
- **THEN** 系统返回所有 group_id 为 1 的待办