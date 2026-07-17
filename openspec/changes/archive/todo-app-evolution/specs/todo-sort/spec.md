## ADDED Requirements

### Requirement: 拖拽排序
系统 SHALL 支持通过 `PATCH /api/todos/:id/sort` 更新待办排序序号。
系统 SHALL 支持按 sort_order 排序返回待办列表。
系统 SHALL 在列表视图和看板视图中反映用户手动排序结果。

#### Scenario: 更新排序
- **WHEN** 用户 PATCH `{"sort_order": 10}` 到 `/api/todos/5/sort`
- **THEN** 系统更新待办 5 的 sort_order 为 10

#### Scenario: 按排序返回列表
- **WHEN** 用户请求 `GET /api/todos?sort=sort_order`
- **THEN** 系统按 sort_order 升序返回待办列表

#### Scenario: 默认排序
- **WHEN** 用户请求 `GET /api/todos` 未指定 sort 参数
- **THEN** 系统默认按 sort_order 排序